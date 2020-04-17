/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-25
 *
 * @details
 *
 * Example UDP echo server
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>

#include "xp/common/gg_port.h"
#include "xp/coap/gg_coap.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_DataSink);

    GG_DatagramSocket* socket;
} EchoServer;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define UDP_ECHO_SERVER_PORT 9000

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static GG_Result
EchoServer_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(_self);
    EchoServer* self = GG_SELF(EchoServer, GG_DataSink);

    printf("=== got data, size=%d\n", (int)GG_Buffer_GetDataSize(data));
    GG_SocketAddressMetadata metadata_copy;
    if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS) {
        const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)metadata;
        printf("remote address: %d.%d.%d.%d:%d\n",
               socket_metadata->socket_address.address.ipv4[0],
               socket_metadata->socket_address.address.ipv4[1],
               socket_metadata->socket_address.address.ipv4[2],
               socket_metadata->socket_address.address.ipv4[3],
               socket_metadata->socket_address.port);

        // copy the source metadata to the destination metadata, changing the type
        metadata_copy = *socket_metadata;
        metadata_copy.base.type = GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS;
        metadata = &metadata_copy.base;
    }

    // echo the data back (ignore failures for now)
    GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(self->socket), data, metadata);

    return GG_SUCCESS;
}

static GG_Result
EchoServer_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

static void
EchoServer_OnCanPut(GG_DataSinkListener* _self)
{
    GG_COMPILER_UNUSED(_self);
}

GG_IMPLEMENT_INTERFACE(EchoServer, GG_DataSink) {
    EchoServer_PutData,
    EchoServer_SetListener
};

GG_IMPLEMENT_INTERFACE(EchoServer, GG_DataSinkListener) {
    EchoServer_OnCanPut
};

static void
InitServer(EchoServer* server)
{
    server->socket = NULL;

    // setup interfaces
    GG_SET_INTERFACE(server, EchoServer, GG_DataSinkListener);
    GG_SET_INTERFACE(server, EchoServer, GG_DataSink);
}

int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate UDP Echo Server - port=%d ===\n", UDP_ECHO_SERVER_PORT);

    // init a server instance
    EchoServer server;
    InitServer(&server);

    // create a UDP socket
    GG_SocketAddress local_address = {
        GG_IpAddress_Any,
        UDP_ECHO_SERVER_PORT
    };
    GG_Result result = GG_DatagramSocket_Create(&local_address, NULL, false, 1024, &server.socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_DatagramSocket_Create failed (%d)\n", result);
        return 1;
    }
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(server.socket), GG_CAST(&server, GG_DataSink));

    GG_Loop* loop = NULL;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);
    GG_DatagramSocket_Attach(server.socket, loop);
    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}
