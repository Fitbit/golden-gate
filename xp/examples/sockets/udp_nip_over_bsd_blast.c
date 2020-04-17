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
 * @date 2018-02-03
 *
 * @details
 *
 * Example UDP blaster with NIP UDP sockets with a BSD UDP transport
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_io.h"
#include "xp/module/gg_module.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/sockets/ports/nip/gg_nip_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/utils/gg_blaster_data_source.h"
#include "xp/utils/gg_perf_data_sink.h"
#include "xp/nip/gg_nip.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);

    GG_Loop*         loop;
    unsigned int     source_packet_size;
    unsigned int     source_packet_interval;
    GG_SocketAddress local_address;
    GG_SocketAddress remote_address;
    GG_SocketAddress transport_in_address;
    GG_SocketAddress transport_out_address;
} StartArgs;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_NIP_SINK_TIME_INTERVAL 1000

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void
start(void* arg) {
    StartArgs* self = (StartArgs*)arg;

    // create the transport UDP socket (BSD)
    GG_DatagramSocket* transport_socket = NULL;
    GG_Result result = GG_BsdDatagramSocket_Create(&self->transport_in_address,
                                                   &self->transport_out_address,
                                                   false,
                                                   2048, // max datagram size
                                                   &transport_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        GG_Loop_RequestTermination(self->loop);
        return;
    }

    // attach the transport socket to the loop
    GG_DatagramSocket_Attach(transport_socket, self->loop);

    // setup the stack
    GG_Nip_Configure(&self->local_address.address);

    // connect the transport
    GG_DataSource_SetDataSink(GG_Nip_AsDataSource(), GG_DatagramSocket_AsDataSink(transport_socket));
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(transport_socket), GG_Nip_AsDataSink());

    // create a UDP socket (NIP)
    GG_DatagramSocket* socket = NULL;
    result = GG_NipDatagramSocket_Create(&self->local_address, &self->remote_address, true, 0, &socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_NipDatagramSocket_Create failed (%d)\n", result);
        GG_Loop_RequestTermination(self->loop);
        return;
    }

    // create a blaster
    GG_BlasterDataSource* blaster = NULL;
    result = GG_BlasterDataSource_Create(self->source_packet_size,
                                        GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                        0,    // unlimited packets
                                        GG_Loop_GetTimerScheduler(self->loop),
                                        self->source_packet_interval,
                                        &blaster);

    // connect the blaster to the socket
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster),
                              GG_DatagramSocket_AsDataSink(socket));

    // create a perf sink
    GG_PerfDataSink* perf_sink = NULL;
    result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER,
                                    GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_CONSOLE |
                                    GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_LOG,
                                    GG_NIP_SINK_TIME_INTERVAL,
                                    &perf_sink);

    // connect the sink to the socket
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(socket),
                              GG_PerfDataSink_AsDataSink(perf_sink));

    // let's go
    GG_BlasterDataSource_Start(blaster);
}

//----------------------------------------------------------------------
// Send a blast of packets of equal size to a NIP UDP socket, through
// an NIP stack configured with a network interface that uses BSD sockets
// as its transport.
//
// The other end of the transport should be a process that can receive IP
// packets encapsulated in UDP packets and tunnel them to an IP stack
//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    if (argc != 9) {
        printf("usage: gg-udp-nip-over-bsd-blast-example <local-ip-addr> "
               "<remote-ip-addr> <transport-out-addr> <transport-out-port> <transport-in-port> "
               "<blast-port> <blast-packet-size> <blast-packet-interval>]\n");
        return 1;
    }

    StartArgs start_args;

    start_args.local_address.port = 0;
    start_args.remote_address.port = 0;
    GG_IpAddress_Copy(&start_args.transport_in_address.address, &GG_IpAddress_Any);

    GG_IpAddress_SetFromString(&start_args.local_address.address, argv[1]);
    GG_IpAddress_SetFromString(&start_args.remote_address.address, argv[2]);
    GG_IpAddress_SetFromString(&start_args.transport_out_address.address, argv[3]);
    start_args.transport_out_address.port = (uint16_t)strtoul(argv[4], NULL, 10);
    start_args.transport_in_address.port = (uint16_t)strtoul(argv[5], NULL, 10);
    start_args.remote_address.port       = (uint16_t)strtoul(argv[6], NULL, 10);
    start_args.source_packet_size        = (unsigned int)strtoul(argv[7], NULL, 10);
    start_args.source_packet_interval    = (unsigned int)strtoul(argv[8], NULL, 10);

    printf("=== Golden Gate NIP UDP example ===\n");

    // init the library (will init NIP)
    GG_Result result = GG_Module_Initialize();
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_Module_Initialize failed (%d)\n", result);
        return 1;
    }

    // setup the loop
    GG_Loop* loop = NULL;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);
    start_args.loop = loop;

    // invoke the start method on the loop
    GG_Loop_InvokeAsync(loop, start, &start_args);

    // run the loop
    printf("+++ running loop\n");
    result = GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}
