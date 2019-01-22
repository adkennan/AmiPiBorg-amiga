
#include "lib.h"

#include <clib/exec_protos.h>


VOID __asm __saveds APB_Read(
    register __a0 struct APBRequest * request)
{
    struct RequestInt *r = (struct RequestInt *) request;

    r->r_Req.r_Type = APB_RT_READ;
    r->r_Req.r_HandlerId = r->r_Conn->c_HandlerId;
    r->r_Req.r_ConnId = r->r_Conn->c_ConnId;

    LOGI1(r->r_Conn->c_Ctx->ctx_Logger, LOG_TRACE, "Read Request %x", r);

    APB_PutMsg(r->r_Conn->c_Ctx, (struct Message *) r);
}

VOID __asm __saveds APB_Write(
    register __a0 struct APBRequest *request)
{
    struct RequestInt *r = (struct RequestInt *) request;

    r->r_Req.r_Type = APB_RT_WRITE;
    r->r_Req.r_HandlerId = r->r_Conn->c_HandlerId;
    r->r_Req.r_ConnId = r->r_Conn->c_ConnId;

    LOGI1(r->r_Conn->c_Ctx->ctx_Logger, LOG_TRACE, "Write Request %x", r);

    APB_PutMsg(r->r_Conn->c_Ctx, (struct Message *) r);
}

VOID __asm __saveds APB_Abort(
    register __a0 struct APBRequest *request)
{
    struct RequestInt *r = (struct RequestInt *) request;
    struct Node *n = (struct Node *) request;

    LOGI1(r->r_Conn->c_Ctx->ctx_Logger, LOG_TRACE, "Abort Request %x", r);

    if(n->ln_Pred || n->ln_Succ) {
        Remove(n);
    }
}
