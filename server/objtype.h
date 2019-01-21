#ifndef __APB_OBJTYPE_H__
#define __APB_OBJTYPE_H__

enum ObjectTypes {
    OT_BUFFER = 100,
    OT_IN_PACKET = 200,
    OT_IN_BUFFER = 300,
    OT_CONNECTION = 400,
    OT_OUT_PACKET = 500,
    OT_OUT_BUFFER = 600,
    OT_EVENT_SUB = 700,
    OT_EVENT = 800,
    OT_EVENT_NODE = 900
};

#endif // __APB_OBJTYPE_H__
