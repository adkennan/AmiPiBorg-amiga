
#include "event.h"

#include "log.h"

#include <exec/lists.h>
#include <exec/memory.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>

UWORD ets[ET_COUNT] = { ET_LOG, ET_CONN, ET_STAT };

struct EventBroker 
{
    MemoryPool eb_MemPool;
    ObjectPool eb_ObjPool;
    struct MinList eb_Subs;    
    UWORD eb_SubCount[ET_COUNT];
};

struct Subscriber
{
    struct MinNode s_Node;
    UWORD s_EventTypes;
    EventHandler s_Handler;
    APTR s_Object;
};

struct Event
{   
    struct EventBroker *e_Broker;
    UWORD e_Type;
    UWORD e_Length;
    BYTE *e_Data;
    UWORD e_SubCount;
};

#define EV_DATA_SIZE 128

#define SUB_LIST(eb) ((struct List *)&((eb)->eb_Subs))
#define SUB_NODE(sub) ((struct Node *)&((sub)->s_Node))

EventBroker APB_CreateEventBroker(MemoryPool memPool, ObjectPool objPool)
{
    struct EventBroker *eb;
    UWORD ix;

    if( ! (eb = APB_AllocMem(memPool, sizeof(struct EventBroker) ) ) ) {
        return NULL;
    }

    eb->eb_MemPool = memPool;
    eb->eb_ObjPool = objPool;

    NewList(SUB_LIST(eb));

    for( ix = 0; ix < ET_COUNT; ix++ ) {
        eb->eb_SubCount[ix] = 0;
    }

    if( ! APB_TypeRegistered(objPool, OT_EVENT_SUB) ) {
        APB_RegisterObjectType(objPool, OT_EVENT_SUB, sizeof(struct Subscriber), 4, 16);
    }

    if( ! APB_TypeRegistered(objPool, OT_EVENT) ) {
        APB_RegisterObjectType(objPool, OT_EVENT, sizeof(struct Event) + EV_DATA_SIZE, 8, 32);
    }

    if( ! APB_TypeRegistered(objPool, OT_EVENT_NODE) ) {
        APB_RegisterObjectType(objPool, OT_EVENT_NODE, sizeof(struct EventNode), 8, 32);
    }

    return eb;
}

VOID APB_DestroyEventBroker(EventBroker eventBroker)
{
    struct EventBroker *eb = (struct EventBroker *)eventBroker;
    struct Subscriber *sub;

    while( ! IsListEmpty(SUB_LIST(eb)) ) {
        sub = (struct Subscriber *)RemHead(SUB_LIST(eb));
        APB_FreeObject(eb->eb_ObjPool, OT_EVENT_SUB, sub);
    }
}

ULONG APB_AddSubscriber(EventBroker eventBroker, UWORD eventTypes, APTR obj, EventHandler handler)
{
    struct EventBroker *eb = (struct EventBroker *)eventBroker;
    UWORD ix;
    struct Subscriber *sub;

    if( eventTypes == 0 || handler == NULL ) {
        return 0;
    }

    if( ! (sub = APB_AllocObject(eb->eb_ObjPool, OT_EVENT_SUB) ) ) {
        return 0;
    }

    sub->s_EventTypes = eventTypes;
    sub->s_Object = obj;
    sub->s_Handler = handler;

    AddTail(SUB_LIST(eb), SUB_NODE(sub));

    for( ix = 0; ix < ET_COUNT; ix++ ) {            
        if( eventTypes & ets[ix] ) {
            eb->eb_SubCount[ix]++;
        }
    }    

    return (ULONG)sub;
}

VOID APB_RemoveSubscriber(EventBroker eventBroker, ULONG handle)
{
    struct EventBroker *eb = (struct EventBroker *)eventBroker;
    struct Subscriber *sub = (struct Subscriber *)handle;
    UWORD ix;

    if( sub ) {
        Remove(SUB_NODE(sub));

        for( ix = 0; ix < ET_COUNT; ix++ ) {            
            if( sub->s_EventTypes & ets[ix] ) {
                eb->eb_SubCount[ix]--;
            }
        }    

        APB_FreeObject(eb->eb_ObjPool, OT_EVENT_SUB, sub);
    }
}

BOOL APB_EmitEvent(EventBroker eventBroker, UWORD eventType, BYTE *data, UWORD length)
{
    struct EventBroker *eb = (struct EventBroker *)eventBroker;
    struct Event *ev;
    struct EventNode *en;
    struct Subscriber *sub;
    UWORD ix, sc = 0;

    for( ix = 0; ix < ET_COUNT; ix++ ) {            
        if( eventType == ets[ix] ) {
            sc = eb->eb_SubCount[ix];
            break;
        }
    }    

    if( sc == 0 ) {
        return TRUE;
    }

    while( length > 0 ) {
        if( ! (ev = APB_AllocObject(eb->eb_ObjPool, OT_EVENT) ) ) {
            return FALSE;
        }

        ev->e_Broker = eb;
        ev->e_Type = eventType;
        ev->e_SubCount = sc;
        ev->e_Data = APB_PointerAdd(ev, sizeof(struct Event));

        if( length > EV_DATA_SIZE ) {
            ev->e_Length = EV_DATA_SIZE;
            CopyMem(data, ev->e_Data, EV_DATA_SIZE);
            length -= EV_DATA_SIZE;
        } else {
            ev->e_Length = length;
            CopyMem(data, ev->e_Data, length);
            length = 0;
        }

        for( sub = (struct Subscriber *)SUB_LIST(eb)->lh_Head;
             SUB_NODE(sub)->ln_Succ;
             sub = (struct Subscriber *)SUB_NODE(sub)->ln_Succ ) {

            if( sub->s_EventTypes & eventType ) {

                if( en = APB_AllocObject(eb->eb_ObjPool, OT_EVENT_NODE) ) {
    
                    en->en_Event = en;

                    sub->s_Handler(sub->s_Object, en);
                }
            }
        }    
    }

    return TRUE;
}


UWORD APB_GetEventType(Event e)
{
    return ((struct Event *)e)->e_Type;
}

UWORD APB_GetEventSize(Event e)
{
    return ((struct Event *)e)->e_Length;
}

VOID APB_CopyEvent(Event e, BYTE *dest)
{
    struct Event *ev = (struct Event *)e;
    CopyMem(ev->e_Data, dest, ev->e_Length);
}

VOID APB_FreeEventNode(struct EventNode *en)
{
    struct Event *ev = (struct Event *)en->en_Event;

    ev->e_SubCount--;
    
    if( ev->e_SubCount == 0 ) {
        APB_FreeObject(ev->e_Broker->eb_ObjPool, OT_EVENT, ev);
    }

    APB_FreeObject(ev->e_Broker->eb_ObjPool, OT_EVENT_NODE, en);
}


