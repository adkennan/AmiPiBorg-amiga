#ifndef __APB_CONFIG_H__
#define __APB_CONFIG_H__

#include "remote.h"
#include "remote_protos.h"
#include "remote_pragmas.h"

#include <exec/types.h>

struct Config {
    APTR      cf_Ctx;
    UWORD     cf_Size;
    STRPTR    cf_RemoteName;
    STRPTR    cf_RemoteArgs;
};

struct Config *APB_GetConfig(
    APTR ctx);

VOID      APB_FreeConfig(
    struct Config *cfg);

BOOL      APB_ConfigureRemote(
    struct Config *cfg,
    Remote rem);

#endif
