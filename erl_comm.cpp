#include "../include/erl_comm.h"

#include <string>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../include/test_conf.h"

using std::string;

#ifdef ERL_COMM_DEBUG
#include <iostream>
#include <fstream>
using std::ofstream;
using std::endl;

static ofstream stream;
#endif

/**
 * receiver definitions
 */
tFrame_erl_comm::tFrame_erl_comm(char * nodeName, char * parent, unsigned char *buf, int length) {
#ifdef ERL_COMM_DEBUG
	stream.open("../log/erl_comm.log", std::ios::out | std::ios::binary);
#endif
	erl_init(NULL, 0);

	struct in_addr addr;
	addr.s_addr = inet_addr("172.16.0.65");

	string fullName = string(nodeName) + string("@172.16.0.65");

	if (erl_connect_xinit(nodeName, nodeName, (char *) fullName.c_str(), &addr, DEFAULT_COOKIE, 0) == -1) {
	//if (erl_connect_init(1, "evertz",0) == -1) {
		erl_err_quit("erl_connect_init");
	}

	_fd = erl_connect(parent);
	_buf = buf;
	_length = length;

	_erl_receive_loop = true;
	_recv_ret = _send_ret = -1;
	_mt = PTHREAD_MUTEX_INITIALIZER;
	pthread_attr_init(&thread_attr);
}

tFrame_erl_comm::~tFrame_erl_comm() {
	// erlang term clean up
	erl_free_term(emsg.from);
	erl_free_term(emsg.msg);
	
	// pthread clean up
	pthread_mutex_destroy(&_mt);
	pthread_attr_destroy(&thread_attr);
	pthread_cancel(precv);
	pthread_cancel(psend);

	pthread_exit(NULL);
}

void * tFrame_erl_comm::staticRecvEntry(void * c) {
	((tFrame_erl_comm *) c) ->_receive();
	return NULL;
}

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

void tFrame_erl_comm::_receive() {
	int got = 0;
	int cnt = 0;
	
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

			//		if (emsg.type == ERL_REG_SEND) {
#ifdef ERL_COMM_DEBUG
						stream << "receive: " << ++cnt << endl;
#endif
						//_from = erl_element(2, emsg.msg);
			//		}

					data_access_end();
				} else 
					_recv_ret = GENERIC_ERROR;
			}
		}
	}

	_recv_ret = NO_ERROR;
	pthread_exit(NULL);
}

void * tFrame_erl_comm::staticSendEntry(void * package) {
	((tFrame_erl_comm *) ((send_t *) package)->instance)->
		_send(((send_t *) package)->type, ((send_t *) package)->size);

	return NULL;
}

int tFrame_erl_comm::send(global_msg_t type, size_t size) {
	int rc;
	void * status;
	send_t package;
	package.instance = this;
	package.type = type;
	package.size = size;

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

	return _send_ret;
};

void tFrame_erl_comm::_send(global_msg_t type, size_t size) {
	if (size > _length)
		_send_ret = ARG_ERROR;
	else if (type == INIT)
		_send_ret = NOT_HANDLED;
	else if (type == TIMEOUT || type == CRASH || type == FAULT || type == TRACE)
		_send_ret = SELF_CONTAINED;

	ETERM * message[2];

	if (data_access_start()) {
		message[1] = erl_mk_binary((const char *) _buf, size);
		data_access_end();
	} else {
		_send_ret = GENERIC_ERROR;
	}

	message[0] = erl_mk_int((int) type);

	// _send_ret = as {int, binary}
	ETERM * resp = erl_mk_tuple(message, 2);

	if (erl_reg_send(_fd, (char *) string("unit_test").c_str(), resp) == 0) {
		_send_ret = GENERIC_ERROR;
	}

	// we've encoded $size bytes and 1 extra 32 bit integer for global type. total is size + 4
	_send_ret = size + 4;

	erl_free_term(message[0]);
	erl_free_term(message[1]);
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
