/**********************
*
* Progam Name: MP1. Membership Protocol
* 
* Code authors: <your name here>
*
* Current file: mp1_node.c
* About this file: Member Node Implementation
* 
***********************/

#include "mp1_node.h"
#include "emulnet.h"
#include "MPtemplate.h"
#include "log.h"


/*
 *
 * Routines for introducer and current time.
 *
 */

char NULLADDR[] = {0,0,0,0,0,0};
memberdata *memberlist = NULL;		//Global list initialse to NULL
static int countofMembers = 0;		// Global count
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
	/* <your code goes in here> */
	memberdata neighbourlist[countofMembers];
#ifdef DEBUGLOG
    	//static char s[1024];
#endif
	member *node = (member *) env;
    	address *addr = (address *)data;
	//LOG(&node->addr,"Inside process joinreq");
	//LOG(addr,"Src Address");
	messagehdr *msg;
	size_t msgsize = sizeof(messagehdr) + sizeof(address);// + sizeof(memberdata);	//Append memberdata
        msg=malloc(msgsize);
	
	/* populate the field of memberdata. Only address field is updated
	memberlist = (struct memberdata *)realloc(memberlist,(countofMembers + 1) * sizeof(memberdata));
	if (memberlist != NULL) {
		memcpy(memberlist[countofMembers]->addr,addr,sizeof(address));
	}
	countofMembers += 1;	//Increment the counter
	printf("The value of countofMembers %d: \n",countofMembers);
	memcpy(neighbourlist,memberlist,sizeof(memberdata));
	
	/* create JOINREP message */
        msg->msgtype=JOINREP;
        memcpy((char *)(msg + 1), &node->addr, sizeof(address));
	//memcpy((char *)(msg + sizeof(address)), neighbourlist,  sizeof(memberlist));

#ifdef DEBUGLOG
        //sprintf(s, "Accepted the JOINREQ");
        //LOG(&node->addr, s);
#endif
	printf("Accepted the JOINREQ %d\n",node->addr);
    	/* send JOINREQ message to introducer member. */
	
        MPp2psend(&node->addr, addr, (char *)msg, msgsize);
        
        free(msg);
	/* <your code goes in here> */
	return;
}

/* 
Received a JOINREP (joinreply) message. 
*/
void Process_joinrep(void *env, char *data, int size)
{
	/* <your code goes in here> */
	member *node = (member *) env;
    	address *addr = (address *)data;
	//LOG(&node->addr,"Inside process joinrep");
	logNodeAdd(addr,&node->addr);	//Dissimenation
	/* Setting the parameter like ingroup */
	node->ingroup = 1;
	printf("Ingroup: %d\n",node->ingroup);
	memberdata *memberlist = (memberdata *)data;
	printf("Node %d receiving Reply msg\n",node->addr);
	printf("The neighbour details: %d%d%d\n",memberlist->addr,memberlist->heartbeatcounter,memberlist->localtime);
	//Preparing its own membership list
	
	/* <your code goes in here> */
	return;
}


/* 
Array of Message handlers. 
*/
void ( ( * MsgHandler [20] ) STDCLLBKARGS )={
/* Message processing operations at the P2P layer. */
    Process_joinreq, 
    Process_joinrep
};

/* 
Called from nodeloop() on each received packet dequeue()-ed from node->inmsgq. 
Parse the packet, extract information and process. 
env is member *node, data is 'messagehdr'. 
*/
int recv_callback(void *env, char *data, int size){

    member *node = (member *) env;
    messagehdr *msghdr = (messagehdr *)data;
    char *pktdata = (char *)(msghdr+1);

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

	/* <your code goes in here> */
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
	logNodeAdd(&node->addr,&node->addr);	//Dissimenation
	// populate the field of memberdata. Only address field is updated
	memberdata ownMemberList[(countofMembers + 1)];
	memberlist = ownMemberList;
	memcpy(&memberlist->addr,&node->addr,sizeof(address));
	printf("ownMemberList[countofMembers]: %d\n",memberlist->addr);
	//memberlist = (struct memberdata *)malloc((countofMembers + 1) * sizeof(memberdata));
	//if (memberlist != NULL) {
		/* allocate memory for one `struct memberdata` */
	//	memberlist[countofMembers] = malloc(sizeof(memberdata));
	//	if (&memberlist[countofMembers] != NULL)
   	//		memcpy(memberlist[countofMembers]->addr,&node->addr,sizeof(address));
	//	else
	//		printf ("The memberdata is null..!!!");
	//} else {
	//	printf ("The memberlist is null..!!!");
	//}
        node->ingroup = 1;
	countofMembers += 1;
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
    /* Jigar */
#ifdef DEBUGLOG
    //static char s[1024];
#endif
#ifdef DEBUGLOG
        //sprintf(s, "Inside checkmsgs Node Address");
        //LOG(&node->addr, s);
#endif
    /* Jigar */
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
void nodeloopops(member *node){

	/* <your code goes in here> */
	//printf("Inside nodeloopops %d\n",node->addr);
		
	/* <your code goes in here> */

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

