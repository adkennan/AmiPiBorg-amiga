
#ifndef __AMIPIBORG_H__
#define __AMIPIBORG_H__

#include <exec/types.h>
#include <exec/ports.h>

#define PORT_NAME "AmiPiBorg"

enum RequestState {
    APB_RS_OK = 1,
    APB_RS_FAILED,
    APB_RS_NOT_CONNECTED,
    APB_RS_NO_HANDLER,
	APB_RS_NO_CONNECTION,
    APB_RS_NO_SERVER,
    APB_RS_ABORTED,
	APB_RS_BUFFER_TOO_SMALL
};

enum RequestType {
    APB_RT_OPEN = 1,
    APB_RT_CLOSE,
    APB_RT_READ,
    APB_RT_WRITE
};

struct APBRequest {
	struct Message r_Msg;
    UWORD r_Type;
	UWORD r_ConnId;
    UWORD r_HandlerId;
	UWORD r_State;
	UWORD r_Actual;
	UWORD r_Length;
	APTR r_Data;
};

#endif // __AMIPIBORG_H__
