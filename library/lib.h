
#ifndef __LIB_H__
#define __LIB_H__

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/ports.h>
#include <dos/dos.h>

#include "amipiborg.h"
#include "amipiborg_protos.h"

#define CNN_MSG_TIMEOUT 5

struct AmiPiBorgLibrary {
    struct Library          l_Lib;
    UWORD                   l_Pad;
    BPTR                    l_SegList;
    struct Library         *l_ExecBase;
};

struct Connection {
    struct MsgPort         *c_MsgPort;
	APTR 					c_MemPool;
	UWORD            		c_Status;
	UWORD 					c_HandlerId;
	UWORD 					c_ConnId;
};

struct RequestInt {
	struct APBRequest 		r_Req;
	struct Connection 	   *r_Conn;
	UWORD					r_TimeoutTicks;
};

VOID *APB_AllocMem(APTR memPool, ULONG memSize);

VOID APB_FreeMem(APTR memPool, VOID *mem, ULONG memSize);

BOOL APB_PutMsg(struct Message *msg);

#endif

