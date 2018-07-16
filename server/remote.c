
#include "remote.h"
#include "server.h"
#include "stats.h"
#include "protocol.h"

#include <exec/io.h>

#include <devices/serial.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

struct Remote {
    MemoryPool r_MemPool;
    ObjectPool r_ObjPool;
    struct MsgPort *r_ReadPort;
    struct MsgPort *r_WritePort;
    struct IOExtSer *r_ReadReq;
    struct IOExtSer *r_WriteReq;
    BYTE     *r_ReadBuffer;
    UWORD     r_ReadBufLen;
    UWORD     r_BytesRead;
    BOOL      r_ReadReady;
    BOOL      r_WriteReady;
};

Remote APB_CreateRemote(
    MemoryPool memPool,
    ObjectPool objPool)
{
    struct Remote *r;

    if(!(r = APB_AllocMem(memPool, sizeof(struct Remote)))) {
        return NULL;
    }

    r->r_MemPool = memPool;
    r->r_ObjPool = objPool;
    r->r_ReadPort = NULL;
    r->r_WritePort = NULL;
    r->r_ReadReq = NULL;
    r->r_WriteReq = NULL;
    r->r_ReadReady = FALSE;
    r->r_WriteReady = FALSE;
    r->r_ReadBuffer = NULL;
    r->r_ReadBufLen = 0;
    r->r_BytesRead = 0;

    return r;
}

VOID APB_BeginRead(
    struct Remote * r)
{
    struct IOExtSer *req = r->r_ReadReq;

    r->r_BytesRead = 0;

    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Length = 1;
    req->IOSer.io_Data = r->r_ReadBuffer;

    SendIO((struct IORequest *) req);
}

VOID APB_ContinueRead(
    struct Remote *r)
{
    struct IOExtSer *req = r->r_ReadReq;
    UWORD     size;

    r->r_BytesRead += req->IOSer.io_Actual;

    req->IOSer.io_Command = SDCMD_QUERY;

    DoIO((struct IORequest *) req);

    size = r->r_ReadBufLen - r->r_BytesRead;

    if(req->IOSer.io_Actual == 0 || size == 0) {
        return;
    }

    if(size > req->IOSer.io_Actual) {
        size = req->IOSer.io_Actual;
    }

    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Length = size;
    req->IOSer.io_Data = r->r_ReadBuffer + r->r_BytesRead;

    DoIO((struct IORequest *) req);

    r->r_BytesRead += size;
}


BOOL APB_OpenRemote(
    Remote remote)
{
    struct Remote *r = (struct Remote *) remote;
    struct IOExtSer *readReq;
    struct IOExtSer *writeReq;
    struct MsgPort *readPort;
    struct MsgPort *writePort;

    if(readPort = CreateMsgPort()) {

        if(writePort = CreateMsgPort()) {

            if(readReq = (struct IOExtSer *) CreateExtIO(readPort, sizeof(struct IOExtSer))) {

                if(writeReq = (struct IOExtSer *) CreateExtIO(writePort, sizeof(struct IOExtSer))) {

                    if(OpenDevice(SERIALNAME, 0, (struct IORequest *) writeReq, 0) == 0) {

                        writeReq->IOSer.io_Command = SDCMD_SETPARAMS;
                        writeReq->io_SerFlags &= ~SERF_PARTY_ON;
                        writeReq->io_SerFlags |= SERF_XDISABLED;
                        writeReq->io_Baud = 19200;
                        DoIO((struct IORequest *) writeReq);

                        writeReq->IOSer.io_Command = CMD_FLUSH;
                        DoIO((struct IORequest *) writeReq);

                        CopyMem(writeReq, readReq, sizeof(struct IOExtSer));
                        readReq->IOSer.io_Message.mn_ReplyPort = readPort;

                        r->r_ReadPort = readPort;
                        r->r_WritePort = writePort;
                        r->r_ReadReq = readReq;
                        r->r_WriteReq = writeReq;

                        r->r_ReadReady = FALSE;
                        r->r_WriteReady = TRUE;

                        return TRUE;
                    }

                    DeleteExtIO((struct IORequest *) writeReq);
                }

                DeleteExtIO((struct IORequest *) readReq);
            }

            DeleteMsgPort(writePort);
        }

        DeleteMsgPort(readPort);
    }

    return FALSE;
}

VOID APB_CloseRemote(
    Remote remote)
{
    struct Remote *r = (struct Remote *) remote;

    if(r->r_ReadReq) {

        AbortIO((struct IORequest *) r->r_ReadReq);
        WaitIO((struct IORequest *) r->r_ReadReq);


        APB_DeletePort(r->r_ReadPort);
        DeleteExtIO((struct IORequest *) r->r_ReadReq);
    }

    if(r->r_WriteReq) {

        AbortIO((struct IORequest *) r->r_WriteReq);
        WaitIO((struct IORequest *) r->r_WriteReq);

        CloseDevice((struct IORequest *) r->r_WriteReq);

        APB_DeletePort(r->r_WritePort);
        DeleteExtIO((struct IORequest *) r->r_WriteReq);
    }

    r->r_ReadPort = NULL;
    r->r_WritePort = NULL;
    r->r_ReadReq = NULL;
    r->r_WriteReq = NULL;
    r->r_ReadReady = FALSE;
    r->r_WriteReady = FALSE;
    r->r_ReadBuffer = NULL;
    r->r_ReadBufLen = 0;
    r->r_BytesRead = 0;
}

VOID APB_DestroyRemote(
    Remote remote)
{
    struct Remote *r = (struct Remote *) remote;

    APB_CloseRemote(remote);

    APB_FreeMem(r->r_MemPool, r, sizeof(struct Remote));
}

ULONG APB_GetSigBits(
    Remote remote)
{
    struct Remote *r = (struct Remote *) remote;

    if(r->r_ReadPort == NULL || r->r_WritePort == NULL) {
        return 0;
    }

    return (1U << r->r_ReadPort->mp_SigBit)
        | (1U << r->r_WritePort->mp_SigBit);

}

VOID APB_HandleSignal(
    Remote remote,
    ULONG sigBits)
{
    struct Remote *r = (struct Remote *) remote;

    if(sigBits & (1 << r->r_ReadPort->mp_SigBit)) {

        WaitIO((struct IORequest *) r->r_ReadReq);

        APB_ContinueRead(r);

        r->r_ReadReady = TRUE;
    }

    if(sigBits & (1 << r->r_WritePort->mp_SigBit)) {

        WaitIO((struct IORequest *) r->r_WriteReq);

        r->r_WriteReady = TRUE;
    }
}

BOOL APB_CanSendToRemote(
    Remote remote)
{
    struct Remote *r = (struct Remote *) remote;

    return r->r_WriteReady;
}

VOID APB_SendToRemote(
    Remote remote,
    BYTE * data,
    UWORD length)
{
    struct Remote *r = (struct Remote *) remote;
    struct IOExtSer *req = (struct IOExtSer *) r->r_WriteReq;

    req->IOSer.io_Command = CMD_WRITE;
    req->IOSer.io_Length = length;
    req->IOSer.io_Data = data;

    r->r_WriteReady = FALSE;

    SendIO((struct IORequest *) req);
}

BOOL APB_CanReceiveFromRemote(
    Remote remote)
{
    struct Remote *r = (struct Remote *) remote;

    return r->r_ReadReady;
}

VOID APB_ReceiveFromRemote(
    Remote remote,
    BYTE * data,
    UWORD length)
{
    struct Remote *r = (struct Remote *) remote;

    r->r_ReadBuffer = data;
    r->r_ReadBufLen = length;
    r->r_BytesRead = 0;

    r->r_ReadReady = FALSE;

    APB_BeginRead(r);
}

UWORD APB_BytesReceived(
    Remote remote)
{
    struct Remote *r = (struct Remote *) remote;

    return r->r_BytesRead;
}
