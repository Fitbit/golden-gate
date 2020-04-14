/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-10-17
 *
 * @details
 *
 * Example of a remote shell running over a UDP transport
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_memory.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/remote/gg_remote.h"
#include "xp/smo/gg_smo_allocator.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_RemoteTransport);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_DataSink);

    GG_SharedQueue* rx_queue;

    GG_DatagramSocket*       socket;
    GG_SocketAddressMetadata socket_info;
} UdpTransport;

typedef struct {
    GG_IMPLEMENTS(GG_RemoteCborHandler);

    GG_StaticBuffer canned_response;
} HelloWorldHandler;

typedef struct {
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    unsigned int counter;
} CounterHandler;

typedef struct {
    GG_LinkedListNode list_node;
    uint8_t*          payload;
    size_t            payload_size;
} DataQueueItem;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define UDP_REMOTE_SERVER_PORT 9000

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static GG_Result
UdpTransport_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    UdpTransport* self = GG_SELF(UdpTransport, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    printf("=== got data, size=%d\n", (int)GG_Buffer_GetDataSize(data));

    // copy the socket info on the first packet
    if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS &&
        self->socket_info.base.type == 0) {
        self->socket_info = *((const GG_SocketAddressMetadata*)metadata);
    }

    // make a copy of the data so we can send it to another thread
    // NOTE: this is just an example, not something one would do in a real
    // application
    DataQueueItem* item = (DataQueueItem*)GG_AllocateZeroMemory(sizeof(DataQueueItem));
    item->payload = (uint8_t*)GG_AllocateMemory(GG_Buffer_GetDataSize(data));
    item->payload_size = GG_Buffer_GetDataSize(data);
    memcpy(item->payload, GG_Buffer_GetData(data), item->payload_size);
    GG_SharedQueue_Enqueue(self->rx_queue, &item->list_node, GG_TIMEOUT_INFINITE);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
UdpTransport_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
UdpTransport_OnCanPut(GG_DataSinkListener* _self)
{
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
static GG_Result
UdpTransport_Send(GG_RemoteTransport* _self, GG_Buffer* data)
{
    UdpTransport* self = GG_SELF(UdpTransport, GG_RemoteTransport);
    GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(self->socket), data, &self->socket_info.base);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
UdpTransport_Receive(GG_RemoteTransport* _self, GG_Buffer** data)
{
    UdpTransport* self = GG_SELF(UdpTransport, GG_RemoteTransport);

    // default
    *data = NULL;

    // wait for a message
    GG_LinkedListNode* node = NULL;
    GG_Result result = GG_SharedQueue_Dequeue(self->rx_queue, &node, GG_TIMEOUT_INFINITE);
    if (GG_FAILED(result)) {
        return result;
    }

    DataQueueItem* item = GG_LINKED_LIST_ITEM(node, DataQueueItem, list_node);
    GG_DynamicBuffer* buffer = NULL;
    GG_DynamicBuffer_Create(item->payload_size, &buffer);
    GG_DynamicBuffer_SetData(buffer, item->payload, item->payload_size);
    *data = GG_DynamicBuffer_AsBuffer(buffer);
    GG_FreeMemory(item);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(UdpTransport, GG_DataSink) {
    UdpTransport_PutData,
    UdpTransport_SetListener
};

GG_IMPLEMENT_INTERFACE(UdpTransport, GG_DataSinkListener) {
    UdpTransport_OnCanPut
};

GG_IMPLEMENT_INTERFACE(UdpTransport, GG_RemoteTransport) {
    UdpTransport_Send,
    UdpTransport_Receive
};

//----------------------------------------------------------------------
static void
InitTransport(UdpTransport* transport)
{
    // init all the fields to 0
    memset(transport, 0, sizeof(*transport));

    // create a queue to receive data (ignore errors here for simplicity)
    GG_SharedQueue_Create(8, &transport->rx_queue);

    // setup interfaces
    GG_SET_INTERFACE(transport, UdpTransport, GG_DataSinkListener);
    GG_SET_INTERFACE(transport, UdpTransport, GG_DataSink);
    GG_SET_INTERFACE(transport, UdpTransport, GG_RemoteTransport);
}

//----------------------------------------------------------------------
static GG_Result
CounterHandler_HandleRequest(GG_RemoteSmoHandler* _self,
                             const char*          request_method,
                             Fb_Smo*              request_params,
                             GG_JsonRpcErrorCode* rpc_error_code,
                             Fb_Smo**             rpc_result)
{
    CounterHandler* self = GG_SELF(CounterHandler, GG_RemoteSmoHandler);
    GG_COMPILER_UNUSED(request_method);
    GG_COMPILER_UNUSED(rpc_error_code);

    // get the 'x' parameter from the request params
    Fb_Smo* x = Fb_Smo_GetChildByName(request_params, "x");
    int64_t value = 1;
    if (x && Fb_Smo_GetType(x) == FB_SMO_TYPE_INTEGER) {
        value = Fb_Smo_GetValueAsInteger(x);
    }

    // respond with an error if x is odd
    if ((value % 2) == 1) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // multiply the input x value by the counter and increemnt the counter
    value *= self->counter++;

    // create the result as a single integer
    *rpc_result = Fb_Smo_Create(&GG_SmoHeapAllocator, "i", value);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(CounterHandler, GG_RemoteSmoHandler) {
    CounterHandler_HandleRequest
};

//----------------------------------------------------------------------
static void
InitCounterHandler(CounterHandler* handler)
{
    handler->counter = 0;

    // setup interfaces
    GG_SET_INTERFACE(handler, CounterHandler, GG_RemoteSmoHandler);
}

//----------------------------------------------------------------------
static GG_Result
HelloWorldHandler_HandleRequest(GG_RemoteCborHandler* _self,
                                const char*          request_method,
                                GG_Buffer*            request_params,
                                GG_JsonRpcErrorCode*  rpc_error_code,
                                GG_Buffer**           rpc_result)
{
    HelloWorldHandler* self = GG_SELF(HelloWorldHandler, GG_RemoteCborHandler);
    GG_COMPILER_UNUSED(request_method);
    GG_COMPILER_UNUSED(request_params);
    GG_COMPILER_UNUSED(rpc_error_code);

    *rpc_result = GG_StaticBuffer_AsBuffer(&self->canned_response);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(HelloWorldHandler, GG_RemoteCborHandler) {
    HelloWorldHandler_HandleRequest
};

//----------------------------------------------------------------------
static uint8_t canned_cbor_response[38] = {
  0xa2, 0x6a, 0x6f, 0x74, 0x68, 0x65, 0x72, 0x46, 0x69, 0x65, 0x6c, 0x64,
  0x83, 0x01, 0x02, 0x03, 0x68, 0x67, 0x72, 0x65, 0x65, 0x74, 0x69, 0x6e,
  0x67, 0x6c, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0x57, 0x6f, 0x72,
  0x6c, 0x64
};

static void
InitHelloWorldHandler(HelloWorldHandler* handler)
{
    GG_StaticBuffer_Init(&handler->canned_response, canned_cbor_response, sizeof(canned_cbor_response));

    // setup interfaces
    GG_SET_INTERFACE(handler, HelloWorldHandler, GG_RemoteCborHandler);
}

//----------------------------------------------------------------------
static void*
RemoteShellThreadMain(void* arg)
{
    GG_RemoteShell* shell = (GG_RemoteShell*)arg;

    printf("=== remote shell thread starting\n");
    GG_RemoteShell_Run(shell);
    printf("=== remote shell thread ending\n");

    return NULL;
}

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Remote Shell over UDP - port=%d ===\n", UDP_REMOTE_SERVER_PORT);

    GG_Loop* loop;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // init the hello world handler
    HelloWorldHandler hello_world_handler;
    InitHelloWorldHandler(&hello_world_handler);

    // init the counter handler
    CounterHandler counter_handler;
    InitCounterHandler(&counter_handler);

    // init a transport instance
    UdpTransport transport;
    InitTransport(&transport);

    // create a UDP socket for the transport
    GG_SocketAddress local_address = {
        GG_IpAddress_Any,
        UDP_REMOTE_SERVER_PORT
    };
    GG_Result result = GG_BsdDatagramSocket_Create(&local_address, NULL, false, 1024, &transport.socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        return 1;
    }
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(transport.socket), GG_CAST(&transport, GG_DataSink));
    GG_DatagramSocket_Attach(transport.socket, loop);

    // create a remote shell
    GG_RemoteShell* shell = NULL;
    GG_RemoteShell_Create(GG_CAST(&transport, GG_RemoteTransport), &shell);

    // register the handlers with the shell
    GG_RemoteShell_RegisterCborHandler(shell, "hello-world", GG_CAST(&hello_world_handler, GG_RemoteCborHandler));
    GG_RemoteShell_RegisterSmoHandler(shell,  "counter",     GG_CAST(&counter_handler,      GG_RemoteSmoHandler));

    // spawn a thread for the transport
    printf("=== spawning thread\n");
    pthread_t thread;
    pthread_create(&thread, NULL, RemoteShellThreadMain, shell);

    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}
