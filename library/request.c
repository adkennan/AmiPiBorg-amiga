
#include "lib.h"

#include <clib/exec_protos.h>

struct APBRequest * __asm __saveds APB_AllocRequest(
	register __a0 APTR connection)
{
    struct Connection *conn = (struct Connection *)connection;
    struct RequestInt *req;

    if( req = (struct RequestInt *)APB_AllocMem(conn->c_MemPool, sizeof(struct RequestInt) ) ) {

        req->r_Req.r_Msg.mn_ReplyPort = conn->c_MsgPort;
        req->r_Req.r_Msg.mn_Length = sizeof(struct RequestInt);
        req->r_Req.r_ConnId = conn->c_ConnId;
        req->r_Conn = conn;
    }

    return (struct APBRequest *)req;
}

VOID __asm __saveds APB_FreeRequest(
	register __a0 struct APBRequest *request)
{
    struct RequestInt *ri = (struct RequestInt *)request;

    APB_FreeMem(ri->r_Conn->c_MemPool, ri, sizeof(struct RequestInt));
}

