
#ifndef __APB_CLIENT_H__
#define __APB_CLIENT_H__

#include <exec/list.h>
#include <exec/ports.h>

#include <clib/exec_protos.h>

#include "buffer.h"

typedef enum {
	APB_M_CONNECT,
	APB_M_DISCONNECT,
	APB_M_CHECK,
	APB_M_READ,
	APB_M_WRITE,
	APB_M_ABORT
} MessageType;


typedef enum 
{
    CS_CONNECTING,
    CS_CONNECTED,
    CS_DISCONNECTING,
    CS_DISCONNECTED,
} ConnectionState;


struct Request
{
    struct Message r_Msg;
	UWORD r_Error;
	UWORD r_Actual;
	UWORD r_Length;
	APTR r_Data;    
};

struct ConnRequest {
    struct Request          cr_Req;
    UWORD                   cr_MsgType;
    UWORD                   cr_HandlerId;
    UWORD                   cr_Status;
};


struct Connection 
{
    struct MinNode  c_Node;
    UWORD           c_State;
    UWORD           c_HandlerId;
    UWORD           c_Id;
    struct MinList  c_ReadQueue;
    struct MsgPort  c_MsgPort;
    struct MinList  c_RequestQueue;
};

VOID APB_SendToClient(struct Connection *cnn, struct BufferRef *msg);

VOID APB_HandleReadRequest(struct Connection *cnn, struct Request *req);

#endif // __APB_CLIENT_H__

