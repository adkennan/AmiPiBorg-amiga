
#include "remote.h"

#include <devices/serial.h>

#include <clib/alib_protos.h>

BOOL APB_OpenDevice(struct Remote *r)
{
    struct IOExtSer *readReq;
    struct IOExtSer *writeReq;
    struct MsgPort  *readPort;
    struct MsgPort  *writePort;

    if( readPort = CreateMsgPort() ) {
        
        if( writePort = CreateMsgPort() ) {

            if( readReq = (struct IOExtSer *)CreateExtIO(readPort, sizeof(struct IOExtSer)) ) {

                if( writeReq = (struct IOExtSer *)CreateExtIO(writePort, sizeof(struct IOExtSer)) ) {

                    if( OpenDevice(SERIALNAME, 0, (struct IORequest *)readReq, 0) == 0 ) {


                        CopyMem(readReq, writeReq, sizeof(IOExtSer));
                        writeReq->IOSer.io_Message.mn_ReplyPort = writePort;

                        r->r_ReadPort = readPort;
                        r->r_WritePort = writePort;
                        r->r_ReadReq = readReq;
                        r->r_WriteReq = writeReq;

                        NewList((struct List *)r->r_ReqQueue);

                        r->r_ReadReady = TRUE;
                        r->r_WriteReady = TRUE;

                        return TRUE;
                    }

                    DeleteExtIO(writeReq);
                }

                DeleteExtIO(readReq);
            }

            DeleteMsgPort(writePort);
        }

        DeleteMsgPort(readPort);
    }

    return FALSE;
    
}

VOID APB_CloseDevice(struct Remote *r)
{
    if( r->r_ReadReq ) {
        CloseDevice(r->r_ReadReq);
        DeleteExtIO(r->r_ReadReq);
        r->r_ReadReq = NULL;
    }

    if( r->r_WriteReq ) {
        DeleteExtIO(r->r_WriteReq);
        r->r_WriteReq = NULL;
    }

    if( r->r_ReadPort ) {
        DeleteMsgPort(r->r_ReadPort);
        r->r_ReadPort = NULL;
    }

    if( r->r_WritePort) {
        DeleteMsgPort(r->r_WritePort);
        r->r_WritePort = NULL;
    }          
}

VOID APB_SendToRemote(struct Remote *r, struct Request *req)
{

}

VOID APB_WriteComplete(struct Remote *r)
{

}

VOID APB_ReadComplete(struct Server *srv, struct Remote *r)
{

}

