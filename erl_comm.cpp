#include "../include/erl_comm.h"

#include <string>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../include/test_conf.h"
#include "../include/global_err_msg.h"

using std::string;

#ifdef ERL_COMM_DEBUG
#include <iostream>
#include <fstream>
using std::ofstream;
using std::endl;

static ofstream stream;
FILE * log_fd;
#endif

/* receiver definition */

/**
 * @fn	tFrame_erl_comm::tFrame_erl_comm(char * nodeName, char * parent, unsigned char *buf, int length)
 *
 * @brief	Constructor.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param [in,out]	nodeName	name of the running node.
 * @param [in,out]	parent  	name of the parent which spawend current process.
 * @param [in,out]	buf			the buffer used to hold message.
 * @param	length				maximum length of the buffer.
 *
 * ### remarks	Awang, 16/01/2014.
 */

tFrame_erl_comm::tFrame_erl_comm(char * nodeName, char * parent, unsigned char *buf, int length) {
#ifdef ERL_COMM_DEBUG
	{
		char logFile[2][128];
		sprintf(logFile[0], "../log/erl_comm_%d.log", nodeName[4]);
		sprintf(logFile[1], "../log/erl_comm_%d_c.log", nodeName[4]);
		stream.open(logFile[0], std::ios::out | std::ios::binary);
		log_fd = fopen(logFile[1], "wb+");
	}
#endif
	erl_init(NULL, 0);

	struct in_addr addr;
	char * ip = &nodeName[11];
	addr.s_addr = inet_addr(/*"172.16.0.66"*/ip);

	//string fullName = string(nodeName) + string("@172.16.0.66");

	if (erl_connect_xinit(nodeName, nodeName, /*(char *) fullName.c_str()*/nodeName, &addr, (char *) string(DEFAULT_COOKIE).c_str(), 0) == -1) {
		erl_err_quit("erl_connect_init");
	}

	_fd = erl_connect(parent);
	_buf = buf;
	_length = length;
	_recv_cir_ptr = 0;
	_recv_read_ptr = 0;

	_erl_receive_loop = true;
	_recv_ret = _send_ret = -1;
	_mt = PTHREAD_MUTEX_INITIALIZER;
	pthread_attr_init(&thread_attr);

	data_access_start();
	for (int i = 0; i < CIR_BUF_SIZE; ++i) {
		recv_cir_buf[i].type = NUM_RECV_ARG_TYPE;
	}
	data_access_end();
}

/**
 * @fn	tFrame_erl_comm::~tFrame_erl_comm()
 *
 * @brief	Destructor.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * ### remarks	Awang, 16/01/2014.
 */

tFrame_erl_comm::~tFrame_erl_comm() {
	// erlang term clean up
	if (emsg.from) {
		erl_free_term(emsg.from);
	}

	if (emsg.msg) {
		erl_free_compound(emsg.msg);
	}
	
	// pthread clean up
	pthread_mutex_destroy(&_mt);
	pthread_attr_destroy(&thread_attr);
	pthread_cancel(precv);
	pthread_cancel(psend);
}

/**
 * @fn	void * tFrame_erl_comm::staticRecvEntry(void * c)
 *
 * @brief	Wrapper function for thread spawn.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param [in,out]	c	If non-null, the pointer to current instance.
 *
 * @return	No return.
 *
 * ### remarks	Awang, 16/01/2014.
 */

void * tFrame_erl_comm::staticRecvEntry(void * c) {
	((tFrame_erl_comm *) c) ->_receive();
	return NULL;
}

/**
 * @fn	int tFrame_erl_comm::receive()
 *
 * @brief	Receive routine.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @return	return from _receive routine.
 *
 * ### remarks	Awang, 16/01/2014.
 */

int tFrame_erl_comm::receive() {
	int rc;

	rc = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED);
	if (rc) {
#ifdef ERL_COMM_DEBUG
		stream << "receive " << rc << " pthread error setdetachstate" << endl;
#endif
		return PTHREAD_ERROR;
	}

	rc = pthread_create(&precv, &thread_attr, &staticRecvEntry, this);
	if (rc) {
#ifdef ERL_COMM_DEBUG
		stream << "receive " << rc << " pthread error create" << endl;
#endif
		return PTHREAD_ERROR;
	}

#if 0
	rc = pthread_detach(precv);
	if (rc) {
#ifdef ERL_COMM_DEBUG
		stream << "receive " << rc << " pthread error detach" << endl;
#endif
		return PTHREAD_ERROR;
	}
#endif

	return _recv_ret;
}

/**
 * @fn	void tFrame_erl_comm::_receive()
 *
 * @brief	Actual receiving body.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * ### remarks	Awang, 16/01/2014.
 */

void tFrame_erl_comm::_receive() {
	int got = 0;
	
	while (1) {
		if (_erl_receive_loop) {
			got = erl_receive_msg(_fd, _buf, _length, &emsg);
			if (got == ERL_TICK) {
				/**
				 * ERL_TICK will be handled automatically by erl_interface.
				 * ignore
				 */
#ifdef ERL_COMM_DEBUG
				stream << "tick" << endl;
#endif
			} else if (got == ERL_ERROR) {
				/**
				 * receive message error
				 */
#ifdef ERL_COMM_DEBUG
				stream << "error" << endl;
#endif
			} else {
				/**
				 * work load when message received
				 */
				if (data_access_start()) {

					if (emsg.type == ERL_REG_SEND) {
#ifdef ERL_COMM_DEBUG
						stream << "message received. " << emsg.type << "-" << ERL_REG_SEND << endl;
						fprintf(log_fd, "emsg: %p buffer: %p type: %d name: %s message: ", &emsg, &recv_cir_buf[_recv_cir_ptr], emsg.type, emsg.to_name);
						erl_print_term(log_fd, emsg.msg);
						fflush(log_fd);
						fprintf(log_fd, " to: ");
						erl_print_term(log_fd, emsg.to);
						fflush(log_fd);
						fprintf(log_fd, " from: ");
						erl_print_term(log_fd, emsg.from);
						fflush(log_fd);
						fprintf(log_fd, "\n");
						fflush(log_fd);
#endif

						if (!populate_recv_arg(
#ifdef ERL_COMM_DEBUG
												log_fd,
#endif
												&emsg,
												&recv_cir_buf[_recv_cir_ptr]
												)) {
						//if (!populate) {
#ifdef ERL_COMM_DEBUG
							stream << "GENERIC ERROR detected. check recv arg population.\n" << endl;
#endif
							_recv_ret = GENERIC_ERROR;
						} else {
#ifdef ERL_COMM_DEBUG
							stream << "packets parsed. recv_cir_buf[" << _recv_cir_ptr << "]: " << recv_cir_buf[_recv_cir_ptr].type
									<< " stmp[" << _recv_cir_ptr << "]: " << (long long) recv_cir_buf[_recv_cir_ptr].ts.tv_sec << ":" << recv_cir_buf[_recv_cir_ptr].ts.tv_nsec << endl;
#endif
							_recv_cir_ptr = (_recv_cir_ptr + 1) % CIR_BUF_SIZE;
#if 0
							if (_recv_cir_ptr > _recv_read_ptr)
#ifdef ERL_COMM_DEBUG
								stream << "ERROR circular buffer over flow detected. Consider increasing buffer size." << endl;
#else
								fprintf(stderr, "ERROR circular buffer over flow detected. Consider increasing buffer size.\n");
#endif
#endif
						}
					}
					data_access_end();
				} else 
					_recv_ret = GENERIC_ERROR;
			}
		}
	}

	_recv_ret = NO_ERROR;
	pthread_exit(NULL);
}

/* sender definitions */

/**
 * @fn	void * tFrame_erl_comm::staticSendEntry(void * package)
 *
 * @brief	Wrapper funtion for thread spawn.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param [in,out]	package	pointer to a package structure which holdes the calling arguments.
 *
 * @return	null if it fails, else a void*.
 *
 * ### remarks	Awang, 16/01/2014.
 */

void * tFrame_erl_comm::staticSendEntry(void * package) {
	((tFrame_erl_comm *) ((send_t *) package)->instance)->
		_send(((send_t *) package)->type, ((send_t *) package)->size, ((send_t *) package)->args);

	return NULL;
}

/**
 * @fn	int tFrame_erl_comm::send(ETERM * msg);
 *
 * @brief	Send erlang term msg as raw copy without data manipulation.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param [in,out]	An erlang term to be sent as raw copy.
 *
 * @return	Number of bytes sent
 */

int tFrame_erl_comm::send(ETERM * msg) {
	if (erl_reg_send(_fd, (char *) string(LOCAL_MASTER_NAME).c_str(), msg) == 0) {
		return GENERIC_ERROR;
	} else return erl_size(msg);
}

/**
 * @fn	int tFrame_erl_comm::send(global_msg_t type, size_t size, erl_comm_send_arg * buf)
 *
 * @brief	Send message routine.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param	type	   	The message type.
 * @param	size	   	The message size.
 * @param [in,out]	buf	If non-null, the buffer holds the message content.
 *
 * @return	Return from _send routine.
 *
 * ### remarks	Awang, 16/01/2014.
 */

int tFrame_erl_comm::send(global_msg_t type, size_t size, erl_comm_send_arg * buf) {
	int rc;
	void * status;
	send_t package;
	package.instance = this;
	package.type = type;
	package.size = size;
	package.args = buf;

	printf("%p %d %u %p\n", &package, package.type, (unsigned) package.size, package.args);
	rc = pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	if (rc) {
#ifdef ERL_COMM_DEBUG
		stream << "send " << rc << " pthread error setdetachstate" << endl;
#endif
		return PTHREAD_ERROR;
	}

	rc = pthread_create(&psend, &thread_attr, staticSendEntry, (void *) &package);
	if (rc) {
#ifdef ERL_COMM_DEBUG
		stream << "send " << rc << " pthread error create" << endl;
#endif
		return PTHREAD_ERROR;
	}

	rc = pthread_join(psend, &status);
	if (rc) {
#ifdef ERL_COMM_DEBUG
		stream << "send " << rc << " pthread error join" << endl;
#endif
		return PTHREAD_ERROR;
	}

#ifdef ERL_COMM_DEBUG
		stream << "send finished " << (char *) status << " " << _send_ret << endl;
#endif

	return _send_ret;
};

/**
 * @fn	void tFrame_erl_comm::_send(global_msg_t type, size_t size, erl_comm_send_arg * args)
 *
 * @brief	actual send routine.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param	type			The message type.
 * @param	size			The message size.
 * @param [in,out]	args	If non-null, pointer to erl_comm_send_arg which holds send args.
 *
 * ### remarks	Awang, 16/01/2014.
 */

void tFrame_erl_comm::_send(global_msg_t type, size_t size, erl_comm_send_arg * args) {
	if (size > _length) {
		_send_ret = ARG_ERROR;
		return;
	} else if (type == INIT) {
		_send_ret = NOT_HANDLED;
		return;
	} else if (type == TIMEOUT || type == CRASH || type == FAULT || type == TRACE) {
		_send_ret = SELF_CONTAINED;
		return;
	}

	ETERM * message[2];
	message[0] = erl_mk_int(type);
	message[1] = populate_send_arg(
#ifdef ERL_COMM_DEBUG
					log_fd,
#endif
					args
					);
	ETERM * resp = erl_mk_tuple(message, 2);

#ifdef ERL_COMM_DEBUG
	erl_print_term(log_fd, message[0]);
	fprintf(log_fd, "\t");
	erl_print_term(log_fd, message[1]);
	fprintf(log_fd, "\n");
	erl_print_term(log_fd, resp);
	fprintf(log_fd, "\n\n");
	fflush(log_fd);
#endif
	if (erl_reg_send(_fd, (char *) string(LOCAL_MASTER_NAME).c_str(), resp) == 0) {
		_send_ret = GENERIC_ERROR;
	}

	// we've encoded $size bytes and 1 extra 32 bit integer for global type. total is size + 4
	_send_ret = erl_size(resp);

	erl_free_term(message[0]);
	erl_free_compound(message[1]);
	erl_free_term(resp);
	pthread_exit(NULL);
}

void tFrame_erl_comm::toggel_receive(bool en) {
	_erl_receive_loop = en;
}

bool tFrame_erl_comm::data_access_start() {
	int ret = 0;
	do {
		ret = pthread_mutex_trylock(&_mt);
	} while (ret == EINVAL);

	return (ret == 0) ? true : false;
}

void tFrame_erl_comm::data_access_end(void) {
	pthread_mutex_unlock(&_mt);
}

/**
 * @fn	void tFrame_erl_comm::get_recv_buf(erl_comm_recv_arg * buf)
 *
 * @brief	Exposes receive buffer.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param [in,out]	buf	If non-null, pointer of buffer the exposed content copies to.
 *
 * ### remarks	Awang, 16/01/2014.
 */

int tFrame_erl_comm::get_recv_buf(erl_comm_recv_arg * buf) {
	if (!parse_recv_buf(buf, &recv_cir_buf[_recv_read_ptr])) {
		return -1;
	} else {
#ifdef ERL_COMM_DEBUG
		stream << "buffer read: stmp[" << _recv_read_ptr << "] = " << buf->ts.tv_sec << ":" << buf->ts.tv_nsec << endl;
#endif
	}

	_recv_read_ptr = (_recv_read_ptr + 1) % CIR_BUF_SIZE;

	if (_recv_read_ptr == _recv_cir_ptr && recv_cir_buf[_recv_cir_ptr].type != NUM_RECV_ARG_TYPE && data_access_start()) {
#ifdef ERL_COMM_DEBUG
		printf("buffer underflow\n");
#endif
		for (int i = 0; i < CIR_BUF_SIZE; ++i) {
			recv_cir_buf[i].type = NUM_RECV_ARG_TYPE;
		}

		//_recv_read_ptr = (_recv_read_ptr + 15) % CIR_BUF_SIZE;
		data_access_end();
	}

	return _recv_read_ptr;
}
