# AmiPiBorg-amiga
Let your Amiga share Raspberry Pi resources - Amiga side.

AmiPiBorg extends an Amiga by pairing it with a Raspberry Pi. 

A server process runs on both the Amiga and Pi exchanging packets of data over a link between the Amiga and the Pi. The goal is to use the fastest available connection such as the Amiga's parallel port but for ease of development it currently supports only the serial port.

At startup the Amiga will request a list of capabilities from the Pi. Each capability is provided by a piece of software called a Handler which runs on the Pi. 

A Client application on the Amiga requests a connection to a Handler on the Pi and, if the Pi supports that Handler, a Connection is established between them. Once established the Client and Handler can exchange data as streams of bytes.

Terms
=====

* Amiga - the AmiPiBorg server application running on the Amiga.
* Raspberry Pi or just Pi - The AmiPiBorg server application running on the Rasperry Pi
* Client - an application running on the Amiga that communicates with a Handler.
* Handler - software running on the Raspberry Pi that services requests from a Client.
* Connection - a bi-directional stream of data between a Client and Handler.
* Packet - a message exchanged between the Amiga and Pi.
* Device - the driver for the device used to communicate between the Amiga and Pi. E.g serial.device.

Protocol
========

Packet Structure
----------------

All data exchanged between the Amiga and Raspberry Pi is organized into packets. Each packet consists of a header describing the type of packet, the Connection it belongs to, it's length and other useful information. The structure of the packet looks like this:
 
```
Byte  Length  Description
-------------------------
0     4       Packet start identifier ASCII `AmPi` 0x416d5069
4     1       Packet type. See packet types below.
5     1       Flags. See flags below.
6     2       Connection identifier. 0 for control messages.
8     2       Packet identifier.
10    2       Checksum. 
12    2       Data length.
14    -       Data.
```

The packet header can be represented as a C struct like this:

```
struct Packet 
{
	ULONG pac_Id;
	UBYTE pac_Type;
	UBYTE pac_Flags;
	UWORD pac_ConnId;
	UWORD pac_PackId;
	UWORD pac_Checksum;
	UWORD pac_Length;
};
```

With pac_Length bytes of data following immediately.

Packet Types
------------

* 0x00 - Init - Sent by Amiga to Pi. 
* 0x01 - Hello - Sent by Pi to Amiga. Payload contains list of Handlers.
* 0x02 - Shutdown - Causes receiver to shut down all connections and exit.
* 0x04 - Goodbye - Response to Shutdown.

* 0x10 - Connect - Opens a new connection. Payload contains ID of Handler.
* 0x11 - Connected - Response to the Connect message.
* 0x12 - Disconnect - Closes a connection.
* 0x13 - Disconnected - Connection closed.

* 0x20 - Data - Connection specific handler data.
* 0x22 - Resend - Request the re-sending of a packet.
* 0x23 - Ping - Sent from the Amiga to check status of connection.
* 0x24 - Pong - Sent from the Pi as a response to a ping.

* 0x30 - Error - An error response.

Packet Structures
-----------------

Some packets contain additional data for managing communications between
the Amiga and Pi.

### Init
```
struct PacketInit 
{
	struct Packet pi_Pac;
	UWORD pi_Version;     	// Version of AmiPiBorg running on Amiga.
	UWORD pi_MaxPacketSize	// Maximum size of a packet supported by the server.
};
```

### Hello

This packet contains a list of Handler descriptions. Each description consists
of a HandlerId and a NULL terminated string.

```
struct HandlerDesc
{
	UWORD hd_HandlerId;		// ID of the Hander.
	UBYTE hd_HandlerName;	// NULL-terminated string containing the Handler 
							// name.
};

struct PacketHello
{
	struct Packet ph_Pac;
	UWORD ph_Version; 		// Version of AmiPiBord running on Raspberry Pi.
	UWORD ph_HandlerCount;	// Number of handlers.
	struct HandlerDesc ph_HandlerDesc;	// First handler. 
};
```

### Connect

```
struct PacketConnect 
{
	struct Packet pc_Pac;
	UWORD pc_HandlerId;		// Id of the Handler to connect to.
};
```

### Resend

```
struct PacketResponse
{
	struct Packet pr_Pac;
	UWORD pr_PacketId;		// Id of the Packet being responded to.
};
```

### Error

```
struct PacketError
{
	struct Packet pe_Pac;
	UWORD pe_PacketId;		// Id of the Packet causing the error.
	UWORD pe_ErrorCode;		// Type of error.
	UBYTE pe_Message;		// NULL-terminated error message string.
};
```

Packet Flags
------------

* Bit 0 - A padding byte has been added to the data to make it an even length.
* Bit 1 - The packet is a re-send.

Amiga Client Library
====================

Clients running on the Amiga communicate using a library interface. The API sort of resembles the Exec device API.

Structures
----------

A Connection represents the Client's connection to the Handler on the Pi. The connection object does not expose any public attributes so only an APTR is returned from OpenConnection.

A Request represents an asynchronous I/O operation between the Client and Handler. Requests must always be created with APB_AllocRequest and destroyed with APB_FreeRequest.

```
struct APBRequest
{
	struct Message r_Msg;
   	UWORD r_Type;				// The type of request.
	UWORD r_ConnId;				// The ID of this connection.
   	UWORD r_HandlerId;			// The ID of the handler on the Pi side.
	UWORD r_State;				// The currenct state of the request. See the RequestState enum.
	UWORD r_Actual;				// The actual number of bytes read or written.
	UWORD r_Length;				// The length of the data to be read or written.
	UWORD r_Timeout;			// The number of seconds a request can be enqueued with the server before it times out.
	APTR r_Data;				// The data to be written or a buffer to receive data.
};
```

Enumerations
------------

```
enum RequestState {
    APB_RS_OK = 1,				// Request completed successfully.
    APB_RS_FAILED,				// Request failed.
    APB_RS_NOT_CONNECTED,		// Client is not connected to the server.
    APB_RS_NO_HANDLER,			// Invalid handler id.
    APB_RS_NO_CONNECTION,		// Invalid connection id.
    APB_RS_NO_SERVER,			// Server could not be contacted.
    APB_RS_ABORTED,				// Request was aborted.
    APB_RS_BUFFER_TOO_SMALL,	// Read buffer is too small for a packet received from the Pi.
    APB_RS_TIMEOUT				// The request timed out.
};
```
```
enum RequestType {
    APB_RT_OPEN = 1,			// Open the connection.
    APB_RT_CLOSE,				// Close the connection.
    APB_RT_READ,				// Read data.
    APB_RT_WRITE				// Write data.
};
```

Library Functions
-----------------

*  APTR APB_AllocConnection(struct MsgPort *port, UWORD handlerId, APTR memoryPool)
*  BOOL APB_OpenConnection(APTR conn) 
*  VOID APB_CloseConnection(APTR conn)
*  UWORD APB_ConnectionState(APTR conn)
*  struct APBRequest *APB_AllocRequest(APTR conn)
*  VOID APB_FreeRequest(struct APBRequest *req)
*  VOID APB_Write(struct APBRequest *req)
*  VOID APB_Read(struct APBRequest *req)
*  VOID APB_Abort(struct APBRequest *req)

### APB_AllocConnection
 
```
 APTR APB_AllocConnection(struct MsgPort *port, UWORD handlerId, APTR memoryPool)
```
 
 Constructs a new Connection object using the port and handlerId. If memoryPool is not NULL it will be used for all memory allocations related to the connection.
 
 Returns a pointer to the Connection object.

### APB_OpenConnection

``` 
BOOL APB_OpenConnection(APTR conn) 
```

Establishes the communication channel between the connection and the handler. Returns TRUE for success or FALSE for failure. A failure reason can be determined with the APB_ConnectionState function.

### APB_CloseConnection

``` 
VOID APB_CloseConnection(APTR conn)
```

Closes the supplied connection. There must be no Requests pending when the connection is closed.

### APB_ConnectionState

```
UWORD APB_ConnectionState(APTR conn);
```

Returns the current state of the connection. Contains the same values as the Request r_State field.

### APB_AllocRequest

```
struct APBRequest *APB_AllocRequest(APTR conn)
```

Constructs and initializes a new Request object for sending or receiving data. Multiple requests can be active at any time.

### APB_FreeRequest

```
VOID APB_FreeRequest(struct APBRequest *req)
```

Deallocates a request. The request must not be involved in a Read or Write operation at the time it is freed.

### APB_Write

```
VOID APB_Write(struct APBRequest *req)
```

Initiates a write operation using the supplied request. When data has been sent to the Handler the request will be returned to the Connection's message port.

The request must not be modified while a Write operation is pending.

### APB_Read

```
VOID APB_Read(struct Request *req)
```

Initiates a read operation using the supplied request. When data has been received from the Handler or it times out the request will be returned to the Connection's message port.

The request must not be modified while a Read operation is pending.

### APB_Abort

```
VOID APB_Abort(struct Request *req)
```

Aborts a request that has been sent from a client to the server.

Amiga Side Server
=================

The server application running on the Amiga side will receive messages from Clients and enqueue them for sending asynchronously to the Pi. Messages from Handlers will be delivered to a message port on the Client.

Start Up
--------

The server will open the Device used to communicate with the Pi and send an Init message. It will then wait for a corresponding Hello from the Pi.

Upon establishing the connection to the Pi the Amiga will open a public message port named AmiPiBorg. The client library will send all requests to this port.

The server will then enter a running state and will listen for messages on both the client and device ports.

Message Passing
---------------

When a message is ready to be delivered to the Pi the server will check the state of the device. If it is ready to send messages and there are no messages already enqueued the message will be sent immediately. If the device is not ready due to a send currently in progress the message will be queued. When the device is once again ready to send the first message from the queue will be sent.

When a message is received from the Pi the connection it belongs to will be identified. If the connection is ready to receive, the message will be sent to it immediately otherwise it will be enqueued until the client performs a Read.

When an OpenConnection message is received from a Client the Server will create a new Connection object with a status of "Connecting" and send a  Connect message to the Pi.  The Pi will respond with either a Connected message in which case the connection status will be changed to Connected and the client can proceed with sending messages, or an Error message which will change the status to Disconnected.


