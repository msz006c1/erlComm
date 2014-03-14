/**
 * This file contains the C/C++ header definition for global message type.
 * Global message type should be encoded as the first byte of any message in system.
 * This type will signal framework node behaivor.
 */

#ifndef GLB_MSG_TYPE_H
#define GLB_MSG_TYPE_H

/*
 *	magic pattern for trace message tailing. 
 *	using 0xaa 0xaa 0xaa for 24 bytes. 
 *	last 8 byte for special character \.
 */
#define MAGIC_PATTERN 0xAAAAAA5C // magic pattern for trace message parsing. 0x5c is \.

typedef enum {
	RAW,      // message is to be sent as raw. no packaging is required.
	ACK,      // message is ack of incoming command
	RESPONSE, // message is a response to node command
	PASS,     // message is to be passed through node without capturing
	COPY,     // message is to be passed through node with capturing
	CRASH,    // other end is suppose to capture message, pass it along, wait for ack and then kill the other C/C++ node
	FAULT,    // message signals fault detection. connected node should capture and log. all running keep going as normal
	TRACE,    // message used for path tracing. each node on the path should make a copy and append node info at message end
	TIMEOUT,  // message used to signal timeout
	INIT,     // message is for any init trigger from node to external C/C++ program

	NUM_GLB_MSG_TYPE_TOTAL
} global_msg_t;

#endif
