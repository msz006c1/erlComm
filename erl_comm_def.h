#ifndef ERL_COMM_DEF_H
#define ERL_COMM_DEF_H

#include <erl_interface.h>
#include <ei.h>
#include <string>
#include <time.h>

/**
 * By default, do not use IMPORT_DEF for such argument definition passing
 */

#if IMPORT_DEF /* following lines are for importing definition */
#include "import.h"

#define erl_comm_recv_arg import_recv_t
#define RECV_ARG_CNT import_arg_cnt

#define populate_recv_arg import_recv_pop
#else /* following lines are for definition within current header */
#define RECV_ARG_CNT 2
typedef enum recv_arg_type_e {
	UPDATE = 0x10,
	KILL = 0x11,

	NUM_RECV_ARG_TYPE
} recv_arg_type;

typedef struct update_s {
	int update_node;
} update_t;

typedef struct stop_s{
	ETERM * kill_Pid;
} stop_t;

typedef struct erl_comm_recv_arg_s {
	recv_arg_type type;
	union msg_val_u {
		update_t updateMsg;
		stop_t stopMsg;
	} msg_val;
	struct timespec ts;
	bool read_ready;
} erl_comm_recv_arg;

typedef struct erl_comm_send_arg_s {
	char * cmd;
	char * node;
	int cnt;
	struct timespec * stamp;
} erl_comm_send_arg;

/**
 * @brief	Populate receive argument.
 *
 * ### remarks	Awang, 16/01/2014.
 * ### param [in,out]	def_log	A FILE * to log stream. Only available with ERL_COMM_DEBUG flag.
 * ### param [in,out]	msg	   	the received message.
 * ### param [in,out]	buf	   	the buffer destination to hold parsed data.
 * ### return	true if it succeeds, false if it fails.
 */

inline bool populate_recv_arg(
#ifdef ERL_COMM_DEBUG
	FILE * def_log,
#endif
	ErlMessage * msg,
	erl_comm_recv_arg * buf) {
	static ETERM * const update_pattern = erl_format((char *) std::string("{update, _}").c_str());
	static ETERM * const stop_pattern = erl_format((char *) std::string("{_, stop}").c_str());

	if (msg != NULL && !buf->read_ready) {
		// we only process none-empty message

		// process based on matched pattern
		if (erl_match(update_pattern, msg->msg)) {

			buf->type = UPDATE;
			buf->msg_val.updateMsg.update_node = ERL_INT_VALUE(erl_element(2, msg->msg));
			//buf->msg_val.updateMsg.update_term = ERL_ATOM_PTR(arg[0]);
			clock_gettime(CLOCK_REALTIME, &(buf->ts));
#ifdef ERL_COMM_DEBUG
			fprintf(def_log, "update message received. type %d-%d node %d-%d\n", UPDATE, buf->type, ERL_INT_VALUE(erl_element(2, msg->msg)), buf->msg_val.updateMsg.update_node);
			fprintf(def_log, "timestamp %llu.%llu\n", (long long unsigned) buf->ts.tv_sec, buf->ts.tv_nsec);
			fflush(def_log);
#endif
			
			buf->read_ready = true;
			return true;
		} else if (erl_match(stop_pattern, msg->msg)) {

			buf->type = KILL;
			buf->msg_val.stopMsg.kill_Pid = erl_element(1, msg->msg);
			//buf->msg_val.updateMsg.update_term = ERL_ATOM_PTR(arg[1]);
			clock_gettime(CLOCK_REALTIME, &(buf->ts));
#ifdef ERL_COMM_DEBUG
			fprintf(def_log, "kill message received. type %d-%d pid ", KILL, buf->type);
			erl_print_term(def_log, erl_element(1, msg->msg));
			fprintf(def_log, "-");
			erl_print_term(def_log, buf->msg_val.stopMsg.kill_Pid);
			fprintf(def_log, "\n");
			fflush(def_log);
#endif
			
			buf->read_ready = true;
			return true;
		} else {
			// if all above pattern match failed, we do not understand the pattern.
#ifdef ERL_COMM_DEBUG
			fprintf(def_log, "unkown pattern received\n");
			fflush(def_log);
#endif
		}
	} else {
#ifdef ERL_COMM_DEBUG
		fprintf(def_log, "ERROR empty msg\n");
		fflush(def_log);
#endif
	}
	
	return false;
}

/**
 * @fn	inline void timespec_to_erltime(long int& sec, long int& usec, long int& erl_megsec)
 *
 * @brief	Timespec to erltime.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param [in,out]	sec		  	The second.
 * @param [in,out]	usec	  	The nano second.
 * @param [in,out]	erl_megsec	The megasecond representation.
 *
 * ### remarks	Awang, 16/01/2014.
 */

inline void timespec_to_erltime(long int& sec, long int& usec, long int& erl_megsec) {
	erl_megsec = sec / 1000000; // ErLang megasecond is simple 1 million seconds
	sec = sec % 1000000;        // ErLang second is part less than 1 mill seconds
	usec = usec;                // ErLang usec is the same as timespec usec
}

/**
 * @brief	Populate send argument.
 *
 * ### remarks	Awang, 16/01/2014.
 * ### param [in,out]	fd 	A FILE * to log stream. Only available with ERL_COMM_DEF flag.
 * ### param [in,out]	buf	the buffer that holds send arguments which is to be sent.
 * ### return	null if it fails, else an erlang term represents message to be sent.
 */

inline ETERM * populate_send_arg(
#ifdef ERL_COMM_DEBUG
	FILE * fd,
#endif
	erl_comm_send_arg * buf)
{
	ETERM * msg = NULL;
	if (buf != NULL) {
		ETERM * finalMsg[2];
		ETERM * payLoad[3];

		payLoad[0] = erl_mk_atom(buf->node);
		payLoad[1] = erl_mk_int(buf->cnt);

		long int erl_megsec, sec, usec;
		sec = buf->stamp->tv_sec;
		usec = buf->stamp->tv_nsec;
		timespec_to_erltime(sec, usec, erl_megsec);

		ETERM * ts[3] = {erl_mk_int(erl_megsec), erl_mk_int(sec), erl_mk_int(usec)};
		payLoad[2] = erl_mk_tuple(ts, 3);

		finalMsg[0] = erl_mk_atom(buf->cmd);
		finalMsg[1] = erl_mk_tuple(payLoad, 3);

		msg = erl_mk_tuple(finalMsg, 2);

		return msg;
	}
	return msg;
}

/**
 * @fn	inline bool parse_recv_buf(erl_comm_recv_arg * dst, erl_comm_recv_arg * src)
 *
 * @brief	Parse receive buffer.
 *
 * @author	Awang
 * @date	16/01/2014
 *
 * @param [in,out]	dst	destination structure.
 * @param [in,out]	src	source structure.
 *
 * @return	true if it succeeds, false if it fails.
 *
 * ### remarks	Awang, 16/01/2014.
 */

inline bool parse_recv_buf(erl_comm_recv_arg * dst, erl_comm_recv_arg * src) {
	if (src->read_ready) {
		src->read_ready = false;
		dst->type = src->type;
		dst->ts = src->ts;
		switch(dst->type) {
		case UPDATE:
			dst->msg_val.updateMsg.update_node = src->msg_val.updateMsg.update_node;
			src->type = NUM_RECV_ARG_TYPE;

			break;
		case KILL:
			dst->msg_val.stopMsg.kill_Pid = src->msg_val.stopMsg.kill_Pid;
			src->type = NUM_RECV_ARG_TYPE;

			break;
		case NUM_RECV_ARG_TYPE:
		default:
			return false;
		}
	} else {
		return false;
	}

	return true;
}

#endif
#endif // ERL_COMM_DEF_H
