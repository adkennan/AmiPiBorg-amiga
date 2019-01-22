
#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/io.h>
#include <exec/types.h>
#include <exec/ports.h>
#include <devices/timer.h>
#include <utility/date.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>
#include <clib/utility_protos.h>

#define HANDLER_ID 2

struct Library *AmiPiBorgBase;
struct Library *UtilityBase;

#define ARG_TEMPLATE "L=LOGLEVEL/K"
#define ARGS_LEN 1

WORD GetLogLevel(
    VOID)
{
    struct RDArgs *rd;
    WORD      logLevel = -1;

    LONG      argArr[ARGS_LEN] = {
        (LONG) "E"
    };

    if(!(rd = ReadArgs(ARG_TEMPLATE, argArr, NULL))) {
        printf("Invalid Log Level. Valid values are E, I, D or T.\n\nFormat: ");
        printf(ARG_TEMPLATE);
        printf("\n");

        goto done;
    }

    logLevel = APB_GetLogLevel((STRPTR) argArr[0]);

  done:
    if(rd) {
        FreeArgs(rd);
    }

    return logLevel;
}

int main(
    int argc,
    char **argv)
{
    APTR      ctx;
    APTR      conn;
    struct APBRequest *reader;
    struct MsgPort *port;
    struct Message *msg;
    ULONG     sig;
    ULONG     time = 0;
    struct timerequest *timerIO;
    struct ClockData cd;

    WORD      logLevel = GetLogLevel();

    if(logLevel < 0) {
        return;
    }

    if(port = CreatePort(NULL, 0)) {

        if(AmiPiBorgBase = OpenLibrary(APB_LibName, 1)) {

            if(ctx = APB_CreateContext(NULL, Output(), logLevel)) {

                if(conn = APB_AllocConnection(ctx, port, HANDLER_ID)) {

                    if(reader = APB_AllocRequest(conn)) {

                        reader->r_Data = &time;
                        reader->r_Length = sizeof(ULONG);
                        reader->r_Timeout = 5;

                        if(APB_OpenConnection(conn)) {

                            APB_Read(reader);

                            sig = Wait((1 << port->mp_SigBit) | SIGBREAKF_CTRL_C);

                            while(msg = GetMsg(port)) {
                                if(msg != (struct Message *) reader) {
                                    ReplyMsg(msg);
                                }
                            }

                            APB_CloseConnection(conn);
                        } else {

                            printf("Failed to connect: %d\n", APB_ConnectionState(conn));
                        }

                        APB_FreeRequest(reader);
                    }

                    APB_FreeConnection(conn);
                }

                APB_FreeContext(ctx);
            }

            CloseLibrary(AmiPiBorgBase);
        }

        if(time == 0) {
            printf("Unabled to get updated date and time.\n");
        } else if(timerIO = (struct timerequest *) CreateExtIO(port, sizeof(struct timerequest))) {

            if(OpenDevice(TIMERNAME, UNIT_VBLANK, (struct IORequest *) timerIO, 0L) == 0) {

                timerIO->tr_node.io_Command = TR_SETSYSTIME;
                timerIO->tr_time.tv_micro = 0;
                timerIO->tr_time.tv_secs = time;

                DoIO((struct IORequest *) timerIO);

                CloseDevice((struct IORequest *) timerIO);
            }

            DeleteExtIO((struct IORequest *) timerIO);

            if(UtilityBase = OpenLibrary("utility.library", 1)) {

                Amiga2Date(time, &cd);

                printf("%02d/%02d/%04d %02d:%02d:%02d\n", cd.mday, cd.month, cd.year, cd.hour, cd.min, cd.sec);

                CloseLibrary(UtilityBase);
            }
        }

        DeletePort(port);
    }
}
