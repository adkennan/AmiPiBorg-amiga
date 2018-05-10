
#include "remote.h"
#include "server.h"
#include "buffer.h"
#include "stats.h"
#include "protocol.h"

#include <exec/io.h>

#include <devices/serial.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

struct Remote
{
    MemoryPool          r_MemPool;
    ObjectPool          r_ObjPool;
    struct MsgPort     *r_ReadPort;
    struct MsgPort     *r_WritePort;
    struct IOExtSer    *r_ReadReq;
    struct IOExtSer    *r_WriteReq;
    struct Buffer      *r_ReadBuffer;
    BOOL                r_ReadReady;
    BOOL                r_WriteReady;    
};

Remote APB_CreateRemote(MemoryPool memPool, ObjectPool objPool)
{
    struct Remote *r;

    if( ! (r = APB_AllocMem(memPool, sizeof(struct Remote) ) ) ) {
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

    return r;
}

struct Buffer *APB_GetReadBuffer(struct Remote *r, UWORD *length)
{
    struct Buffer *buf = r->r_ReadBuffer;

    if( buf == NULL || buf->b_Offset == BUFFER_SIZE ) {

        if( ! ( buf = APB_AllocateBuffer(r->r_ObjPool) ) ) {
            return NULL;
        }
        r->r_ReadBuffer = buf;
    }

	if( *length > BUFFER_SIZE - buf->b_Offset ) {
        *length = BUFFER_SIZE - buf->b_Offset;
    }

    return buf;
}

VOID APB_BeginRead(struct Remote *r)
{
    struct Buffer *buf;
    struct IOExtSer *req = r->r_ReadReq;
    UWORD length = sizeof(struct Packet);

    if( ! (buf = APB_GetReadBuffer(r, &length) ) ) {
        return;
    }
    
    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Length = 1;
    req->IOSer.io_Data = APB_PointerAdd(buf->b_Data, buf->b_Offset);

    SendIO((struct IORequest *)req);
}

VOID APB_ContinueRead(struct Remote *r)
{
    struct Buffer *buf;
    struct IOExtSer *req = r->r_ReadReq;
    UWORD size;

    r->r_ReadBuffer->b_Offset++;

    req->IOSer.io_Command = SDCMD_QUERY;

    DoIO((struct IORequest *)req);

    if( req->IOSer.io_Actual == 0 ) {
        return;
    }

    size = req->IOSer.io_Actual > BUFFER_SIZE ? BUFFER_SIZE : req->IOSer.io_Actual;

    if( ! (buf = APB_GetReadBuffer(r, &size) ) ) {
        return;
    }

    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Length = size;
    req->IOSer.io_Data = APB_PointerAdd(buf->b_Data, buf->b_Offset);

    DoIO((struct IORequest *)req);

    buf->b_Offset += size;
}


BOOL APB_OpenRemote(Remote remote)
{
    struct Remote *r = (struct Remote *)remote;
    struct IOExtSer *readReq;
    struct IOExtSer *writeReq;
    struct MsgPort  *readPort;
    struct MsgPort  *writePort;

    if( readPort = CreateMsgPort() ) {
        
        if( writePort = CreateMsgPort() ) {

            if( readReq = (struct IOExtSer *)CreateExtIO(readPort, sizeof(struct IOExtSer)) ) {

                if( writeReq = (struct IOExtSer *)CreateExtIO(writePort, sizeof(struct IOExtSer)) ) {

                    if( OpenDevice(SERIALNAME, 0, (struct IORequest *)writeReq, 0) == 0 ) {
                        
                        writeReq->IOSer.io_Command = SDCMD_SETPARAMS;
                        writeReq->io_SerFlags &= ~SERF_PARTY_ON;
                        writeReq->io_SerFlags |= SERF_XDISABLED;
                        writeReq->io_Baud = 19200;
                        DoIO((struct IORequest *)writeReq);

                        writeReq->IOSer.io_Command = CMD_FLUSH;
                        DoIO((struct IORequest *)writeReq);

                        CopyMem(writeReq, readReq, sizeof(struct IOExtSer));
                        readReq->IOSer.io_Message.mn_ReplyPort = readPort;

                        r->r_ReadPort = readPort;
                        r->r_WritePort = writePort;
                        r->r_ReadReq = readReq;
                        r->r_WriteReq = writeReq;

                        r->r_ReadReady = FALSE;
                        r->r_WriteReady = TRUE;

                        APB_BeginRead(r);

                        return TRUE;
                    }

                    DeleteExtIO((struct IORequest *)writeReq);
                }

                DeleteExtIO((struct IORequest *)readReq);
            }

            DeleteMsgPort(writePort);
        }

        DeleteMsgPort(readPort);
    }

    return FALSE;
}

VOID APB_CloseRemote(Remote remote)
{
    struct Remote *r = (struct Remote *)remote;

    if( r->r_ReadReq ) {

        AbortIO((struct IORequest *)r->r_ReadReq);
        WaitIO((struct IORequest *)r->r_ReadReq);
   

        APB_DeletePort(r->r_ReadPort);
        DeleteExtIO((struct IORequest *)r->r_ReadReq);
    }

    if( r->r_WriteReq ) {
    
        AbortIO((struct IORequest *)r->r_WriteReq);
        WaitIO((struct IORequest *)r->r_WriteReq);

        CloseDevice((struct IORequest *)r->r_WriteReq);

        APB_DeletePort(r->r_WritePort);
        DeleteExtIO((struct IORequest *)r->r_WriteReq);
    }

    if( r->r_ReadBuffer ) {
        APB_ReleaseBuffer(r->r_ReadBuffer);
    }

    r->r_ReadPort = NULL;
    r->r_WritePort = NULL;
    r->r_ReadReq = NULL;
    r->r_WriteReq = NULL;
    r->r_ReadReady = FALSE;
    r->r_WriteReady = FALSE;
    r->r_ReadBuffer = NULL;
}

VOID APB_DestroyRemote(Remote remote)
{
    struct Remote *r = (struct Remote *)remote;

    APB_CloseRemote(remote);

    APB_FreeMem(r->r_MemPool, r, sizeof(struct Remote));    
}

ULONG APB_GetSigBits(Remote remote)
{
    struct Remote *r = (struct Remote *)remote;

    if( r->r_ReadPort == NULL || r->r_WritePort == NULL ) {
        return 0;
    }

    return  (1U << r->r_ReadPort->mp_SigBit)
          | (1U << r->r_WritePort->mp_SigBit);
        
}

VOID APB_HandleSignal(Remote remote, ULONG sigBits)
{
    struct Remote *r = (struct Remote *)remote;

    if( sigBits & (1 << r->r_ReadPort->mp_SigBit ) ) {
        
        WaitIO((struct IORequest *)r->r_ReadReq);

        APB_ContinueRead(r);

        r->r_ReadReady = TRUE;   
    }

    if( sigBits & (1 << r->r_WritePort->mp_SigBit ) ) {

        WaitIO((struct IORequest *)r->r_WriteReq);

        r->r_WriteReady = TRUE;
    }
}

BOOL APB_CanSendToRemote(Remote remote)
{
    struct Remote *r = (struct Remote *)remote;

    return r->r_WriteReady;
}

VOID APB_SendToRemote(Remote remote, BYTE *data, UWORD length)
{
    struct Remote *r = (struct Remote *)remote;
    struct IOExtSer *req = (struct IOExtSer *)r->r_WriteReq;
    
    req->IOSer.io_Command = CMD_WRITE;
    req->IOSer.io_Length = length;
    req->IOSer.io_Data = data;

    r->r_WriteReady = FALSE;

    SendIO((struct IORequest *)req);
}

BOOL APB_CanReceiveFromRemote(Remote remote)
{
    struct Remote *r = (struct Remote *)remote;

    return r->r_ReadReady;
}

struct Buffer *APB_ReceiveFromRemote(Remote remote)
{
    struct Remote *r = (struct Remote *)remote;
    struct Buffer *buf = r->r_ReadBuffer;

    r->r_ReadReady = FALSE;

    APB_BeginRead(r);
        
    return buf;
}

