
#include "lib.h"

APTR __asm APB_CreateContext(
	register __a0 APTR memPool,
    register __a1 BPTR logFh,
	register __d0 UWORD logLevel
)
{
    struct ApbContext *ctx;

    if( ctx = APB_AllocMemInternal(memPool, sizeof(struct ApbContext)) ) {
        
        ctx->ctx_MemPool = memPool;

        if( ctx->ctx_Logger = APB_CreateLogger(memPool, logFh, logLevel) ) {

            if( ctx->ctx_ObjPool = APB_CreateObjectPool(memPool, ctx->ctx_Logger) ) {

                if( ctx->ctx_StatColl = APB_CreateStatCollector(memPool) ) { 

                    return ctx;
                }
            }
        }
    }

    APB_FreeContext(ctx);
    return NULL;
}

VOID __asm __saveds APB_FreeContext(
	register __a0 APTR context
)
{
    struct ApbContext *ctx = (struct ApbContext *)context;

    if( ctx->ctx_StatColl ) {
        APB_FreeStatCollector(ctx->ctx_StatColl);
    }

    if( ctx->ctx_ObjPool ) {
        APB_FreeObjectPool(ctx->ctx_ObjPool);
    }

    if( ctx->ctx_Logger ) {
        APB_FreeLogger(ctx->ctx_Logger);
    }

    APB_FreeMem(ctx, ctx, sizeof(struct ApbContext));
}
