
#include <stdio.h>

#include "stats.h"


const char *StatNames[] = {

    "Memory - Allocation Count",
    "Memory - Free Count",
    "Memory - Total Allocated",
    "Memory - Total Freed",
    "Memory - Current Allocated",

    "Objects - Type Count",
    "Objects - Create Count",
    "Objects - Destroy Count",
    "Objects - Allocate Count",
    "Objects - Release Count",
    "Objects - Instance Count",
    "Objects - Currently Allocated",
    
    "Buffers - Allocated",
    "Buffers - Allocation Failures",

    "Buffer Refs - Allocated",
    "Buffer Refs - Allocation Failures",
    
    "Packet Refs - Allocated",
    "Packet Refs - Allocation Failures",
    "Packet Refs - Split Packets",

    "Queues - Incoming Queue Size",
    "Queues - Outgoing Queue Size",

    "Packets - Sent",
    "Packets - Received",

    "Bytes - Sent",
    "Bytes - Received",

    "Connections - Created",
    "Connections - Successful",
    "Connections - Failed",
    "Connections - Current",

    NULL    
};


LONG _statCounters[ST_END];

VOID APB_IncrementStat(UWORD statType, LONG value)
{
    if( statType >= ST_END ) {
        return;
    }

    _statCounters[statType] += value;
}

VOID APB_DumpStats(VOID) 
{
    UWORD stat = 0;
    
    printf("Statistics:\n");

    while( stat < ST_END ) {

        printf("\t%s:\t\t%ld\n", StatNames[stat], _statCounters[stat]);

        stat++;
    }
}
