
#include "server.h"
#include "stats.h"

#include <stdio.h>


void __regargs _CXBRK(void) {
    return;
}

int main(int argc, char **argv)
{
    Server srv;
    
    if( srv = APB_CreateServer() ) {

        APB_Run(srv);

        printf("Destroying Server\n");

        APB_DestroyServer(srv);

        printf("Done.\n");
    }

    APB_DumpStats();

    return 0;
}