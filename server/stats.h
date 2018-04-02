
#ifndef __APB_STATS_H__
#define __APB_STATS_H__

#include <exec/types.h>

enum StatTypes
{
    // Memory
    ST_MEM_ALLOC_COUNT,
    ST_MEM_FREE_COUNT,
    ST_MEM_TOTAL_ALLOCATED,
    ST_MEM_TOTAL_FREED,
    ST_MEM_CURRENT_ALLOCATED,

    // Objects
    ST_OBJ_TYPE_COUNT,
    ST_OBJ_CREATE_COUNT,
    ST_OBJ_DESTROY_COUNT,
    ST_OBJ_ALLOC_COUNT,
    ST_OBJ_RELEASE_COUNT,
    ST_OBJ_INSTANCE_COUNT,
    ST_OBJ_CURRENT_ALLOCATED,

    // Buffers
    ST_BUF_ALLOCATED,
    ST_BUF_ALLOC_FAILURES,
    
    // Buffer Refs
    ST_BR_ALLOCATED,
    ST_BR_ALLOC_FAILURES,

    // Packet Refs
    ST_PR_ALLOCATED,
    ST_PR_ALLOC_FAILURES,
    ST_PR_SPLIT_COUNT,

    // Queues   
    ST_Q_INCOMING_SIZE,
    ST_Q_OUTGOING_SIZE,

    // Packets
    ST_PAC_SENT,
    ST_PAC_RECEIVED,

    // Bytes
    ST_BYTES_SENT,
    ST_BYTES_RECEIVED,

    // Connections
    ST_CNN_CREATED,
    ST_CNN_SUCCESSES,
    ST_CNN_FAILURES,
    ST_CNN_CURRENT,


    ST_END
};

VOID APB_IncrementStat(UWORD statType, LONG value);

VOID APB_DumpStats(VOID);

#endif // __APB_STATS_H__