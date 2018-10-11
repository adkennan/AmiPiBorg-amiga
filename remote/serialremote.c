
#define LIB_NAME "serialremote"
#define LIB_VERSION 1
#define LIB_REVISION 0
#define LIB_VER_STR "1.0 (14/10/2018)"

#include <exec/types.h>

#include <exec/io.h>

#include <devices/serial.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <string.h>

#define ARG_DEVICE 0
#define ARG_UNIT 1
#define ARG_BAUD 2
#define ARG_DATABITS 3
#define ARG_PARITY 4
#define ARG_STOPBITS 5
#define ARG_COUNT 6

#define DEF_UNIT 0
#define DEF_BAUD 19200
#define DEF_DATABITS 8
#define DEF_PARITY FALSE
#define DEF_STOPBITS 1

#define ARG_TEMPLATE "DEVICE/K,UNIT/K/N,BAUD/K/N,DATABITS/K/N,PARITY/S,STOPBITS/K/N"

struct Remote {
    struct MsgPort *r_ReadPort;
    struct MsgPort *r_WritePort;
    struct IOExtSer *r_ReadReq;
    struct IOExtSer *r_WriteReq;
    BYTE     *r_ReadBuf;
    UWORD     r_ReadBufSize;
    UWORD     r_BytesRead;
    BOOL      r_ReadReady;
    BOOL      r_WriteReady;
    STRPTR    r_Device;
    UWORD     r_Unit;
    UWORD     r_DataBits;
    UWORD     r_StopBits;
    LONG      r_Baud;
    BOOL      r_Parity;
};

#include "lib.c"

VOID BeginRead(
    struct Remote *r)
{
    struct IOExtSer *req = r->r_ReadReq;

    r->r_BytesRead = 0;

    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Length = 1;
    req->IOSer.io_Data = r->r_ReadBuf;

    SendIO((struct IORequest *) req);
}

VOID ContinueRead(
    struct Remote *r)
{
    struct IOExtSer *req = r->r_ReadReq;
    UWORD     size;

    r->r_BytesRead += req->IOSer.io_Actual;

    req->IOSer.io_Command = SDCMD_QUERY;

    DoIO((struct IORequest *) req);

    size = r->r_ReadBufSize - r->r_BytesRead;

    if(req->IOSer.io_Actual == 0 || size == 0) {
        return;
    }

    if(size > req->IOSer.io_Actual) {
        size = req->IOSer.io_Actual;
    }

    req->IOSer.io_Command = CMD_READ;
    req->IOSer.io_Length = size;
    req->IOSer.io_Data = r->r_ReadBuf + r->r_BytesRead;

    DoIO((struct IORequest *) req);

    r->r_BytesRead += size;
}

APTR __saveds __asm REM_CreateRemote(
    VOID)
{
    struct Remote *r;

    if(r = AllocMem(sizeof(struct Remote), MEMF_ANY)) {

        r->r_ReadPort = NULL;
        r->r_WritePort = NULL;
        r->r_ReadReq = NULL;
        r->r_WriteReq = NULL;
        r->r_ReadBuf = NULL;
        r->r_ReadBufSize = 0;
        r->r_BytesRead = 0;
        r->r_ReadReady = FALSE;
        r->r_WriteReady = FALSE;

        r->r_Device = NULL;
        r->r_Unit = DEF_UNIT;
        r->r_Baud = DEF_BAUD;
        r->r_DataBits = DEF_DATABITS;
        r->r_Parity = DEF_PARITY;
        r->r_StopBits = DEF_STOPBITS;
    }

    return r;
}

VOID __saveds __asm REM_DestroyRemote(
    register __a0 APTR remote)
{
    struct Remote *r = (struct Remote *) remote;

    if(r->r_Device) {
        FreeMem(r->r_Device, strlen(r->r_Device) + 1);
    }

    FreeMem(remote, sizeof(struct Remote));
}

struct RemoteArgs *__saveds __asm REM_GetArgTemplate(
    register __a0 APTR remote)
{
//    struct Remote *r = (struct Remote *)remote;
    struct RemoteArgs *ra;
    LONG     *argVals;

    if(!(ra = AllocMem(sizeof(struct RemoteArgs) + (sizeof(LONG) * ARG_COUNT), MEMF_ANY))) {
        return NULL;
    }

    argVals = (LONG *) (ra + 1);

    argVals[ARG_DEVICE] = (LONG) SERIALNAME;
    argVals[ARG_UNIT] = DEF_UNIT;
    argVals[ARG_BAUD] = DEF_BAUD;
    argVals[ARG_DATABITS] = DEF_DATABITS;
    argVals[ARG_PARITY] = (LONG) DEF_PARITY;
    argVals[ARG_STOPBITS] = DEF_STOPBITS;

    ra->ra_Template = ARG_TEMPLATE;
    ra->ra_ArgCount = ARG_COUNT;
    ra->ra_ArgValues = argVals;

    return ra;
}

BOOL __saveds __asm REM_ConfigureRemote(
    register __a0 APTR remote,
    register __a1 struct RemoteArgs * ra)
{
    struct Remote *r = (struct Remote *) remote;
    BOOL      result = TRUE;

    if(ra->ra_ArgValues[ARG_DEVICE]) {
        if(!(r->r_Device = AllocMem(strlen((STRPTR) ra->ra_ArgValues[ARG_DEVICE]) + 1, MEMF_ANY))) {
            result = FALSE;
            goto done;
        }

        strcpy(r->r_Device, (STRPTR) ra->ra_ArgValues[ARG_DEVICE]);
    }

    if(ra->ra_ArgValues[ARG_UNIT]) {
        r->r_Unit = (UWORD) * (LONG *) ra->ra_ArgValues[ARG_UNIT];
    }

    if(ra->ra_ArgValues[ARG_BAUD]) {
        r->r_Baud = (UWORD) * (LONG *) ra->ra_ArgValues[ARG_BAUD];
    }

    if(ra->ra_ArgValues[ARG_DATABITS]) {
        r->r_DataBits = (UWORD) * (LONG *) ra->ra_ArgValues[ARG_DATABITS];
    }

    if(ra->ra_ArgValues[ARG_PARITY]) {
        r->r_Parity = (BOOL) ra->ra_ArgValues[ARG_PARITY];
    }

    if(ra->ra_ArgValues[ARG_STOPBITS]) {
        r->r_StopBits = (UWORD) * (LONG *) ra->ra_ArgValues[ARG_STOPBITS];
    }

  done:
    FreeMem(ra, sizeof(struct RemoteArgs) + (sizeof(LONG) * ARG_COUNT));

    return result;
}


BOOL __saveds __asm REM_OpenRemote(
    register __a0 APTR remote)
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

                    if(OpenDevice(r->r_Device ? r->r_Device : (STRPTR) SERIALNAME, r->r_Unit, (struct IORequest *) writeReq, 0) == 0) {

                        writeReq->IOSer.io_Command = SDCMD_SETPARAMS;
                        if(r->r_Parity) {
                            writeReq->io_SerFlags |= SERF_PARTY_ON;
                        } else {
                            writeReq->io_SerFlags &= ~SERF_PARTY_ON;
                        }
                        writeReq->io_SerFlags |= SERF_XDISABLED;
                        writeReq->io_Baud = r->r_Baud;
                        writeReq->io_ReadLen = r->r_DataBits;
                        writeReq->io_WriteLen = r->r_DataBits;
                        writeReq->io_StopBits = r->r_StopBits;

                        DoIO((struct IORequest *) writeReq);

                        writeReq->IOSer.io_Command = CMD_FLUSH;
                        DoIO((struct IORequest *) writeReq);

                        CopyMem(writeReq, readReq, sizeof(struct IOExtSer));
                        readReq->IOSer.io_Message.mn_ReplyPort = readPort;

                        r->r_ReadPort = readPort;
                        r->r_WritePort = writePort;
                        r->r_ReadReq = readReq;
                        r->r_WriteReq = writeReq;
                        r->r_ReadBuf = NULL;
                        r->r_ReadBufSize = 0;
                        r->r_BytesRead = 0;
                        r->r_ReadReady = TRUE;
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

VOID REM_DeletePort(
    struct MsgPort * port)
{

    struct Message *msg;

    if(port) {
        while(msg = GetMsg(port)) {
            ReplyMsg(msg);
        }

        if(port->mp_Node.ln_Name) {
            DeletePort(port);
        } else {
            DeleteMsgPort(port);
        }
    }
}


VOID __saveds __asm REM_CloseRemote(
    register __a0 APTR remote)
{
    struct Remote *r = (struct Remote *) remote;

    if(r->r_ReadReq) {

        AbortIO((struct IORequest *) r->r_ReadReq);
        WaitIO((struct IORequest *) r->r_ReadReq);

        REM_DeletePort(r->r_ReadPort);
        DeleteExtIO((struct IORequest *) r->r_ReadReq);
    }

    if(r->r_WriteReq) {

        AbortIO((struct IORequest *) r->r_WriteReq);
        WaitIO((struct IORequest *) r->r_WriteReq);

        CloseDevice((struct IORequest *) r->r_WriteReq);

        REM_DeletePort(r->r_WritePort);
        DeleteExtIO((struct IORequest *) r->r_WriteReq);
    }

    r->r_ReadPort = NULL;
    r->r_WritePort = NULL;
    r->r_ReadReq = NULL;
    r->r_WriteReq = NULL;
    r->r_ReadReady = FALSE;
    r->r_WriteReady = FALSE;
    r->r_ReadBuf = NULL;
}

ULONG __saveds __asm REM_GetSignalBits(
    register __a0 APTR remote)
{
    struct Remote *r = (struct Remote *) remote;

    if(r->r_ReadPort == NULL || r->r_WritePort == NULL) {
        return 0;
    }

    return (1U << r->r_ReadPort->mp_SigBit)
        | (1U << r->r_WritePort->mp_SigBit);
}

VOID __saveds __asm REM_HandleSignal(
    register __a0 APTR remote,
    register __d0 ULONG signalBits)
{
    struct Remote *r = (struct Remote *) remote;

    if(signalBits & (1 << r->r_ReadPort->mp_SigBit)) {

        WaitIO((struct IORequest *) r->r_ReadReq);

        ContinueRead(r);

        r->r_ReadReady = TRUE;
    }

    if(signalBits & (1 << r->r_WritePort->mp_SigBit)) {

        WaitIO((struct IORequest *) r->r_WriteReq);

        r->r_WriteReady = TRUE;
    }
}

BOOL __saveds __asm REM_CanRead(
    register __a0 APTR remote)
{
    struct Remote *r = (struct Remote *) remote;

    return r->r_ReadReady;
}

VOID __saveds __asm REM_Read(
    register __a0 APTR remote,
    register __a1 BYTE * data,
    register __d0 UWORD length)
{
    struct Remote *r = (struct Remote *) remote;

    r->r_ReadBuf = data;
    r->r_ReadBufSize = length;
    r->r_BytesRead = 0;

    r->r_ReadReady = FALSE;

    BeginRead(r);
}

UWORD __saveds __asm REM_BytesRead(
    register __a0 APTR remote)
{
    struct Remote *r = (struct Remote *) remote;

    return r->r_BytesRead;
}

BOOL __saveds __asm REM_CanWrite(
    register __a0 APTR remote)
{
    struct Remote *r = (struct Remote *) remote;

    return r->r_WriteReady;
}

VOID __saveds __asm REM_Write(
    register __a0 APTR remote,
    register __a1 BYTE * data,
    register __d0 UWORD length)
{
    struct Remote *r = (struct Remote *) remote;
    struct IOExtSer *req = (struct IOExtSer *) r->r_WriteReq;

    if(data == NULL || length == 0) {
        return;
    }

    req->IOSer.io_Command = CMD_WRITE;
    req->IOSer.io_Length = length;
    req->IOSer.io_Data = data;

    r->r_WriteReady = FALSE;

    SendIO((struct IORequest *) req);
}
