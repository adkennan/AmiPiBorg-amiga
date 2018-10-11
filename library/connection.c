
#include "lib.h"

#include <clib/exec_protos.h>

#include <string.h>

BOOL APB_PutMsg(
    struct Message *msg)
{
    struct MsgPort *serverPort;

    Forbid();

    if(serverPort = FindPort(PORT_NAME)) {
        PutMsg(serverPort, msg);
    }

    Permit();

    return (BOOL) (serverPort != NULL);
}

APTR __asm __saveds APB_AllocConnection(
    register __a0 struct MsgPort * port,
    register __d0 UWORD handlerId,
    register __a1 APTR memoryPool)
{
    struct Connection *conn;

    if(conn = (struct Connection *) APB_AllocMem(memoryPool, sizeof(struct Connection))) {

        conn->c_HandlerId = handlerId;
        conn->c_MsgPort = port;
        conn->c_MemPool = memoryPool;
        conn->c_Status = APB_RS_NO_CONNECTION;
        conn->c_ConnId = 0;
    }

    return conn;
}

VOID __asm __saveds APB_FreeConnection(
    register __a0 APTR connection)
{
    struct Connection *conn = (struct Connection *) connection;

    APB_FreeMem(conn->c_MemPool, conn, sizeof(struct Connection));
}

BOOL __asm __saveds APB_OpenConnection(
    register __a0 APTR connection)
{
    struct Connection *conn = (struct Connection *) connection;
    struct APBRequest *req;
    ULONG     sig;

    if(req = APB_AllocRequest(conn)) {

        req->r_Type = APB_RT_OPEN;
        req->r_HandlerId = conn->c_HandlerId;
        req->r_Msg.mn_ReplyPort = conn->c_MsgPort;
        req->r_Timeout = CNN_MSG_TIMEOUT;

        if(APB_PutMsg((struct Message *) req)) {

            sig = Wait(1 << req->r_Msg.mn_ReplyPort->mp_SigBit | SIGBREAKF_CTRL_C);

            if(sig & (1 << req->r_Msg.mn_ReplyPort->mp_SigBit)) {

                GetMsg(req->r_Msg.mn_ReplyPort);

                conn->c_Status = req->r_State;
                conn->c_ConnId = req->r_ConnId;

            } else {

                conn->c_Status = APB_RS_ABORTED;
            }
        } else {

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

    if(req = APB_AllocRequest(conn)) {

        req->r_Type = APB_RT_CLOSE;
        req->r_HandlerId = conn->c_HandlerId;
        req->r_ConnId = conn->c_ConnId;
        req->r_Msg.mn_ReplyPort = conn->c_MsgPort;
        req->r_Timeout = CNN_MSG_TIMEOUT;

        if(APB_PutMsg((struct Message *) req)) {

            sig = Wait(1 << req->r_Msg.mn_ReplyPort->mp_SigBit | SIGBREAKF_CTRL_C);

            if(sig & (1 << req->r_Msg.mn_ReplyPort->mp_SigBit)) {

                GetMsg(req->r_Msg.mn_ReplyPort);

                conn->c_Status = req->r_State;
                conn->c_ConnId = 0;

            } else {

                conn->c_Status = APB_RS_ABORTED;
            }

        } else {

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

VOID __asm __saveds APB_Log(
    register __a0 APTR connection,
    register __d0 UWORD level,
    register __a1 STRPTR msg)
{
    struct Connection *conn = (struct Connection *) connection;
    struct APBRequest *req;
    ULONG     sig;

    if(req = APB_AllocRequest(conn)) {

        req->r_Type = APB_RT_LOG;
        req->r_HandlerId = conn->c_HandlerId;
        req->r_ConnId = conn->c_ConnId;
        req->r_Msg.mn_ReplyPort = conn->c_MsgPort;
        req->r_Timeout = CNN_MSG_TIMEOUT;

        req->r_DataType = level;
        req->r_Length = strlen(msg) + 1;
        req->r_Data = msg;

        if(APB_PutMsg((struct Message *) req)) {

            sig = Wait(1 << req->r_Msg.mn_ReplyPort->mp_SigBit | SIGBREAKF_CTRL_C);

            if(sig & (1 << req->r_Msg.mn_ReplyPort->mp_SigBit)) {

                GetMsg(req->r_Msg.mn_ReplyPort);

                conn->c_Status = req->r_State;

            } else {

                conn->c_Status = APB_RS_ABORTED;
            }

        } else {

            conn->c_Status = APB_RS_NO_SERVER;
        }

        APB_FreeRequest(req);
    }
}
