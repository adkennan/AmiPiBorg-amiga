#ifndef __APB_CONFIG_H__
#define __APB_CONFIG_H__

#include "memory.h"
#include "remote.h"
#include "remote_protos.h"
#include "remote_pragmas.h"

#include <exec/types.h>

struct Config {
    MemoryPool cf_MemPool;
    UWORD     cf_Size;
    UWORD     cf_LogLevel;
    UWORD     cf_LogToStdOut;
    STRPTR    cf_RemoteName;
    STRPTR    cf_RemoteArgs;
};

struct Config *APB_GetConfig(
    MemoryPool mp);

VOID      APB_FreeConfig(
    struct Config *cfg);

BOOL      APB_ConfigureRemote(
    struct Config *cfg,
    Remote rem);

#endif
