
#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/ports.h>

#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#define HANDLER_ID 1

#define BUF_SIZE 64

struct Library *AmiPiBorgBase;
struct Library *DosBase;

#define ARG_TEMPLATE "L=LOGLEVEL/K,R=REPEAT/K/N,MSG/F"
#define ARG_LOGLEVEL 0
#define ARG_REPEAT 1
#define ARG_MSG 2

VOID ShowError(
    STRPTR msg)
{
    if(msg) {
        PutStr(msg);
        PutStr("\n");
    }

    PutStr("Format: ");
    PutStr(ARG_TEMPLATE);
    PutStr("\n");
}

struct RDArgs *ParseArgs(
    LONG * argData,
    UWORD * logLevel,
    LONG * repeat,
    UBYTE ** msg)
{
    struct RDArgs *rd;

    if(!(rd = ReadArgs(ARG_TEMPLATE, argData, NULL))) {
        ShowError("Invalid Arguments");
        goto error;
    }

    *logLevel = APB_GetLogLevel((STRPTR) argData[ARG_LOGLEVEL]);
    if(*logLevel < 0) {
        ShowError("Invalid Log Level. Valid values are E, I, D or T.");
        goto error;
    }

    *repeat = *(LONG *) argData[ARG_REPEAT];
    if(*repeat < 1) {
        ShowError("Repeat must be greater than 0.");
        goto error;
    }

    *msg = (UBYTE *) argData[ARG_MSG];

    return rd;

  error:
    if(rd) {
        FreeArgs(rd);
    }

    return NULL;
}

int main(
    int argc,
    char **argv)
{
    APTR      ctx;
    APTR      conn;
    struct APBRequest *reader;
    struct APBRequest *writer;
    UBYTE     readBuf[BUF_SIZE];
    struct MsgPort *port;
    ULONG     sig;
    struct Message *msg;
    ULONG     count = 0, len, actual;

    struct RDArgs *rd;

    UBYTE    *writeBuf;
    LONG      repeat = 5;
    UWORD     logLevel;

    LONG      argData[] = {
        (LONG) "E",
        0,
        (LONG) "Ping!"
    };
    argData[ARG_REPEAT] = (LONG) & repeat;

    if(DosBase = OpenLibrary("dos.library", 34)) {

        if(rd = ParseArgs(argData, &logLevel, &repeat, &writeBuf)) {

            len = strlen(writeBuf);

            if(AmiPiBorgBase = OpenLibrary(APB_LibName, 1)) {

                if(ctx = APB_CreateContext(NULL, Output(), logLevel)) {

                    if(port = CreatePort(NULL, 0)) {

                        if(conn = APB_AllocConnection(ctx, port, HANDLER_ID)) {

                            if(reader = APB_AllocRequest(conn)) {

                                reader->r_Data = &readBuf;
                                reader->r_Length = BUF_SIZE;
                                reader->r_Timeout = 5;

                                if(writer = APB_AllocRequest(conn)) {

                                    writer->r_Data = writeBuf;
                                    writer->r_Length = len;
                                    writer->r_Timeout = 5;

                                    if(APB_OpenConnection(conn)) {

                                        printf("Sending %s\n", writer->r_Data);

                                        APB_Write(writer);

                                        while(count < repeat) {

                                            sig = Wait((1 << port->mp_SigBit) | SIGBREAKF_CTRL_C);

                                            if(sig & (1 << port->mp_SigBit)) {

                                                while(msg = GetMsg(port)) {

                                                    if(msg == (struct Message *) writer) {

                                                        if(writer->r_State != APB_RS_OK) {
                                                            printf("Write failed: %d\n", writer->r_State);
                                                            count = repeat;
                                                            continue;
                                                        }

                                                        printf("%d: Sent %d bytes: %s \n", count, writer->r_Actual, writer->r_Data);

                                                        APB_Read(reader);
                                                    }

                                                    else if(msg == (struct Message *) reader) {

                                                        if(reader->r_State != APB_RS_OK) {
                                                            printf("Read failed: %d\n", reader->r_State);
                                                            count = repeat;
                                                            continue;
                                                        }

                                                        actual = reader->r_Actual > len ? len : reader->r_Actual;
                                                        readBuf[actual] = 0;
                                                        if(actual != writer->r_Length) {
                                                            printf("Expected %ld bytes, got %ld.\n", len, actual);
                                                        }
                                                        if(strncmp(writeBuf, readBuf, actual) != 0) {
                                                            printf("!!! Read bytes differ !!!\n");
                                                        }
                                                        printf("%d: Received %d bytes: %s\n", count, reader->r_Actual, readBuf);

                                                        //Delay(50);

                                                        count++;
                                                        if(count < repeat) {
                                                            APB_Write(writer);
                                                        }

                                                    } else {

                                                        ReplyMsg(msg);

                                                    }
                                                }
                                            }

                                            if(sig & SIGBREAKF_CTRL_C) {

                                                break;

                                            }

                                        }

                                        while(msg = GetMsg(port)) {
                                            if(msg != (struct Message *) reader && msg != (struct Message *) writer) {
                                                ReplyMsg(msg);
                                            }
                                        }

                                        APB_CloseConnection(conn);

                                    } else {

                                        printf("Failed to connect: %d\n", APB_ConnectionState(conn));
                                    }

                                    APB_FreeRequest(writer);
                                }

                                APB_FreeRequest(reader);
                            }

                            APB_FreeConnection(conn);
                        }

                        DeletePort(port);
                    }
                    APB_FreeContext(ctx);
                }

                CloseLibrary(AmiPiBorgBase);
            }

            FreeArgs(rd);
        }

        CloseLibrary(DosBase);
    }
}
