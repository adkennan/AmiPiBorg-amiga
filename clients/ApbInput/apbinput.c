

#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/io.h>
#include <exec/types.h>
#include <exec/ports.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <dos/dos.h>
#include <intuition/screens.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/utility_protos.h>
#include <clib/intuition_protos.h>

#define HANDLER_ID 3

#define EV_MOUSEMOVE 1
#define EV_MOUSEBTN 2
#define EV_KEYBOARD 4

struct Library *AmiPiBorgBase;
struct Library *IntuitionBase;

struct InputMsg {
	UWORD im_EventType;
	UWORD im_Data;
};

struct MsgConfig {
	UWORD mc_EventType;
};

struct MsgMouseMoveEvent {
	UWORD me_EventType;
	BYTE me_DeltaX;
	BYTE me_DeltaY;
};

struct MsgMouseBtnEvent {
	UWORD me_EventType;
	UWORD me_Button;
};

int main(int argc, char **argv)
{
    APTR conn;
    struct APBRequest *reader;
	struct APBRequest *writer;
    struct MsgPort *port;
	struct Message *msg;
    ULONG sig;
	UWORD eventType = EV_MOUSEMOVE | EV_MOUSEBTN;
	struct InputMsg ev;
	struct MsgMouseMoveEvent *evMM;
	struct MsgMouseBtnEvent *evMB;
	BOOL running = TRUE;
	struct IOStdReq *ioReq;
	struct InputEvent inputEv;
	struct IEPointerPixel ptr;
	struct Screen *screen;
	
	if( port = CreatePort(NULL, 0) ) {

		if( ioReq = CreateIORequest(port, sizeof(struct IOStdReq)) ) {

			if( ! OpenDevice("input.device", NULL, (struct IORequest *)ioReq, NULL) ) {

		if( IntuitionBase = OpenLibrary("intuition.library", 36) ) {

			if( AmiPiBorgBase = OpenLibrary(APB_LibName, 1) ) {

   		   	    if( conn = APB_AllocConnection(port, HANDLER_ID, NULL) ) {

       		       	if( reader = APB_AllocRequest(conn) ) {

						if( writer = APB_AllocRequest(conn) ) {

							if( APB_OpenConnection(conn) ) {

								reader->r_Length = sizeof(struct InputMsg);
								reader->r_Data = &ev;
								reader->r_Timeout = 5;
						
								writer->r_Length = 2;
								writer->r_Data = &eventType;
								writer->r_Timeout = 5;

								inputEv.ie_EventAddress = (APTR)&ptr;	
								inputEv.ie_NextEvent = NULL;
								inputEv.ie_Class = IECLASS_NEWPOINTERPOS;
								inputEv.ie_SubClass = IESUBCLASS_PIXEL;
								inputEv.ie_Code = 0;
								inputEv.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;

								ioReq->io_Data = (APTR)&inputEv;
								ioReq->io_Length = sizeof(struct InputEvent);
								ioReq->io_Command = IND_WRITEEVENT;

								inputEv.ie_NextEvent = NULL;

								APB_Write(writer);
								
								while( running ) {

	           	                    sig = Wait((1 << port->mp_SigBit) |  SIGBREAKF_CTRL_C );

                   	                while( msg = GetMsg(port) ) {

										if( msg == (struct Message *)reader ) {

											if( reader->r_State == APB_RS_OK ) {

												switch( ev.im_EventType ) {
													case EV_MOUSEMOVE:
														if( screen = LockPubScreen(NULL) ) {
															evMM = (struct MsgMouseMoveEvent *)&ev;
															inputEv.ie_EventAddress = (APTR)&ptr;	
															inputEv.ie_NextEvent = NULL;
															inputEv.ie_Class = IECLASS_NEWPOINTERPOS;
															inputEv.ie_SubClass = IESUBCLASS_PIXEL;
															inputEv.ie_Code = 0;
															inputEv.ie_Qualifier = IEQUALIFIER_RELATIVEMOUSE;

															ptr.iepp_Screen = screen;
															ptr.iepp_Position.X = evMM->me_DeltaX;
															ptr.iepp_Position.Y = evMM->me_DeltaY;

															DoIO((struct IORequest *)ioReq);

															UnlockPubScreen(NULL, screen);
														}
														break;
													
													case EV_MOUSEBTN:
														if( screen = LockPubScreen(NULL) ) {
															evMB = (struct MsgMouseBtnEvent *)&ev;

															inputEv.ie_EventAddress = NULL;
															inputEv.ie_Class = IECLASS_RAWMOUSE;
															inputEv.ie_SubClass = 0;
															inputEv.ie_Code = evMB->me_Button;
															inputEv.ie_Qualifier = 0;

															DoIO((struct IORequest *)ioReq);

															UnlockPubScreen(NULL, screen);
														}
														break;

												}
											} else if( reader->r_State != APB_RS_TIMEOUT ) {
												printf("Read error: %d\n", reader->r_State);
												running = FALSE;
												break;
											}

											APB_Read(reader);

										} else if( msg == (struct Message *)writer ) {

											if( writer->r_State != APB_RS_OK ) {		
												printf("Unable to configure remote: %d\n", writer->r_State);
												running = FALSE;
												break;
											}

											APB_Read(reader);									

										} else {
	
											ReplyMsg(msg);	
										}
									}

                                	if( sig & SIGBREAKF_CTRL_C ) {
                                    	break;
	                                }
								}

								APB_CloseConnection(conn);
							} else {
		    	                printf("Failed to connect: %d\n", APB_ConnectionState(conn));
							}

							APB_FreeRequest(writer);
						}

						APB_FreeRequest(reader);
					}
			
					APB_FreeConnection(conn);
				}

				CloseLibrary(AmiPiBorgBase);
			}

			CloseLibrary(IntuitionBase);
		}

			
				CloseDevice((struct IORequest *)ioReq);
			}

			DeleteIORequest(ioReq);
		}
		DeletePort(port);
	}
}