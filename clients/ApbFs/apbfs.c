
#include "apbfs.h"
#include "amipiborg_protos.h"

#include <clib/exec_protos.h>

struct Library *DosBase;

struct Library *AmiPiBorgBase;

int main(int argc, char** argv)
{
    struct ApbFs *device;

    if( DosBase = OpenLibrary(DOSNAME, 37) ) {

        if( AmiPiBorgBase = OpenLibrary(APB_LibName, 1) ) {

            if( device = FS_CreateDevice() ) {

                FS_Run(device);

                FS_FreeDevice(device);
            }

            CloseLibrary(AmiPiBorgBase);
        }

        CloseLibrary(DosBase);
    }    
}
