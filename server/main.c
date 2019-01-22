
#include "server.h"
#include "amipiborg.h"

#include <utility/utility.h>

#include <clib/exec_protos.h>

#include <stdio.h>

struct Library *DosBase;
struct Library *UtilityBase;
struct Library *AmiPiBorgBase;

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

        if(UtilityBase = OpenLibrary(UTILITYNAME, 37)) {

            if(AmiPiBorgBase = OpenLibrary(APB_LibName, 1)) {

                if(srv = APB_CreateServer()) {

                    APB_Run(srv);

                    APB_DestroyServer(srv);
                }

                CloseLibrary(AmiPiBorgBase);

            } else {
                printf("Can't open %s >= 1\n", APB_LibName);
            }

            CloseLibrary(UtilityBase);

        } else {
            printf("Can't open %s >= 37\n", UTILITYNAME);
        }

        CloseLibrary(DosBase);

    } else {
        printf("Can't open dos.library >= 34\n");
    }

    return 0;
}
