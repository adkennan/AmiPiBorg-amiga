
#include "lib.h"

#include <exec/lists.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

// Object Type

struct ObjectType {
    struct MinNode ot_Node;
    struct MinList ot_Instances;
    UWORD     ot_Id;
    STRPTR    ot_Name;
    UWORD     ot_Size;
    UWORD     ot_MinInstances;
    UWORD     ot_MaxInstances;
    UWORD     ot_InstanceCount;
    UWORD     ot_InUseCount;
};

#define INST_LIST(ot) ((struct List *)&ot->ot_Instances)

#define OBJ_TO_NODE(obj) (struct MinNode *)APB_PointerAdd(obj, -sizeof(struct MinNode))
#define NODE_TO_OBJ(obj) APB_PointerAdd(obj, sizeof(struct MinNode))

#define NODE_SIZE(ot) (ot->ot_Size + sizeof(struct MinNode))


struct ObjectPool {

    struct MinList op_Types;
    APTR op_MemPool;
    APTR op_Log;
};

#define TYPE_LIST(pool) ((struct List *)&pool->op_Types)

#define FOR_EACH_TYPE(pool, curr) for( curr = (struct ObjectType *)((pool)->op_Types.mlh_Head); \
                                       curr->ot_Node.mln_Succ; \
                                       curr = (struct ObjectType *)curr->ot_Node.mln_Succ )

VOID APB_DestroyInstance(
    struct ObjectPool *op,
    struct ObjectType *ot,
    struct MinNode *obj)
{
//    LOGI2(op->op_Log, LOG_TRACE, "Destroy Object 0x%00000000x, Type %s", obj, ot->ot_Name);

    APB_FreeMemInternal(op->op_MemPool, obj, NODE_SIZE(ot));

    ot->ot_InstanceCount--;

//    APB_IncrementStat(ST_OBJ_DESTROY_COUNT, 1);
//    APB_IncrementStat(ST_OBJ_INSTANCE_COUNT, -1);
}


VOID APB_DestroyObjectType(
    struct ObjectPool *op,
    struct ObjectType *ot)
{
    while(ot->ot_InstanceCount > 0) {
        APB_DestroyInstance(op, ot, (struct MinNode *) RemHead(INST_LIST(ot)));
    }

    if(ot->ot_InUseCount > 0) {
        LOGI2(op->op_Log, LOG_INFO, "WARNING: Object Type %s has %d unreleased instances.", ot->ot_Name, ot->ot_InUseCount);
    }

    LOGI1(op->op_Log, LOG_TRACE, "Destroy Object Type %s", ot->ot_Name);

    APB_FreeMemInternal(op->op_MemPool, ot, sizeof(struct ObjectType));
//    APB_IncrementStat(ST_OBJ_TYPE_COUNT, -1);
}

struct MinNode *APB_CreateInstance(
    struct ObjectPool *op,
    struct ObjectType *ot)
{
    struct MinNode *obj;

    if(ot->ot_InstanceCount >= ot->ot_MaxInstances) {
        LOGI2(op->op_Log, LOG_ERROR, "Type %s, limit of %d instances reached", ot->ot_Name, ot->ot_MaxInstances);
        return NULL;
    }

    if(!(obj = (struct MinNode *) APB_AllocMemInternal(op->op_MemPool, NODE_SIZE(ot)))) {
        LOGI1(op->op_Log, LOG_ERROR, "Unable to create instance of type %s", ot->ot_Name);
        return NULL;
    }

    ot->ot_InstanceCount++;

//    APB_IncrementStat(ST_OBJ_CREATE_COUNT, 1);
//    APB_IncrementStat(ST_OBJ_INSTANCE_COUNT, 1);

//      LOGI2(op->op_Log, LOG_TRACE, "Create Object 0x%00000000x, Type %s", obj, ot->ot_Name);

    return obj;
}

struct MinNode *APB_AllocInstance(
    struct ObjectPool *op,
    struct ObjectType *ot)
{
    struct MinNode *obj;

    if(ot->ot_InUseCount == ot->ot_InstanceCount) {

        if(!(obj = APB_CreateInstance(op, ot))) {
            return NULL;
        }

    } else {

        obj = (struct MinNode *) RemHead(INST_LIST(ot));

    }

    ot->ot_InUseCount++;

//    APB_IncrementStat(ST_OBJ_ALLOC_COUNT, 1);
//    APB_IncrementStat(ST_OBJ_CURRENT_ALLOCATED, 1);

//    LOGI2(op->op_Log, LOG_TRACE, "Allocate Object 0x%00000000x, Type %s", obj, ot->ot_Name);

    return obj;
}

VOID APB_ReleaseInstance(
    struct ObjectPool *op,
    struct ObjectType * ot,
    struct MinNode * obj)
{
//    APB_IncrementStat(ST_OBJ_RELEASE_COUNT, 1);
//    APB_IncrementStat(ST_OBJ_CURRENT_ALLOCATED, -1);

    if(ot->ot_InstanceCount > ot->ot_MinInstances) {

        APB_DestroyInstance(op, ot, obj);

    } else {

        AddTail(INST_LIST(ot), (struct Node *) obj);
    }

    LOGI2(op->op_Log, LOG_TRACE, "Release Object 0x%00000000x, Type %s", obj, ot->ot_Name);

    ot->ot_InUseCount--;
}

// Object Pool

APTR APB_CreateObjectPool(
    APTR memPool,
    APTR log
)
{
    struct ObjectPool *op;

    if(!(op = APB_AllocMemInternal(memPool, sizeof(struct ObjectPool)))) {
        LOGI0(log, LOG_ERROR, "Unable to create object pool");
        return NULL;
    }

    NewList(TYPE_LIST(op));
    op->op_MemPool = memPool;
    op->op_Log = log;

    return op;
}

VOID APB_FreeObjectPool(
    APTR pool)
{
    struct ObjectPool *op = (struct ObjectPool *) pool;
    struct ObjectType *ot;

    while(!IsListEmpty(TYPE_LIST(op))) {
        ot = (struct ObjectType *) RemTail(TYPE_LIST(op));
        APB_DestroyObjectType(op, ot);
    }
    
    APB_FreeMemInternal(op->op_MemPool, op, sizeof(struct ObjectPool));
}

struct ObjectType *APB_GetObjectType(
    struct ObjectPool *op,
    UWORD id)
{
    struct ObjectType *ot;

    FOR_EACH_TYPE(op, ot) {

        if(ot->ot_Id == id) {

            return ot;
        }
    }
    return NULL;
}

BOOL APB_TypeRegisteredInternal(
    APTR pool,
    UWORD id)
{
    struct ObjectPool *op = (struct ObjectPool *) pool;
    struct ObjectType *ot;

    if(ot = APB_GetObjectType(op, id)) {
        return TRUE;
    }
    return FALSE;
}


VOID APB_RegisterObjectTypeInternal(
    APTR pool,
    UWORD id,
    STRPTR name,
    UWORD size,
    UWORD minInstances,
    UWORD maxInstances)
{
    struct ObjectPool *op = (struct ObjectPool *) pool;
    struct ObjectType *ot;
    struct MinNode *obj;

    if(ot = APB_GetObjectType(op, id)) {
        LOGI2(op->op_Log, LOG_ERROR, "Type id %d exists as %s.", id, ot->ot_Name);
        return;
    }

    if(!(ot = APB_AllocMemInternal(op->op_MemPool, sizeof(struct ObjectType)))) {
        LOGI1(op->op_Log, LOG_ERROR, "Cannot create type %s", name);
        return;
    }

    ot->ot_Id = id;
    ot->ot_Name = name;
    ot->ot_Size = size;
    ot->ot_MinInstances = minInstances;
    ot->ot_MaxInstances = maxInstances;
    ot->ot_InstanceCount = 0;
    ot->ot_InUseCount = 0;

    NewList(INST_LIST(ot));

    LOGI1(op->op_Log, LOG_TRACE, "Register Object Type %s", ot->ot_Name);

    while(ot->ot_InstanceCount < ot->ot_MinInstances) {
        if(!(obj = APB_CreateInstance(op, ot))) {
            break;
        }
        AddTail(INST_LIST(ot), (struct Node *) obj);
    }

    AddTail(TYPE_LIST(op), (struct Node *) &ot->ot_Node);

//    APB_IncrementStat(ST_OBJ_TYPE_COUNT, 1);
}

APTR APB_AllocObjectInternal(
    APTR pool,
    UWORD id)
{
    struct ObjectPool *op = (struct ObjectPool *) pool;
    struct ObjectType *ot;
    struct MinNode *obj;

    if(ot = APB_GetObjectType(op, id)) {

        if(obj = APB_AllocInstance(op, ot)) {

            return NODE_TO_OBJ(obj);
        }
    }

    return NULL;
}

VOID APB_FreeObjectInternal(
    APTR pool,
    UWORD id,
    APTR obj)
{
    struct ObjectPool *op = (struct ObjectPool *) pool;
    struct ObjectType *ot;

    if(ot = APB_GetObjectType(op, id)) {

        APB_ReleaseInstance(op, ot, OBJ_TO_NODE(obj));
    }
}



BOOL __asm __saveds APB_TypeRegistered(
	register __a0 APTR ctx,
    register __d0 UWORD id)
{
    struct ObjectPool *op = (struct ObjectPool *)((struct ApbContext *)ctx)->ctx_ObjPool;

    return APB_TypeRegisteredInternal(op, id);
}


VOID __asm __saveds APB_RegisterObjectType(
	register __a0 APTR ctx,
    register __d0 UWORD id,
    register __a1 STRPTR name,
    register __d1 UWORD size,
    register __d2 UWORD minInstances,
    register __d3 UWORD maxInstances)
{
    struct ObjectPool *op = (struct ObjectPool *)((struct ApbContext *)ctx)->ctx_ObjPool;

    APB_RegisterObjectTypeInternal(op, id, name, size, minInstances, maxInstances);
}


APTR __asm __saveds APB_AllocObject(
	register __a0 APTR ctx,
    register __d0 UWORD id)
{
    struct ObjectPool *op = (struct ObjectPool *)((struct ApbContext *)ctx)->ctx_ObjPool;

    return APB_AllocObjectInternal(op, id);
}

VOID __asm __saveds APB_FreeObject(
	register __a0 APTR ctx,
    register __d0 UWORD id,
    register __a1 APTR obj)
{
    struct ObjectPool *op = (struct ObjectPool *)((struct ApbContext *)ctx)->ctx_ObjPool;

    APB_FreeObjectInternal(op, id, obj);
}

