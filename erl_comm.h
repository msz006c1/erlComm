#ifndef ERL_COMM_H
#define ERL_COMM_H

#include <erl_interface.h>
#include <ei.h>
#include <pthread.h>

#include "global_msg_type.h"
#include "msg_type.h"

class tFrame_erl_comm {
public:
	tFrame_erl_comm(char *, char *, unsigned char *, int);
	~tFrame_erl_comm();

	/**
	 * @brief receive incoming message from fd. this shall be spawned as a separate thread
	 * @output
	 *      if success, return parsed byte size
	 *      if not success, return error number. see header definition for error detail
	 */
	int receive();
	static void * staticRecvEntry(void * c);

	/**
	 * @brief send givem message type with content living in buf
	 * @output
	 *      if success, return written byte size
	 *      if not success, return error number. see header definition for error detail
	 */
	int send(global_msg_t, size_t);
	static void * staticSendEntry(void * c);

	/**
	 * @brief toggel _erl_receive_loop.
	 * @arg bool en - toggel the controller flag to en.
	 */
	void toggel_receive(bool);

	/**
	 * @brief signal data retrieval start and finish
	 */
	inline bool data_access_start(void);
	inline void data_access_end(void);

protected:

	void _receive();
	void _send(global_msg_t, size_t);

private:
	typedef struct send_s {
		void * instance;
		global_msg_t type;
		size_t size;
	} send_t;

	unsigned int _length;
	int _fd, _recv_ret, _send_ret;
	unsigned char * _buf;
	bool _erl_receive_loop;
	char * _parent;
	ErlMessage emsg;
	//ETERM * _from;
	pthread_mutex_t _mt;
	pthread_t precv;
	pthread_t psend;
	pthread_attr_t thread_attr;
};
#endif
