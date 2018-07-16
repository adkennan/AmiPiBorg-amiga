
#include "server.h"
#include "stats.h"

#include <clib/exec_protos.h>

#include <stdio.h>

struct Library *DosBase;

void __regargs _CXBRK(
    void)
{
    return;
}

int main(
    int argc,
    char **argv)
{
    Server    srv;

    if(DosBase = OpenLibrary("dos.library", 34)) {

        if(srv = APB_CreateServer()) {

            APB_Run(srv);

            APB_DestroyServer(srv);

            APB_DumpStats();
        }

        CloseLibrary(DosBase);

    } else {
        printf("Can't open dos.library >= 34\n");
    }

    return 0;
}
