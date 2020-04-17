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
 * @date 2017-09-29
 *
 * @details
 *
 * Gattlink blaster over UDP transport
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_ring_buffer.h"
#include "xp/module/gg_module.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/protocols/gg_ipv4_protocol.h"
#include "xp/gattlink/gg_gattlink.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/utils/gg_blaster_data_source.h"
#include "xp/utils/gg_perf_data_sink.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define BLAST_GATTLINK_BUFFER_SIZE 1152
#define BLAST_GATTLINK_MTU         128
#define BLAST_PACKET_SIZE          512

//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);

    GG_Loop*         loop;
    bool             client_mode;
    GG_SocketAddress local_address;
    GG_SocketAddress remote_address;
} StartMessage;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void
StartMessage_Handle(GG_LoopMessage* _self) {
    StartMessage* self = GG_SELF(StartMessage, GG_LoopMessage);

    // create a UDP socket to use as a GattLink transport
    GG_DatagramSocket* socket;
    GG_Result result = GG_BsdDatagramSocket_Create(&self->local_address, &self->remote_address, false, 1024, &socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        GG_Loop_RequestTermination(self->loop);
        return;
    }

    // attach the socket to the loop
    GG_DatagramSocket_Attach(socket, self->loop);

    // create a frame serializer
    GG_Ipv4FrameSerializer* frame_serializer;
    result = GG_Ipv4FrameSerializer_Create(NULL, &frame_serializer);
    GG_ASSERT(GG_SUCCEEDED(result));

    // create a frame assembler
    GG_Ipv4FrameAssembler* frame_assembler;
    result = GG_Ipv4FrameAssembler_Create(BLAST_GATTLINK_BUFFER_SIZE, NULL, NULL, &frame_assembler);
    GG_ASSERT(GG_SUCCEEDED(result));

    // setup a gattlink instance
    GG_GattlinkGenericClient* client = NULL;
    result = GG_GattlinkGenericClient_Create(GG_Loop_GetTimerScheduler(self->loop),
                                             BLAST_GATTLINK_BUFFER_SIZE,
                                             0, 0,
                                             BLAST_GATTLINK_MTU,
                                             NULL,
                                             GG_Ipv4FrameSerializer_AsFrameSerializer(frame_serializer),
                                             GG_Ipv4FrameAssembler_AsFrameAssembler(frame_assembler),
                                             &client);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_GattlinkGenericClient_Create failed (%d)\n", result);
        GG_Loop_RequestTermination(self->loop);
        return;
    }

    // connect the transport sink and source to the transport side of the client
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(client),
                              GG_DatagramSocket_AsDataSink(socket));
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(socket),
                              GG_GattlinkGenericClient_GetTransportSideAsDataSink(client));


    // start the gattlink session
    GG_GattlinkGenericClient_Start(client);

    // create a performance-measuring sink
    GG_PerfDataSink* sink = NULL;
    result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER,
                                    GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_CONSOLE |
                                    GG_PERF_DATA_SINK_OPTION_AUTO_RESET_STATS,
                                    1000, // print stats every second
                                    &sink);

    // connect the perf sink to the user side of the GattLink client
    GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetUserSideAsDataSource(client),
                              GG_PerfDataSink_AsDataSink(sink));

    // in client mode, start a blaster
    if (self->client_mode) {
        // init a blaster instance
        GG_BlasterDataSource* blaster = NULL;
        result = GG_BlasterDataSource_Create(BLAST_PACKET_SIZE,
                                             GG_BLASTER_IP_COUNTER_PACKET_FORMAT,
                                             0,    // unlimited packets
                                             NULL, // no timer
                                             0,    // no timer
                                             &blaster);
        GG_ASSERT(result == GG_SUCCESS);

        // connect the blaster source to the gattlink sink
        GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster),
                                  GG_GattlinkGenericClient_GetUserSideAsDataSink(client));

        // start the blaster
        GG_BlasterDataSource_Start(blaster);
    }
}

//----------------------------------------------------------------------
static void
StartMessage_Release(GG_LoopMessage* _self) {
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
// Blast packets over a GatttLink session for which the transport is a UDP socket
//
// Launch this either in client mode (last command line argument is 'c') or
// in server mode (last command line argument is 's').
// The client <send-ip-addr> and <send-port> must match the server's host IP address
// address and <receive-port>.
//
// Example, on a single host:
// in one shell, run:
// gg-gattlink-blast-over-udp-example 127.0.0.1 9000 9001 s
// and in a second shell, run:
// gg-gattlink-blast-over-udp-example 127.0.0.1 9001 9000 c
//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    if (argc != 5) {
        printf("usage: gg-gattlink-blast-over-udp-example <send-ip-addr> <send-port> <receive-port> c|s\n");
        return 1;
    }

    // init Golden Gate
    GG_Module_Initialize();

    // setup the start message
    StartMessage start_message;
    memset(&start_message, 0, sizeof(start_message));

    // parse command-line parameters
    start_message.client_mode = !strcmp(argv[4], "c");
    const char* target = argv[1];
    if (GG_FAILED(GG_IpAddress_SetFromString(&start_message.remote_address.address, target))) {
        fprintf(stderr, "ERROR: invalid IP address syntax\n");
        return 1;
    }
    start_message.remote_address.port = (uint16_t)strtoul(argv[2], NULL, 10);
    start_message.local_address.port = (uint16_t)strtoul(argv[3], NULL, 10);

    // let's announce ourselves
    printf("=== Golden Gate Gattlink Over UDP Blast - %s mode, local: port = %d, remote:  host = %s, port = %d ===\n",
           start_message.client_mode ? "client" : "server",
           start_message.local_address.port,
           target,
           start_message.remote_address.port);

    // setup the loop
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_Loop_BindToCurrentThread(loop);
    GG_ASSERT(GG_SUCCEEDED(result));
    start_message.loop = loop;

    // post a start message to the loop
    GG_IMPLEMENT_INTERFACE(StartMessage, GG_LoopMessage) {
        .Handle  = StartMessage_Handle,
        .Release = StartMessage_Release
    };
    GG_SET_INTERFACE(&start_message, StartMessage, GG_LoopMessage);
    GG_Loop_PostMessage(loop, GG_CAST(&start_message, GG_LoopMessage), 0);

    // run the loop
    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    GG_Module_Terminate();

    return 0;
}
