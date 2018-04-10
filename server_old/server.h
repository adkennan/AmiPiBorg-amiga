
#ifndef __APB_SERVER_H__
#define __APB_SERVER_H__

#include "amipiborg.h"

#include <exec/list.h>
#include <exec/ports.h>

#include <clib/exec_protos.h>


#define TO_NODE(o) ((struct Node *)o);

#define ALLOC_STRUCT(T) ((struct T *)APB_AllocMem(sizeof(struct T), MEMF_ANY))


struct Server
{
	UWORD 			   s_RemoteVersion;
	struct List 	   s_Handlers;
    struct MinList     s_Connections;
    UWORD              s_NextConnId;
    UWORD              s_NextPktId;
    struct MsgPort    *s_ClientPort;
    struct BufferPool  s_BufferPool;
};

struct HandlerNode 
{
	struct Node 	hn_Node;
	UWORD			hn_Id;
};


// Memory Management

VOID *APB_AllocMem(ULONG byteSize, ULONG attributes); 

VOID APB_FreeMem(VOID *mem, ULONG byteSize);

/*
// Message Handling

VOID APB_HandleHello(struct Server *srv, struct Packet *pkt);

VOID APB_HandleGoodbye(struct Server *srv, struct Packet *pkt);

VOID APB_HandleConnected(struct Server *srv, struct Packet *pkt);

VOID APB_HandleDisconnected(struct Server *srv, struct Packet *pkt);
*/

VOID APB_RunEventQueue(struct Server *srv);

#endif // __ABP_SERVER_H__

