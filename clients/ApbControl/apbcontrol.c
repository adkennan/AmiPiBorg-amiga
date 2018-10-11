
#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>

#include <stdio.h>
#include <string.h>

struct Library *DosBase;
struct Library *AmiPiBorgBase;

#define ARG_TEMPLATE "Q=QUIT/S,LL=LOGLEVEL/K,L=LOG/S,S=STATS/S,C=CONNS/S"

#define ARG_QUIT 0
#define ARG_LOGLEVEL 1
#define ARG_LOG 2
#define ARG_STATS 3
#define ARG_CONNS 4

#define ARG_COUNT 5

#define DATA_BUF_LEN 1024

void __regargs _CXBRK(
    void)
{
    return;
}

WORD GetLogLevel(
    LONG value)
{

    STRPTR    ll = (STRPTR) value;

    switch (ll[0]) {
    case 'E':
        return 0;
    case 'I':
        return 5;
    case 'D':
        return 10;
    case 'T':
        return 20;
    default:
        return -1;
    }
}

VOID WriteLines(
    BYTE * data,
    UWORD length)
{
    UWORD     l;

    while(length > 0 && *data) {
        printf((STRPTR) data);
        printf("\n");
        l = strlen(data) + 1;
        length -= l;
        data += l;
    }
}

int main(
    int argc,
    char **argv)
{
    LONG      argArr[ARG_COUNT] = { 0 };
    struct RDArgs *rd;
    WORD      ll;
    BYTE     *data;
    UWORD     result = APB_RS_FAILED;

    if(DosBase = OpenLibrary("dos.library", 36)) {

        if(AmiPiBorgBase = OpenLibrary(APB_LibName, 1)) {


            if(rd = ReadArgs(ARG_TEMPLATE, argArr, NULL)) {

                if(argArr[ARG_QUIT]) {
                    printf("Requesting server quit.\n");

                    result = APB_SrvQuit();

                    goto done;
                }

                if(argArr[ARG_LOGLEVEL]) {

                    ll = GetLogLevel(argArr[ARG_LOGLEVEL]);
                    if(ll < 0) {
                        printf("ERROR: Valid log levels are E, I, D or T.\n");
                        goto done;
                    }

                    printf("Setting log level to %s\n", (STRPTR) argArr[ARG_LOGLEVEL]);

                    result = APB_SrvSetLogLevel((UWORD) argArr[ARG_LOGLEVEL]);

                    goto done;
                }

                if(argArr[ARG_LOG]) {
                    if(!(data = AllocMem(DATA_BUF_LEN, MEMF_ANY | MEMF_CLEAR))) {
                        printf("Failed to allocate buffer.\n");
                        goto done;
                    }

                    result = APB_SrvGetLog(data, DATA_BUF_LEN);

                    if(result == APB_RS_OK) {
                        WriteLines(data, DATA_BUF_LEN);
                    }

                    FreeMem(data, DATA_BUF_LEN);

                    goto done;
                }

                if(argArr[ARG_STATS]) {
                    if(!(data = AllocMem(DATA_BUF_LEN, MEMF_ANY | MEMF_CLEAR))) {
                        printf("Failed to allocate buffer.\n");
                        goto done;
                    }

                    result = APB_SrvGetStats(data, DATA_BUF_LEN);

                    if(result = APB_RS_OK) {
                        WriteLines(data, DATA_BUF_LEN);
                    }

                    FreeMem(data, DATA_BUF_LEN);

                    goto done;
                }

                if(argArr[ARG_CONNS]) {
                    if(!(data = AllocMem(DATA_BUF_LEN, MEMF_ANY | MEMF_CLEAR))) {
                        printf("Failed to allocate buffer.\n");
                        goto done;
                    }

                    result = APB_SrvGetConnections(data, DATA_BUF_LEN);

                    if(result = APB_RS_OK) {
                        WriteLines(data, DATA_BUF_LEN);
                    }

                    FreeMem(data, DATA_BUF_LEN);
                }
              done:
                FreeArgs(rd);
            } else {
                printf("Usage: %s\n", ARG_TEMPLATE);
            }

            if(result != APB_RS_OK) {
                printf("Request failed with result %d\n", result);
            }

            CloseLibrary(AmiPiBorgBase);
        }
        CloseLibrary(DosBase);
    }
}
