
#ifndef __APB_SERVER_H__
#define __APB_SERVER_H__

#include <exec/types.h>

typedef APTR Server;


Server    APB_CreateServer(
    VOID);

VOID      APB_DestroyServer(
    Server srv);

VOID      APB_Run(
    Server srv);

VOID      APB_DeletePort(
    struct MsgPort *port);

#endif // __APB_SERVER_H__
