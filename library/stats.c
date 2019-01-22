
#include "lib.h"

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>


struct StatNode {
    struct Node sn_Node;
    LONG      sn_Value;
};

struct StatCollector {
    struct List sc_Stats;
    APTR      sc_MemPool;
    UWORD     sc_StatTypeSum;
    UWORD     sc_StatTypeCount;
    UWORD     sc_StatTypeMean;
};

#define STAT_LIST(sc) (&sc->sc_Stats)

APTR APB_CreateStatCollector(
    APTR memPool)
{

    struct StatCollector *sc;

    if(sc = APB_AllocMemInternal(memPool, sizeof(struct StatCollector))) {

        NewList(STAT_LIST(sc));
        sc->sc_MemPool = memPool;
        sc->sc_StatTypeSum = 0;
        sc->sc_StatTypeCount = 0;
        sc->sc_StatTypeMean = 0;
    }

    return sc;
}

VOID APB_FreeStatCollector(
    APTR statCollector)
{

    struct StatCollector *sc = (struct StatCollector *) statCollector;
    struct StatNode *sn;

    while(!IsListEmpty(STAT_LIST(sc))) {
        sn = (struct StatNode *) RemHead(STAT_LIST(sc));
        APB_FreeMemInternal(sc->sc_MemPool, sn, sizeof(struct StatNode));
    }

    APB_FreeMemInternal(sc->sc_MemPool, sc, sizeof(struct StatCollector));
}

struct StatNode *APB_FindStatNode(
    APTR ctx,
    UWORD statType)
{

    struct StatCollector *sc = (struct StatCollector *) ((struct ApbContext *) ctx)->ctx_StatColl;
    struct StatNode *sn;

    if(statType >= sc->sc_StatTypeMean) {

        for(sn = (struct StatNode *) STAT_LIST(sc)->lh_Tail; sn->sn_Node.ln_Pred; sn = (struct StatNode *) sn->sn_Node.ln_Pred) {
            if(sn->sn_Node.ln_Pri == statType) {
                return sn;
            }
        }

    } else {
        for(sn = (struct StatNode *) STAT_LIST(sc)->lh_Head; sn->sn_Node.ln_Succ; sn = (struct StatNode *) sn->sn_Node.ln_Succ) {
            if(sn->sn_Node.ln_Pri == statType) {
                return sn;
            }
        }
    }

    return NULL;
}

VOID __asm __saveds APB_RegisterStat(
    register __a0 APTR ctx,
    register __d0 UWORD statType)
{
    struct StatCollector *sc = (struct StatCollector *) ((struct ApbContext *) ctx)->ctx_StatColl;
    struct StatNode *sn;

    if(sn = APB_AllocMemInternal(sc->sc_MemPool, sizeof(struct StatNode))) {
        sn->sn_Node.ln_Pri = statType;
        sn->sn_Value = 0;

        sc->sc_StatTypeSum += statType;
        sc->sc_StatTypeCount++;

        sc->sc_StatTypeMean = sc->sc_StatTypeSum / sc->sc_StatTypeCount;

        Enqueue(STAT_LIST(sc), sn);
    }
}

VOID __asm __saveds APB_IncrementStat(
    register __a0 APTR ctx,
    register __d0 UWORD statType,
    register __d1 LONG value)
{
    struct StatNode *sn;

    if(sn = APB_FindStatNode(ctx, statType)) {
        sn->sn_Value += value;
    }
}

LONG __asm __saveds APB_GetStat(
    register __a0 APTR ctx,
    register __d0 UWORD statType)
{
    struct StatNode *sn;

    if(sn = APB_FindStatNode(ctx, statType)) {
        return sn->sn_Value;
    }

    return 0;
}
