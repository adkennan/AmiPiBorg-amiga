
#include "connection.h"
#include "protocol.h"
#include "objtype.h"

#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <exec/lists.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

struct Connection {

    struct MinNode cnn_Node;
    APTR      cnn_Ctx;
    PacketWriter cnn_PacketWriter;
    struct MinList cnn_PacketQueue;
    struct MinList cnn_EventQueue;
    struct MinList cnn_ReqQueue;
    UWORD     cnn_Id;
    UWORD     cnn_HandlerId;
    UWORD     cnn_State;
};

struct RequestInt {
    struct APBRequest ri_Req;
    APTR      ri_ClientCnn;
    UWORD     ri_TimeoutTicks;
};

#define REQ_LIST(c) ((struct List *)&c->cnn_ReqQueue)
#define PAC_LIST(c) ((struct List *)&c->cnn_PacketQueue)
#define EVT_LIST(c) ((struct List *)&c->cnn_EventQueue)

VOID APB_CnnConnect(
    struct Connection *c)
{
    struct PacketConnect *p;

    LOG1(c->cnn_Ctx, LOG_DEBUG, "Send Connect %d", c->cnn_Id);

    if(!(p = (struct PacketConnect *) APB_AllocPacket(c->cnn_PacketWriter, sizeof(struct PacketConnect)))) {
        LOG0(c->cnn_Ctx, LOG_ERROR, "Failed to allocate packet\n");
        return;
    }

    p->pc_Pac.pac_ConnId = c->cnn_Id;
    p->pc_Pac.pac_Type = PT_CONNECT;
    p->pc_HandlerId = c->cnn_HandlerId;

    c->cnn_State = CS_CONNECTING;
}

VOID APB_CnnDisconnect(
    struct Connection * c)
{
    struct Packet *p;

    LOG1(c->cnn_Ctx, LOG_DEBUG, "Send Disconnect %d", c->cnn_Id);

    if(!(p = APB_AllocPacket(c->cnn_PacketWriter, sizeof(struct Packet)))) {
        LOG0(c->cnn_Ctx, LOG_ERROR, "Failed to allocate packet\n");
        return;
    }

    p->pac_ConnId = c->cnn_Id;
    p->pac_Type = PT_DISCONNECT;

    c->cnn_State = CS_DISCONNECTING;
}

Connection APB_CreateConnection(
    APTR ctx,
    struct List * cnns,
    UWORD connId,
    PacketWriter packetWriter)
{
    struct Connection *c;

    if(!APB_TypeRegistered(ctx, OT_CONNECTION)) {
        APB_RegisterObjectType(ctx, OT_CONNECTION, "Connection", sizeof(struct Connection), 5, 1000);
    }

    if(!(c = APB_AllocObject(ctx, OT_CONNECTION))) {
        LOG0(ctx, LOG_ERROR, "Failed to allocate connection\n");
        return NULL;
    }

    LOG1(ctx, LOG_DEBUG, "Create Connection %d", connId);

//    APB_IncrementStat(ST_CNN_CREATED, 1);
//    APB_IncrementStat(ST_CNN_CURRENT, 1);

    c->cnn_Ctx = ctx;
    c->cnn_PacketWriter = packetWriter;
    c->cnn_Id = connId;
    c->cnn_HandlerId = 0;

    NewList(PAC_LIST(c));
    NewList(REQ_LIST(c));

    AddTail(cnns, (struct Node *) c);

    return c;
}

VOID APB_DestroyConnection(
    Connection cnn)
{
    struct Connection *c = (struct Connection *) cnn;
    struct InPacket *ip;

    LOG1(c->cnn_Ctx, LOG_DEBUG, "Destroy Connect %d", c->cnn_Id);

    if(c->cnn_State < CS_DISCONNECTING) {
        APB_CnnDisconnect(c);
    }

    while(!IsListEmpty(PAC_LIST(c))) {
        ip = (struct InPacket *) RemHead(PAC_LIST(c));
        APB_ReleaseInPacket(ip);
    }

    Remove((struct Node *) c);

    APB_FreeObject(c->cnn_Ctx, OT_CONNECTION, c);

//    APB_IncrementStat(ST_CNN_CURRENT, -1);
}

VOID APB_PushToRemote(
    struct Connection *c,
    struct APBRequest *req)
{
    struct Packet *p;
    BYTE     *dest;

    if(!(p = APB_AllocPacket(c->cnn_PacketWriter, sizeof(struct Packet) + req->r_Length))) {

        LOG0(c->cnn_Ctx, LOG_ERROR, "Failed to allocate packet\n");
        req->r_State = APB_RS_FAILED;

    } else {

        LOG2(c->cnn_Ctx, LOG_TRACE, "Conn %d Outgoing Data Packet %d bytes", c->cnn_Id, req->r_Length);

        p->pac_ConnId = c->cnn_Id;
        p->pac_Type = PT_DATA;

        dest = APB_PointerAdd(p, sizeof(struct Packet));

        CopyMem(req->r_Data, dest, req->r_Length);

        req->r_State = APB_RS_OK;
        req->r_Actual = req->r_Length;
    }
}

VOID APB_PushToClient(
    struct Connection *c)
{
    struct APBRequest *r;
    struct InPacket *ip;
    UWORD     toCopy;

    if(c->cnn_State != CS_CONNECTED) {
        return;
    }

    while(!IsListEmpty(PAC_LIST(c)) && !IsListEmpty(REQ_LIST(c))) {

        r = (struct APBRequest *) RemHead(REQ_LIST(c));
        ip = (struct InPacket *) RemHead(PAC_LIST(c));

        toCopy = ip->ip_Data1Length + ip->ip_Data2Length;
        r->r_Actual = toCopy;

        if(r->r_Length < toCopy) {

            LOG3(c->cnn_Ctx, LOG_ERROR, "Conn %d Buffer of %d bytes to small to receive %d bytes", c->cnn_Id, r->r_Length, toCopy);

            r->r_State = APB_RS_BUFFER_TOO_SMALL;

            AddHead(PAC_LIST(c), (struct Node *) ip);

        } else {

            LOG2(c->cnn_Ctx, LOG_TRACE, "Conn %d Incoming Data Packet %d bytes", c->cnn_Id, toCopy);

            APB_InPacketToBuffer(ip, r->r_Data);

            r->r_State = APB_RS_OK;

            APB_ReleaseInPacket(ip);
        }

        ReplyMsg((struct Message *) r);
    }
}

VOID APB_HandleConnectionPacket(
    Connection cnn,
    struct InPacket *ip)
{
    struct Connection *c = (struct Connection *) cnn;
    struct APBRequest *req;
    BOOL      releasePacket = FALSE;
    BOOL      replyReq = FALSE;
    BOOL      destroyCnn = FALSE;
    UWORD     replyReqType = 0;
    UWORD     replyReqState = 0;

    switch (ip->ip_Type) {

    case PT_CONNECTED:
        LOG1(c->cnn_Ctx, LOG_TRACE, "Conn %d Connected", c->cnn_Id);
        c->cnn_State = CS_CONNECTED;
        releasePacket = TRUE;
        replyReq = TRUE;
        replyReqType = APB_RT_OPEN;
        replyReqState = APB_RS_OK;

//        APB_IncrementStat(ST_CNN_SUCCESSES, 1);
        break;

    case PT_DISCONNECTED:
        LOG1(c->cnn_Ctx, LOG_TRACE, "Conn %d Disconnected", c->cnn_Id);
        c->cnn_State = CS_DISCONNECTED;
        releasePacket = TRUE;
        destroyCnn = TRUE;
        break;

    case PT_NO_HANDLER:
        LOG1(c->cnn_Ctx, LOG_ERROR, "Conn %d No Handler", c->cnn_Id);
        c->cnn_State = CS_NO_HANDLER;
        replyReq = TRUE;
        replyReqType = APB_RT_OPEN;
        replyReqState = APB_RS_NO_HANDLER;

//        APB_IncrementStat(ST_CNN_FAILURES, 1);

        break;

    default:
        if(c->cnn_State >= CS_DISCONNECTING) {
            releasePacket = TRUE;
        } else {
            AddTail(PAC_LIST(c), (struct Node *) ip);
            APB_PushToClient(c);
        }
        break;
    }

    if(releasePacket) {
        APB_ReleaseInPacket(ip);
    }

    if(replyReq) {

        for(req = (struct APBRequest *) REQ_LIST(c)->lh_Head; req->r_Msg.mn_Node.ln_Succ; req = (struct APBRequest *) req->r_Msg.mn_Node.ln_Succ) {

            if(req->r_Type == replyReqType) {
                req->r_State = replyReqState;

                Remove((struct Node *) req);

                ReplyMsg((struct Message *) req);

                break;
            }
        }
    }

    if(destroyCnn) {
        APB_DestroyConnection(c);
    }
}

VOID APB_CnnCheckRequestTimeouts(
    struct Connection *c)
{
    struct RequestInt *req;

    for(req = (struct RequestInt *) REQ_LIST(c)->lh_Head; req->ri_Req.r_Msg.mn_Node.ln_Succ; req = (struct RequestInt *) req->ri_Req.r_Msg.mn_Node.ln_Succ) {

        if(req->ri_TimeoutTicks == 1) {

            LOG2(c->cnn_Ctx, LOG_TRACE, "Conn %d Request 0x%x Timed Out", c->cnn_Id, req);

            req->ri_Req.r_State = APB_RS_TIMEOUT;

            Remove((struct Node *) req);

            ReplyMsg((struct Message *) req);

        } else if(req->ri_TimeoutTicks > 1) {
            req->ri_TimeoutTicks--;
        }
    }
}

VOID APB_HandleClientRequest(
    Connection cnn,
    struct APBRequest *req)
{
    struct Connection *c = (struct Connection *) cnn;
    BOOL      reply = FALSE;
    struct RequestInt *ri = (struct RequestInt *) req;

    req->r_State = APB_RS_OK;
    req->r_Actual = 0;
    req->r_ConnId = c->cnn_Id;

    if(req->r_Timeout > 0) {
        ri->ri_TimeoutTicks = req->r_Timeout;
    }

    switch (req->r_Type) {

    case APB_RT_OPEN:
        LOG2(c->cnn_Ctx, LOG_TRACE, "Conn %d Open Handler %d", c->cnn_Id, req->r_HandlerId);
        c->cnn_HandlerId = req->r_HandlerId;
        AddTail(REQ_LIST(c), (struct Node *) req);
        APB_CnnConnect(c);
        break;

    case APB_RT_CLOSE:
        LOG1(c->cnn_Ctx, LOG_TRACE, "Conn %d Close", c->cnn_Id);
        reply = TRUE;
        if(c->cnn_State < CS_DISCONNECTING) {
            APB_CnnDisconnect(c);
        }
        break;

    case APB_RT_READ:
        LOG1(c->cnn_Ctx, LOG_TRACE, "Conn %d Read", c->cnn_Id);
        switch (c->cnn_State) {

        case CS_DISCONNECTING:
        case CS_DISCONNECTED:

            LOG1(c->cnn_Ctx, LOG_ERROR, "Conn %d Cannot Read while disconnecting or disconnected", c->cnn_Id);

            req->r_State = APB_RS_NOT_CONNECTED;
            reply = TRUE;
            break;

        case CS_NO_HANDLER:

            LOG1(c->cnn_Ctx, LOG_TRACE, "Conn %d Cannot Read without a handler", c->cnn_Id);

            req->r_State = APB_RS_NO_HANDLER;
            reply = TRUE;
            break;

        default:
            reply = FALSE;
            AddTail(REQ_LIST(c), (struct Node *) req);
            APB_PushToClient(c);
            break;
        }
        break;

    case APB_RT_WRITE:
        switch (c->cnn_State) {

        case CS_DISCONNECTING:
        case CS_DISCONNECTED:
            LOG1(c->cnn_Ctx, LOG_ERROR, "Conn %d Cannot Write while disconnecting or disconnected", c->cnn_Id);

            req->r_State = APB_RS_NOT_CONNECTED;
            reply = TRUE;
            break;

        case CS_NO_HANDLER:
            LOG1(c->cnn_Ctx, LOG_TRACE, "Conn %d Cannot Write without a handler", c->cnn_Id);

            req->r_State = APB_RS_NO_HANDLER;
            reply = TRUE;
            break;

        default:
            reply = TRUE;
            req->r_State = APB_RS_OK;
            APB_PushToRemote(c, req);
            break;
        }
        break;

    default:
        LOG1(c->cnn_Ctx, LOG_ERROR, "Unknown request type %d\n", req->r_Type);
        reply = TRUE;
        break;
    }

    if(reply) {
        ReplyMsg((struct Message *) req);
    }
}

UWORD APB_GetConnectionState(
    Connection cnn)
{
    struct Connection *c = (struct Connection *) cnn;

    return c->cnn_State;
}

UWORD APB_GetId(
    Connection cnn)
{
    struct Connection *c = (struct Connection *) cnn;

    return c->cnn_Id;
}

Connection APB_FindConnection(
    struct List * cnns,
    UWORD id)
{
    struct Connection *cnn;

    for(cnn = (struct Connection *) (cnns->lh_Head); cnn->cnn_Node.mln_Succ; cnn = (struct Connection *) cnn->cnn_Node.mln_Succ) {

        if(cnn->cnn_Id == id) {
            return cnn;
        }

    }
    return NULL;
}

VOID APB_CheckRequestTimeouts(
    struct List * cnns)
{
    struct Connection *cnn;

    for(cnn = (struct Connection *) (cnns->lh_Head); cnn->cnn_Node.mln_Succ; cnn = (struct Connection *) cnn->cnn_Node.mln_Succ) {

        APB_CnnCheckRequestTimeouts(cnn);
    }
}
