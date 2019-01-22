#include "lib.h"

#include <exec/ports.h>

#include <clib/alib_protos.h>
#include <clib/exec_protos.h>

UWORD SendControlMessage(
    UWORD rt,
    BYTE * data,
    UWORD length)
{
    struct MsgPort *port;
    struct RequestInt *req;
    ULONG     sig;
    UWORD     result = APB_RS_FAILED;

    if(port = CreatePort(NULL, 0)) {

        if(req = (struct RequestInt *) APB_AllocMem(NULL, sizeof(struct RequestInt))) {

            req->r_Req.r_Msg.mn_ReplyPort = port;
            req->r_Req.r_Msg.mn_Length = sizeof(struct RequestInt);
            req->r_Req.r_ConnId = 0;
            req->r_Conn = NULL;

            req->r_Req.r_Timeout = CNN_MSG_TIMEOUT;
            req->r_Req.r_Type = rt;
            req->r_Req.r_Length = length;
            req->r_Req.r_Data = data;

            APB_PutMsg(NULL, (struct Message *) req);

            sig = Wait(1 << port->mp_SigBit | SIGBREAKF_CTRL_C);

            if(sig & (1 << port->mp_SigBit)) {

                GetMsg(req->r_Req.r_Msg.mn_ReplyPort);

                result = req->r_Req.r_State;

            } else {

                result = APB_RS_ABORTED;
            }

            APB_FreeMem(NULL, req, sizeof(struct RequestInt));
        }

        DeletePort(port);
    }

    return result;
}

UWORD __asm __saveds APB_SrvQuit(
    VOID)
{
    return SendControlMessage(APB_RT_SRV_QUIT, NULL, 0);
}

UWORD __asm __saveds APB_SrvSetLogLevel(
    register __d0 UWORD level)
{
    UWORD     l = level;

    return SendControlMessage(APB_RT_SRV_LOG_LEVEL, (BYTE *) & l, sizeof(UWORD));
}

UWORD __asm __saveds APB_SrvGetLog(
    register __a0 BYTE * data,
    register __d0 UWORD length)
{
    return SendControlMessage(APB_RT_SRV_LOG, data, length);
}

UWORD __asm __saveds APB_SrvGetConnections(
    register __a0 BYTE * data,
    register __d0 UWORD length)
{
    return SendControlMessage(APB_RT_SRV_CONNS, data, length);
}

UWORD __asm __saveds APB_SrvGetStats(
    register __a0 BYTE * data,
    register __d0 UWORD length)
{
    return SendControlMessage(APB_RT_SRV_STATS, data, length);
}
