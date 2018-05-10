
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

    "In Buffers - Allocated",
    "In Buffers - Allocation Failures",
    
    "In Packets - Allocated",
    "In Packets - Allocation Failures",
    "In Packets - Split Packets",
    "In Packets - Invalid Data",

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
    if( statType < ST_MEM_ALLOC_COUNT || statType >= ST_END ) {
        return;
    }

    _statCounters[statType - 1] += value;
}

VOID APB_DumpStats(VOID) 
{
    UWORD stat = ST_MEM_ALLOC_COUNT;
    
    printf("Statistics:\n");

    while( stat < ST_END ) {

        printf("\t%s:\t\t%ld\n", StatNames[stat - 1], _statCounters[stat - 1]);

        stat++;
    }
}
