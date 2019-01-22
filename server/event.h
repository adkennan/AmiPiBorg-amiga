#ifndef __APB_EVENT_H__
#define __APB_EVENT_H__

#include <exec/types.h>
#include <exec/lists.h>

#include "memory.h"
#include "objectpool.h"

typedef APTR EventBroker;

typedef APTR Event;

struct EventNode {
    struct MinNode en_Node;
    Event     en_Event;
};

typedef   VOID(
    *EventHandler) (
    APTR,
    struct EventNode *);

enum EventType {
    ET_LOG = 1,
    ET_CONN = 2,
    ET_STAT = 4
};

#define ET_COUNT 3

EventBroker APB_CreateEventBroker(
    MemoryPool memPool,
    ObjectPool objPool);

VOID      APB_DestroyEventBroker(
    EventBroker eb);

ULONG     APB_AddSubscriber(
    EventBroker eb,
    UWORD eventTypes,
    APTR obj,
    EventHandler handler);

VOID      APB_RemoveSubscriber(
    EventBroker eb,
    ULONG handle);

BOOL      APB_EmitEvent(
    EventBroker eb,
    UWORD eventType,
    BYTE * data,
    UWORD length);

UWORD     APB_GetEventType(
    Event e);

UWORD     APB_GetEventSize(
    Event e);

VOID      APB_CopyEvent(
    Event e,
    BYTE * dest);

VOID      APB_FreeEventNode(
    struct EventNode *en);

#endif
