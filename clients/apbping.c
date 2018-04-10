
#include "amipiborg.h"
#include "amipiborg_protos.h"
#include "amipiborg_pragmas.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <exec/types.h>
#include <exec/ports.h>

#include <dos/dos.h>

#include <clib/exec_protos.h>
#include <clib/alib_protos.h>
#include <clib/dos_protos.h>

#define HANDLER_ID 1

#define BUF_SIZE 64

struct Library *AmiPiBorgBase;

int main(int argc, char **argv)
{
    APTR conn;
    struct APBRequest *reader;
    struct APBRequest *writer;
    UBYTE readBuf[BUF_SIZE];
    UBYTE *writeBuf;
    struct MsgPort *port;
    ULONG sig;
    struct Message *msg;
    ULONG repeat = 5, count = 0, len, actual;
    
    if( argc > 1 ) {
        writeBuf = argv[1];
    } else {
        writeBuf = "Ping!";
    }
	len = strlen(writeBuf);

    if( argc > 2 ) {
        repeat = atol(argv[2]);
    }

    if( AmiPiBorgBase = OpenLibrary(APB_LibName, 1) ) {

        if( port = CreatePort(NULL, 0) ) {

            if( conn = APB_AllocConnection(port, HANDLER_ID, NULL) ) {

                if( APB_OpenConnection(conn) ) {

                    printf("Connected!\n");

                    if( reader = APB_AllocRequest(conn) ) {

                        reader->r_Data = &readBuf;
                        reader->r_Length = BUF_SIZE;

                        if( writer = APB_AllocRequest(conn) ) {

                            writer->r_Data = writeBuf;
                            writer->r_Length = len;

                            printf("Sending %s\n", writer->r_Data);

                            APB_Write(writer);

                            while(count < repeat) {

                                sig = Wait((1 << port->mp_SigBit) |  SIGBREAKF_CTRL_C );

                                if( sig & (1 << port->mp_SigBit) ) {

                                    while( msg = GetMsg(port) ) {

                                        if( msg == (struct Message *)writer ) {

											if( writer->r_State != APB_RS_OK ) {
												printf("Write failed: %d\n", writer->r_State);
												count = repeat;
												continue;
											}

                                            printf("%d: Sent %d bytes: %s \n", count, writer->r_Actual, writer->r_Data);

                                            APB_Read(reader);
                                        }

                                        else if( msg == (struct Message *)reader ) {

											if( reader->r_State != APB_RS_OK ) {
												printf("Read failed: %d\n", reader->r_State);
												count = repeat;
												continue;
											}

											actual = reader->r_Actual > len 
														? len
														: reader->r_Actual;
                                            readBuf[actual] = 0;
											if( actual != writer->r_Length ) {
												printf("Expected %ld bytes, got %ld.\n", len, actual);
											}
											if( strncmp(writeBuf, readBuf, actual) != 0 ) {
												printf("!!! Read bytes differ !!!\n");		
											}
                                            printf("%d: Received %d bytes: %s\n", count, reader->r_Actual, readBuf);

                                            Delay(50);

                                            APB_Write(writer);

                                            count++;

                                        } else {

                                            ReplyMsg(msg);

                                        }
                                    }
                                }

                                if( sig & SIGBREAKF_CTRL_C ) {

                                    break;

                                }

                            }

                            printf("Cleaning up\n");
                            while( msg = GetMsg(port) ) {
                                if( msg != (struct Message *)reader 
                                 && msg != (struct Message *)writer ) {
                                    ReplyMsg(msg);
                                }
                            }

                            APB_FreeRequest(writer);
                        }

                        APB_FreeRequest(reader);
                    }
        
                    printf("Closing connection\n");

                    APB_CloseConnection(conn);

                } else {

                    printf("Failed to connect: %d\n", APB_ConnectionState(conn));
                }

                printf("Free connection\n");
                APB_FreeConnection(conn);
            }

            DeletePort(port);
        }
        
        CloseLibrary(AmiPiBorgBase);
    }
}
