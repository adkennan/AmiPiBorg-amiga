
#include "memory.h"
#include "objectpool.h"
#include "stats.h"

#include <exec/lists.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

#include <stdio.h>

// Object Type


struct ObjectType {
    struct MinNode  ot_Node;
    struct MinList  ot_Instances;
	UWORD			ot_Id;
    UWORD           ot_Size;
    UWORD           ot_MinInstances;
    UWORD           ot_MaxInstances;
	UWORD			ot_InstanceCount;
    UWORD           ot_InUseCount;
};

#define INST_LIST(ot) ((struct List *)&ot->ot_Instances)

#define OBJ_TO_NODE(obj) (struct MinNode *)(((BYTE *)obj) - sizeof(struct MinNode))
#define NODE_TO_OBJ(obj) (APTR)(((BYTE *)obj) + sizeof(struct MinNode))

#define NODE_SIZE(ot) (ot->ot_Size + sizeof(struct MinNode))


struct ObjectPool {

    struct MinList op_Types;
	MemoryPool	   op_MemPool;
};

#define TYPE_LIST(pool) ((struct List *)&pool->op_Types)

#define FOR_EACH_TYPE(pool, curr) for( curr = (struct ObjectType *)((pool)->op_Types.mlh_Head); \
                                       curr->ot_Node.mln_Succ; \
                                       curr = (struct ObjectType *)curr->ot_Node.mln_Succ )

VOID APB_DestroyInstance(MemoryPool memPool, struct ObjectType *ot, struct MinNode *obj)
{
    APB_FreeMem(memPool, obj, NODE_SIZE(ot));

    ot->ot_InstanceCount--;

    APB_IncrementStat(ST_OBJ_DESTROY_COUNT, 1);
    APB_IncrementStat(ST_OBJ_INSTANCE_COUNT, -1);
}


VOID APB_DestroyObjectType(MemoryPool memPool, struct ObjectType *ot)
{
    while( ot->ot_InstanceCount > 0 ) {
        APB_DestroyInstance(memPool, ot, (struct MinNode *)RemHead(INST_LIST(ot)));
    }

    if( ot->ot_InUseCount > 0 ) {
        printf("WARNING: Object Type %d has %d unreleased instances.\n", ot->ot_Id, ot->ot_InUseCount);
    }

    APB_FreeMem(memPool, ot, sizeof(struct ObjectType));
    APB_IncrementStat(ST_OBJ_TYPE_COUNT, -1);
}

struct MinNode *APB_CreateInstance(MemoryPool memPool, struct ObjectType *ot)
{
    struct MinNode *obj;

    if( ot->ot_InstanceCount >= ot->ot_MaxInstances ) {
        return NULL; // No more objects allowed.
    }

    if( ! (obj = (struct MinNode *)APB_AllocMem(memPool, NODE_SIZE(ot) ) ) ) {
        return NULL; // No more RAM!
    }

    ot->ot_InstanceCount++;

    APB_IncrementStat(ST_OBJ_CREATE_COUNT, 1);
    APB_IncrementStat(ST_OBJ_INSTANCE_COUNT, 1);

    return obj;
}

struct MinNode *APB_AllocInstance(MemoryPool memPool, struct ObjectType *ot)
{
    struct MinNode *obj;

    if( ot->ot_InUseCount == ot->ot_InstanceCount ) {

        if( !(obj = APB_CreateInstance(memPool, ot) ) ) {
        
            // Can't allocate an object.
            return NULL;
        }                    

    } else {
    
        obj = (struct MinNode *)RemHead(INST_LIST(ot));

    }
    
    ot->ot_InUseCount++;

    APB_IncrementStat(ST_OBJ_ALLOC_COUNT, 1);
    APB_IncrementStat(ST_OBJ_CURRENT_ALLOCATED, 1);

    return obj;        
}

VOID APB_ReleaseInstance(MemoryPool memPool, struct ObjectType *ot, struct MinNode *obj)
{
    APB_IncrementStat(ST_OBJ_RELEASE_COUNT, 1);
    APB_IncrementStat(ST_OBJ_CURRENT_ALLOCATED, -1);

    if( ot->ot_InstanceCount > ot->ot_MinInstances ) {

        APB_DestroyInstance(memPool, ot, obj);

    } else {

        AddTail(INST_LIST(ot), (struct Node *)obj);
    }

    ot->ot_InUseCount--;
}

// Object Pool


ObjectPool APB_CreateObjectPool(MemoryPool memPool)
{
    struct ObjectPool *p;

    if( ! (p = APB_AllocMem(memPool, sizeof(struct ObjectPool)) ) ) {
        return NULL;
    }

    NewList(TYPE_LIST(p));
    p->op_MemPool = memPool;

    return p;
}

VOID APB_DestroyObjectPool(ObjectPool pool)
{
    struct ObjectPool *op = (struct ObjectPool *)pool;
    struct ObjectType *ot;

    while( ! IsListEmpty(TYPE_LIST(op)) ) {
        ot = (struct ObjectType *)RemTail(TYPE_LIST(op));
        APB_DestroyObjectType(op->op_MemPool, ot);
    }

    APB_FreeMem(op->op_MemPool, op, sizeof(struct ObjectPool));    
}

struct ObjectType *APB_GetObjectType(struct ObjectPool *op, UWORD id)
{
    struct ObjectType *ot;

    FOR_EACH_TYPE(op, ot) {

        if( ot->ot_Id == id ) {
            
            return ot;
        }
    }
    return NULL;
}

BOOL APB_TypeRegistered(ObjectPool pool, UWORD id)
{
    struct ObjectPool *op = (struct ObjectPool *)pool;
    struct ObjectType *ot;

    if( ot = APB_GetObjectType(op, id) ) {
        return TRUE;
    }
    return FALSE;
}


VOID APB_RegisterObjectType(ObjectPool  pool
						  , UWORD       id
						  , UWORD       size
    					  , UWORD       minInstances
						  , UWORD       maxInstances)
{
    struct ObjectPool *op = (struct ObjectPool *)pool;
    struct ObjectType *ot;
    struct MinNode *obj;

    if( ot = APB_GetObjectType(op, id) ) {
        printf("Type %d exists.\n", id);
        return;
    }
    
    if( !(ot = APB_AllocMem(op->op_MemPool, sizeof(struct ObjectType) ) ) ) {

        return;
    }
    
    ot->ot_Id               = id;
    ot->ot_Size             = size;
    ot->ot_MinInstances     = minInstances;
    ot->ot_MaxInstances     = maxInstances;
    ot->ot_InstanceCount    = 0;
    ot->ot_InUseCount       = 0;    

    NewList(INST_LIST(ot));

    while(ot->ot_InstanceCount < ot->ot_MinInstances ) {
        if( !(obj = APB_CreateInstance(op->op_MemPool, ot) ) ) {
            break;
        }
        AddTail(INST_LIST(ot), (struct Node *)obj);
    }

    AddTail(TYPE_LIST(op), (struct Node *)&ot->ot_Node);    

    APB_IncrementStat(ST_OBJ_TYPE_COUNT, 1);
}

APTR APB_AllocObject(ObjectPool pool, UWORD id)
{
    struct ObjectPool *op = (struct ObjectPool *)pool;
    struct ObjectType *ot;
    struct MinNode *obj;

    if( ot = APB_GetObjectType(op, id) ) {       

        if( obj = APB_AllocInstance(op->op_MemPool, ot) ) {

            return NODE_TO_OBJ(obj);
        }
    }
   
    return NULL;
}

VOID APB_FreeObject(ObjectPool pool, UWORD id, APTR obj)
{
    struct ObjectPool *op = (struct ObjectPool *)pool;
    struct ObjectType *ot;

    if( ot = APB_GetObjectType(op, id) ) {

       APB_ReleaseInstance(op->op_MemPool, ot, OBJ_TO_NODE(obj));
    }
}


