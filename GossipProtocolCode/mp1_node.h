/**********************
*
* Progam Name: MP1. Membership Protocol.
* 
* Code authors: Jigar S. Rudani
*
* Current file: mp2_node.h
* About this file: Header file.
* 
***********************/

#ifndef _NODE_H_
#define _NODE_H_

#include "stdincludes.h"
#include "params.h"
#include "queue.h"
#include "requests.h"
#include "emulnet.h"

/* Configuration Parameters */
char JOINADDR[30];                    /* address for introduction into the group. */
extern char *DEF_SERVADDR;            /* server address. */
extern short PORTNUM;                /* standard portnum of server to contact. */

/* Miscellaneous Parameters */
extern char *STDSTRING;

typedef struct member{            
        struct address addr;            // my address
		struct Node *memberlist;		// Membership List
        int inited;                     // boolean indicating if this member is up
        int ingroup;                    // boolean indicating if this member is in the group
        queue inmsgq;                   // queue for incoming messages
        int bfailed;                    // boolean indicating if this member has failed
	int memberCount;		// Number of Member's in Membership List
} member;

/* Message types */
/* Meaning of different message types
  JOINREQ - request to join the group
  JOINREP - replyto JOINREQ
*/
enum Msgtypes{
		JOINREQ,			
		JOINREP,
		DUMMYLASTMSGTYPE
};

/* Generic message template. */
typedef struct messagehdr{ 	
	enum Msgtypes msgtype;
} messagehdr;

/* Functions in mp2_node.c */

/* Message processing routines. */
STDCLLBKRET Process_joinreq STDCLLBKARGS;
STDCLLBKRET Process_joinrep STDCLLBKARGS;
STDCLLBKRET Process_receivegossip STDCLLBKARGS;

/*
int recv_callback(void *env, char *data, int size);
int init_thisnode(member *thisnode, address *joinaddr);
*/

/* Member's Data Structure
*/
typedef struct Node {
	address addr;		// Address of the node
	int heartbeatcounter;	// Heartbeat counter updated by node itself
	int localtime;		// Corresponds to currenttime at the particular node
	int suspect;		// Indicates Timeout (Tfail) occurs and mark this Node as Suspect Node
}memberdata;

/*
Other routines.
*/

void nodestart(member *node, char *servaddrstr, short servport);
void nodeloop(member *node);
int recvloop(member *node);
int finishup_thisnode(member *node);

#endif /* _NODE_H_ */

