
#include "lib.h"

#include <clib/exec_protos.h>

#include <string.h>

BOOL APB_PutMsg(
    struct ApbContext *ctx,
    struct Message *msg)
{
    struct MsgPort *serverPort;

    LOGI1(ctx->ctx_Logger, LOG_TRACE, "Send message %x", msg);

    Forbid();

    if(serverPort = FindPort(PORT_NAME)) {
        PutMsg(serverPort, msg);
    } else {
        LOGI1(ctx->ctx_Logger, LOG_ERROR, "Port %s not found", PORT_NAME);
    }

    Permit();

    return (BOOL) (serverPort != NULL);
}

APTR __asm __saveds APB_AllocConnection(
    register __a0 APTR ctx,
    register __a1 struct MsgPort * port,
    register __d0 UWORD handlerId)
{
    struct ApbContext *c = (struct ApbContext *) ctx;
    struct Connection *conn;

    if(!APB_TypeRegisteredInternal(c->ctx_ObjPool, OT_CONNECTION)) {
        APB_RegisterObjectTypeInternal(c->ctx_ObjPool, OT_CONNECTION, "Connection", sizeof(struct Connection), 1, 8);
    }

    if(!APB_TypeRegisteredInternal(c->ctx_ObjPool, OT_REQUEST)) {
        APB_RegisterObjectTypeInternal(c->ctx_ObjPool, OT_REQUEST, "Request", sizeof(struct RequestInt), 1, 64);
    }

    if(conn = (struct Connection *) APB_AllocObjectInternal(c->ctx_ObjPool, OT_CONNECTION)) {

        conn->c_Ctx = c;
        conn->c_HandlerId = handlerId;
        conn->c_MsgPort = port;
        conn->c_Status = APB_RS_NO_CONNECTION;
        conn->c_ConnId = 0;

        LOGI1(c->ctx_Logger, LOG_TRACE, "Create Connection %x", conn);
    } else {
        LOGI0(c->ctx_Logger, LOG_ERROR, "Failed to create Connection");
    }

    return conn;
}

VOID __asm __saveds APB_FreeConnection(
    register __a0 APTR connection)
{
    struct Connection *conn = (struct Connection *) connection;

    LOGI1(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Free Connection %x", conn);

    APB_FreeObjectInternal(conn->c_Ctx->ctx_ObjPool, OT_CONNECTION, conn);
}

BOOL __asm __saveds APB_OpenConnection(
    register __a0 APTR connection)
{
    struct Connection *conn = (struct Connection *) connection;
    struct APBRequest *req;
    ULONG     sig;

    LOGI1(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Open Connection %x", conn);

    if(req = APB_AllocRequest(conn)) {

        req->r_Type = APB_RT_OPEN;
        req->r_HandlerId = conn->c_HandlerId;
        req->r_Msg.mn_ReplyPort = conn->c_MsgPort;
        req->r_Timeout = CNN_MSG_TIMEOUT;

        if(APB_PutMsg(conn->c_Ctx, (struct Message *) req)) {

            sig = Wait(1 << req->r_Msg.mn_ReplyPort->mp_SigBit | SIGBREAKF_CTRL_C);

            if(sig & (1 << req->r_Msg.mn_ReplyPort->mp_SigBit)) {

                GetMsg(req->r_Msg.mn_ReplyPort);

                conn->c_Status = req->r_State;
                conn->c_ConnId = req->r_ConnId;

                LOGI2(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Connection Id: %d, State %d", conn->c_ConnId, conn->c_Status);

            } else {

                LOGI0(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Open attempt aborted");

                conn->c_Status = APB_RS_ABORTED;
            }
        } else {

            LOGI0(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Coult not contact server");

            conn->c_Status = APB_RS_NO_SERVER;
        }

        APB_FreeRequest(req);
    }

    return (BOOL) (conn->c_Status == APB_RS_OK);
}

VOID __asm __saveds APB_CloseConnection(
    register __a0 APTR connection)
{
    struct Connection *conn = (struct Connection *) connection;
    struct APBRequest *req;
    ULONG     sig;

    LOGI1(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Free Connection %x", conn);

    if(req = APB_AllocRequest(conn)) {

        req->r_Type = APB_RT_CLOSE;
        req->r_HandlerId = conn->c_HandlerId;
        req->r_ConnId = conn->c_ConnId;
        req->r_Msg.mn_ReplyPort = conn->c_MsgPort;
        req->r_Timeout = CNN_MSG_TIMEOUT;

        if(APB_PutMsg(conn->c_Ctx, (struct Message *) req)) {

            sig = Wait(1 << req->r_Msg.mn_ReplyPort->mp_SigBit | SIGBREAKF_CTRL_C);

            if(sig & (1 << req->r_Msg.mn_ReplyPort->mp_SigBit)) {

                GetMsg(req->r_Msg.mn_ReplyPort);

                conn->c_Status = req->r_State;
                conn->c_ConnId = 0;

                LOGI1(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Connection %x closed", conn);

            } else {

                LOGI0(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Close attempt aborted");

                conn->c_Status = APB_RS_ABORTED;
            }

        } else {

            LOGI0(conn->c_Ctx->ctx_Logger, LOG_TRACE, "Could not contact server");

            conn->c_Status = APB_RS_NO_SERVER;
        }

        APB_FreeRequest(req);
    }
}

UWORD __asm __saveds APB_ConnectionState(
    register __a0 APTR connection)
{
    return ((struct Connection *) connection)->c_Status;
}
