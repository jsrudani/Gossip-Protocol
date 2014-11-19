/**********************
*
* Progam Name: MP1. Membership Protocol
* 
* Code authors: Jigar S. Rudani
*
* Current file: mp1_node.c
* About this file: Member Node Implementation
* 
***********************/

#include "mp1_node.h"
#include "emulnet.h"
#include "MPtemplate.h"
#include "log.h"
#include <stdlib.h>

/*
 *
 * Routines for introducer and current time.
 *
 */

char NULLADDR[] = {0,0,0,0,0,0};
int isnulladdr( address *addr){
    return (memcmp(addr, NULLADDR, 6)==0?1:0);
}

/* 
Return the address of the introducer member. 
*/
address getjoinaddr(void){

    address joinaddr;

    memset(&joinaddr, 0, sizeof(address));
    *(int *)(&joinaddr.addr)=1;
    *(short *)(&joinaddr.addr[4])=0;

    return joinaddr;
}

/*
 *
 * Message Processing routines.
 *
 */

/* 
Received a JOINREQ (joinrequest) message.
*/
void Process_joinreq(void *env, char *data, int size)
{
	/* Process Join Req message */
	member *node = (member *) env;
    	address *addr = (address *)data;
	messagehdr *msg;
	
	/* Increment its own heartbeat and local time for each requested member */
	node->memberCount += 1;
	node->memberlist->heartbeatcounter += 1;
	node->memberlist->localtime = getcurrtime();
	
	/* Preparing its Own MembershipList */	
	node->memberlist = realloc(node->memberlist,(node->memberCount  * sizeof(memberdata)));		
	memcpy(&node->memberlist[node->memberCount - 1].addr,addr,sizeof(address));
	
	/* Setting other members details */	
	node->memberlist[node->memberCount - 1].heartbeatcounter = 0;
	node->memberlist[node->memberCount - 1].localtime = getcurrtime();
	node->memberlist[node->memberCount - 1].suspect = 0;

	/* Announcing its Member's list arrival */
	logNodeAdd(&node->addr,&node->memberlist[node->memberCount - 1].addr);

	/* Sending its own Membership List */
	size_t msgsize = sizeof(messagehdr) + sizeof(address) + (node->memberCount  * sizeof(memberdata));
        msg=malloc(msgsize);

	/* create JOINREP message: format of data is {struct address myaddr + struct Node *membershipList} */
        msg->msgtype=JOINREP;
        memcpy((msg + 1), &node->addr, sizeof(address));
	memcpy((((char *)msg) + sizeof(msg) + sizeof(address)),node->memberlist,(node->memberCount * sizeof(memberdata)));
	
	/* send JOINREP message to requesting member. */
	MPp2psend(&node->addr, addr, (char *)msg, msgsize);
        free(msg);
	
	return;
}

/* 
Received a JOINREP (joinreply) message. 
*/
void Process_joinrep(void *env, char *data, int size)
{
	int neighbourCount = 0;
	int i = 0;

	/* Process JOINREP message */
	member *node = (member *) env;
    	address *addr = (address *)data;
	memberdata *neighbour = (memberdata *)(data + sizeof(address));

	/* Calculate count of neighbours and intialize the ingroup */
	node->ingroup = 1;
	neighbourCount = ((size - sizeof(address))/sizeof(memberdata));	
	node->memberCount = neighbourCount;

	/* Preparing its Own Membership List */
	node->memberlist = malloc(neighbourCount * sizeof(memberdata));
	memcpy(node->memberlist,neighbour,(neighbourCount * sizeof(memberdata)));
	
	for (i=0;i<node->memberCount;i++) {

		/* Announcing its Member's list arrival */
		logNodeAdd(&node->addr,&node->memberlist[i].addr);

		/* Sets the HeartBeat,LocalTime and Suspect for itself */
		if (memcmp(&node->addr,&node->memberlist[i].addr,sizeof(address)) == 0) {
			node->memberlist[i].heartbeatcounter = 1;
			node->memberlist[i].localtime = getcurrtime();
			node->memberlist[i].suspect = 0;
		}		
	}
		
        return;
}

/* Gossip Protocol Implementation */
/* Each Node which are in the group and not failed will send gossip to randomly selected memeber from its own membership list */
void Process_sendgossip(member *node) {
	
	/* Process Gossip send message */
	int i = 0;
	/* Increment my heartbeat and set local time to current time */
	for(i = 0;i < node->memberCount ; i++) {
		if (memcmp(&node->addr,&node->memberlist[i].addr,sizeof(address)) == 0) {
			node->memberlist[i].heartbeatcounter += 1;
			node->memberlist[i].localtime = getcurrtime();
		}
	}

	/* Sending its own Membership List */
	size_t msgsize = sizeof(messagehdr) + sizeof(address) + (node->memberCount  * sizeof(memberdata));
        messagehdr *msg=malloc(msgsize);

	/* create DUMMYLASTMSGTYPE message: format of data is {struct address myaddr + struct Node *membershipList} */
        msg->msgtype=DUMMYLASTMSGTYPE;
        memcpy((msg + 1), &node->addr, sizeof(address));
	memcpy((((char *)msg) + sizeof(msg) + sizeof(address)),node->memberlist,(node->memberCount * sizeof(memberdata)));
	
	int selectIndex = 0;
	selectIndex = rand() % node->memberCount;
	while (memcmp(&node->addr,&node->memberlist[selectIndex].addr,sizeof(address)) == 0) {
		selectIndex = rand() % node->memberCount;
	}

	/* send DUMMYLASTMSGTYPE message to requesting member. */
	MPp2psend(&node->addr, &node->memberlist[selectIndex].addr, (char *)msg, msgsize);
        free(msg);
	
	return;   	
}
/* Each Node which are in the group and not failed will receive gossip and merge its own membership list */
void Process_receivegossip(void *env, char *data, int size) {
	
	/* Process Gossip receive message */
	int i = 0;
	int j = 0;
	int neighbourCount = 0;
	int Tfail = 25;
	int Tcleanup = 25;
	int currTime = 0;
	int previousHeartbeat = 0;
	int flag = 0;
	
	/* Process DUMMYLASTMSGTYPE message */
	member *node = (member *) env;
    	address *addr = (address *)data;
	memberdata *neighbour = (memberdata *)(data + sizeof(address));
	memberdata *tempNeighbourList = NULL;
	
	/* Calculate count of neighbours and intialize the ingroup */
	neighbourCount = ((size - sizeof(struct address))/sizeof(struct Node));	

	tempNeighbourList = malloc(neighbourCount * sizeof(memberdata));
	memcpy(tempNeighbourList,neighbour,(neighbourCount * sizeof(memberdata)));

	/* Merge the Membership List send by Other Node */
	for ( i = 0; i < neighbourCount; i++) {
		for (j = 0; j < node->memberCount; j++) {
			previousHeartbeat = node->memberlist[j].heartbeatcounter;
			if (memcmp(&tempNeighbourList[i].addr,&node->memberlist[j].addr,sizeof(address)) == 0) {
				if (memcmp(&node->memberlist[j].addr,&node->addr,sizeof(address)) == 0)
					node->memberlist[j].heartbeatcounter += 1;
				else			
					node->memberlist[j].heartbeatcounter = max(tempNeighbourList[i].heartbeatcounter,node->memberlist[j].heartbeatcounter);
				if ((previousHeartbeat != node->memberlist[j].heartbeatcounter) && !tempNeighbourList[i].suspect)
					node->memberlist[j].localtime = getcurrtime();
			}	
			 else {
				mergeMemberList(tempNeighbourList,i,node);
			} 
		}
	}

	// Delete procedure
	currTime = getcurrtime();
	int deleteMemberIndex [node->memberCount];
	for (i = 0;i < node->memberCount; i++) {
		deleteMemberIndex[i] = 0;
	}
	flag = 0;
	for (i = 0;i < node->memberCount; i++) {
		if ((currTime - (node->memberlist[i].localtime)) > Tfail && !node->memberlist[i].suspect) {
			node->memberlist[i].suspect = 1;			
		} else if ((currTime - (node->memberlist[i].localtime)) > (Tfail + Tcleanup)) {			
			deleteMemberIndex[i] = 999;
			flag = 1;
		}
	}
	if (flag)
		deleteNode(node,deleteMemberIndex,node->memberCount);
	free(tempNeighbourList);
	return;
}

/* Return Maximum Hearbeat Counter */
int max(int tempHeartBeat,int memberCurrentHeartbeat) {
	if (tempHeartBeat > memberCurrentHeartbeat)
		return tempHeartBeat;
	else
		return memberCurrentHeartbeat;
}

/* Merge the Member List */
void mergeMemberList(memberdata *tempMember,int index,member *node) {
	
	int i = 0;
	int flag = 1;
	/* Check if Member already exist or not */
	for (i = 0; i < node->memberCount; i++) {
		if (memcmp(&node->memberlist[i].addr,&tempMember[index].addr,sizeof(address)) == 0) {
			flag = 0;
			break;
		}		
	}
	if (flag && !tempMember[index].suspect) {
		node->memberCount += 1;
		node->memberlist = realloc(node->memberlist,(node->memberCount  * sizeof(memberdata)));
		memcpy(&node->memberlist[node->memberCount - 1].addr,&tempMember[index].addr,sizeof(address));
		node->memberlist[node->memberCount - 1].heartbeatcounter = tempMember[index].heartbeatcounter;
		node->memberlist[node->memberCount - 1].localtime = getcurrtime();
		/* Announcing its Member's list arrival */
		logNodeAdd(&node->addr,&node->memberlist[node->memberCount - 1].addr); 
	}
}

/* Delete the Node */
void deleteNode(member *node,int deleteAddr[],int length) {

	int i = 0,j = 0;
	memberdata *deletenodeaddr = malloc(sizeof(memberdata));
	for (i = 0;i < length;i++) {
		if (deleteAddr[i] == 999 ) {
			memcpy(deletenodeaddr,&node->memberlist[i],sizeof(memberdata));			
			memberdata *tempMemberList = malloc((node->memberCount - 1) * sizeof(memberdata));
			for (i = 0;i < node->memberCount;i++) {
				if (memcmp(&node->memberlist[i].addr,&deletenodeaddr->addr,sizeof(address)) != 0) {
 					if (j < (node->memberCount - 1)) {
						memcpy(&tempMemberList[j],&node->memberlist[i],(sizeof(memberdata)));
						j += 1;
					}
				}
			}
			node->memberCount -= 1;
			free(node->memberlist);
			node->memberlist = malloc(node->memberCount * sizeof(memberdata));
			memcpy(node->memberlist,tempMemberList,(node->memberCount * sizeof(memberdata)));
			free(tempMemberList);
			logNodeRemove(&node->addr,&deletenodeaddr->addr);
		}
	}	
}

/* 
Array of Message handlers. 
*/
void ( ( * MsgHandler [20] ) STDCLLBKARGS )={
/* Message processing operations at the P2P layer. */
    Process_joinreq, 
    Process_joinrep,
    Process_receivegossip
};

/* 
Called from nodeloop() on each received packet dequeue()-ed from node->inmsgq. 
Parse the packet, extract information and process. 
env is member *node, data is 'messagehdr'. 
*/
int recv_callback(void *env, char *data, int size){

    member *node = (member *) env;
    messagehdr *msghdr = (messagehdr *)data;
    char *pktdata = (msghdr+1);

    if(size < sizeof(messagehdr)){
#ifdef DEBUGLOG
        LOG(&((member *)env)->addr, "Faulty packet received - ignoring");
#endif
        return -1;
    }

#ifdef DEBUGLOG
    LOG(&((member *)env)->addr, "Received msg type %d with %d B payload", msghdr->msgtype, size - sizeof(messagehdr));
#endif

    if((node->ingroup && msghdr->msgtype >= 0 && msghdr->msgtype <= DUMMYLASTMSGTYPE)
        || (!node->ingroup && msghdr->msgtype==JOINREP))            
            /* if not yet in group, accept only JOINREPs */
        MsgHandler[msghdr->msgtype](env, pktdata, size-sizeof(messagehdr));
    /* else ignore (garbled message) */
    free(data);

    return 0;

}

/*
 *
 * Initialization and cleanup routines.
 *
 */

/* 
Find out who I am, and start up. 
*/
int init_thisnode(member *thisnode, address *joinaddr){
    
    if(MPinit(&thisnode->addr, PORTNUM, (char *)joinaddr)== NULL){ /* Calls ENInit */
#ifdef DEBUGLOG
        LOG(&thisnode->addr, "MPInit failed");
#endif
        exit(1);
    }
#ifdef DEBUGLOG
    else LOG(&thisnode->addr, "MPInit succeeded. Hello.");
#endif

    thisnode->bfailed=0;
    thisnode->inited=1;
    thisnode->ingroup=0;
    /* node is up! */

    return 0;
}


/* 
Clean up this node. 
*/
int finishup_thisnode(member *node){
	
	/* Free the Membership List allocation */
	free(node->memberlist);	
	return 0;
}


/* 
 *
 * Main code for a node 
 *
 */

/* 
Introduce self to group. 
*/
int introduceselftogroup(member *node, address *joinaddr){
    
    messagehdr *msg;
#ifdef DEBUGLOG
    static char s[1024];
#endif

    if(memcmp(&node->addr, joinaddr, 4*sizeof(char)) == 0){
        /* I am the group booter (first process to join the group). Boot up the group. */
#ifdef DEBUGLOG
        LOG(&node->addr, "Starting up group...");
#endif
	// Announcing its arrival
	node->ingroup = 1;
	logNodeAdd(&node->addr,&node->addr);	
	node->memberCount = 0;
	
	/* Preparing its Own Membership List */
	node->memberlist = malloc((node->memberCount + 1) * sizeof(memberdata));
	memcpy(node->memberlist,&node->addr,sizeof(address));

	/* Setting my Heartbeat,Localtime and Suspect */
	node->memberlist[node->memberCount].heartbeatcounter = 0;
	node->memberlist[node->memberCount].localtime = getcurrtime();
	node->memberlist[node->memberCount].suspect = 0;

	node->memberCount += 1;
    }
    else{
        size_t msgsize = sizeof(messagehdr) + sizeof(address);
        msg=malloc(msgsize);

	/* create JOINREQ message: format of data is {struct address myaddr} */
        msg->msgtype=JOINREQ;
        memcpy((char *)(msg+1), &node->addr, sizeof(address));

#ifdef DEBUGLOG
        sprintf(s, "Trying to join...");
        LOG(&node->addr, s);
#endif

    	/* send JOINREQ message to introducer member. */
        MPp2psend(&node->addr, joinaddr, (char *)msg, msgsize);
        
        free(msg);
    }

    return 1;

}

/* 
Called from nodeloop(). 
*/
void checkmsgs(member *node){
    void *data;
    int size;

    /* Dequeue waiting messages from node->inmsgq and process them. */
	
    while((data = dequeue(&node->inmsgq, &size)) != NULL) {
        recv_callback((void *)node, data, size); 
    }
    return;
}


/* 
Executed periodically for each member. 
Performs necessary periodic operations. 
Called by nodeloop(). 
*/
void nodeloopops(member *node) {
	
	/* Start the Gossip HeartBeat protocol */
	if(node->ingroup && !node->bfailed) {
		Process_sendgossip(node);
        }
	return;
}

/* 
Executed periodically at each member. Called from app.c.
*/
void nodeloop(member *node){
    if (node->bfailed) return;

    checkmsgs(node);

    /* Wait until you're in the group... */
    if(!node->ingroup) return ;

    /* ...then jump in and share your responsibilites! */
    nodeloopops(node);
    
    return;
}

/* 
All initialization routines for a member. Called by app.c. 
*/
void nodestart(member *node, char *servaddrstr, short servport){

    address joinaddr=getjoinaddr();

    /* Self booting routines */
    if(init_thisnode(node, &joinaddr) == -1){

#ifdef DEBUGLOG
        LOG(&node->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if(!introduceselftogroup(node, &joinaddr)){
        finishup_thisnode(node);
#ifdef DEBUGLOG
        LOG(&node->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }

    return;
}

/* 
Enqueue a message (buff) onto the queue env. 
*/
int enqueue_wrppr(void *env, char *buff, int size){    return enqueue((queue *)env, buff, size);}

/* 
Called by a member to receive messages currently waiting for it. 
*/
int recvloop(member *node){
    if (node->bfailed) return -1;
    else return MPrecv(&(node->addr), enqueue_wrppr, NULL, 1, &node->inmsgq); 
    /* Fourth parameter specifies number of times to 'loop'. */
}

