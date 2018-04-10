
#ifndef __APB_REMOTE_H__
#define __APB_REMOTE_H__

#include "buffer.h"
#include "server.h"

#include <exec/io.h>

struct Remote {
    struct MsgPort    *r_ReadPort;
    struct MsgPort    *r_WritePort;
    struct IORequest  *r_ReadRequest;
    struct IORequest  *r_WriteRequest;
    struct MinList    *r_ReqQueue;
    BOOL               r_ReadReady;
    BOOL               r_WriteReady;    
};

BOOL APB_OpenDevice(struct Remote *r);

VOID APB_CloseDevice(struct Remote *r);

VOID APB_SendToRemote(struct Remote *r, struct Request *req);

VOID APB_WriteComplete(struct Remote *r);

VOID APB_ReadComplete(struct Server *srv, struct Remote *r);

#endif // _APB_REMOTE_H__