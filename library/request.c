
#include "lib.h"

#include <clib/exec_protos.h>

struct APBRequest *__asm __saveds APB_AllocRequest(
    register __a0 APTR connection)
{
    struct Connection *conn = (struct Connection *) connection;
    struct RequestInt *req;

    if(req = (struct RequestInt *) APB_AllocObjectInternal(conn->c_Ctx->ctx_ObjPool, OT_REQUEST)) {

        req->r_Req.r_Msg.mn_ReplyPort = conn->c_MsgPort;
        req->r_Req.r_Msg.mn_Length = sizeof(struct RequestInt);
        req->r_Req.r_ConnId = conn->c_ConnId;
        req->r_Conn = conn;

        LOGI1(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Create Request %x", req);
    } else {
        LOGI0(conn->c_Ctx->ctx_Logger, LOG_ERROR, "Failed to create Request");
    }


    return (struct APBRequest *) req;
}

VOID __asm __saveds APB_FreeRequest(
    register __a0 struct APBRequest *request)
{
    struct RequestInt *ri = (struct RequestInt *) request;

    LOGI1(ri->r_Conn->c_Ctx->ctx_Logger, LOG_TRACE, "Free Request %x", ri);

    APB_FreeObjectInternal(ri->r_Conn->c_Ctx->ctx_ObjPool, OT_REQUEST, ri);
}
