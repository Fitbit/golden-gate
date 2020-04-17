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
 * @date 2017-12-21
 *
 * @details
 *
 * Example UDP blaster with LWIP UDP sockets with a BSD UDP transport
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_io.h"
#include "xp/module/gg_module.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/sockets/ports/lwip/gg_lwip_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/lwip/gg_lwip_generic_netif.h"
#include "xp/utils/gg_blaster_data_source.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);

    GG_Loop*         loop;
    const char*      blast_target;
    GG_SocketAddress blast_target_address;
    const char*      transport_target;
    GG_SocketAddress transport_target_address;
    const char*      local_ip_address;
} StartMessage;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define BLAST_PACKET_SIZE 512

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void
StartMessage_Handle(GG_LoopMessage* _self) {
    StartMessage* self = GG_SELF(StartMessage, GG_LoopMessage);

    // create the transport UDP socket (BSD)
    GG_DatagramSocket* transport_socket = NULL;
    GG_Result result = GG_BsdDatagramSocket_Create(NULL, &self->transport_target_address, true, 0, &transport_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        GG_Loop_RequestTermination(self->loop);
        return;
    }

    // attach the transport socket to the loop
    GG_DatagramSocket_Attach(transport_socket, self->loop);

    // create a netif
    GG_LwipGenericNetworkInterface* lwip_if;
    result = GG_LwipGenericNetworkInterface_Create(0, self->loop, &lwip_if);

    // set the netif as the sink for the transport socket
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(transport_socket),
                              GG_LwipGenericNetworkInterface_AsDataSink(lwip_if));

    // set the transport socket as the sink for the netif
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if),
                              GG_DatagramSocket_AsDataSink(transport_socket));

    // register the netif
    GG_IpAddress my_addr;
    GG_IpAddress my_netmask;
    GG_IpAddress my_gateway;
    GG_IpAddress_SetFromString(&my_addr,    self->local_ip_address);
    GG_IpAddress_SetFromString(&my_netmask, "255.255.255.0");
    GG_IpAddress_SetFromString(&my_gateway, self->blast_target);
    GG_LwipGenericNetworkInterface_Register(lwip_if, &my_addr, &my_netmask, &my_gateway, true);

    // create a blaster instance
    GG_BlasterDataSource* blaster = NULL;
    result = GG_BlasterDataSource_Create(BLAST_PACKET_SIZE,
                                         GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                         0,    // unlimited packets
                                         NULL, // no timer
                                         0,    // no timer
                                         &blaster);

    // create a UDP socket (LWIP)
    GG_DatagramSocket* socket = NULL;
    result = GG_LwipDatagramSocket_Create(NULL, &self->blast_target_address, true, 0, &socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_LwipDatagramSocket_Create failed (%d)\n", result);
        GG_Loop_RequestTermination(self->loop);
        return;
    }

    // connect the blaster to the socket
    GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(blaster),
                              GG_DatagramSocket_AsDataSink(socket));

    // let's go
    GG_BlasterDataSource_Start(blaster);
}

//----------------------------------------------------------------------
static void
StartMessage_Release(GG_LoopMessage* _self) {
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
// Send a blast of packets of equal size to an LWIP UDP socket, through
// an LWIP stack configured with a network interface that uses BSD sockets
// as its transport.
//
// The other end of the transport should be a process that can receive IP
// packets encapsulated in UDP packets and tunnel them to an IP stack
//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    if (argc != 6) {
        printf("usage: gg-udp-lwip-over-bsd-blast-example <local-ip-addr> "
               "<blast-ip-addr> <blast-port> <transport-ip-addr> <transport-port>\n");
        return 1;
    }

    StartMessage start_message;

    start_message.local_ip_address = argv[1];

    start_message.blast_target = argv[2];
    if (GG_FAILED(GG_IpAddress_SetFromString(&start_message.blast_target_address.address,
                                             start_message.blast_target))) {
        fprintf(stderr, "ERROR: invalid IP address syntax\n");
        return 1;
    }
    start_message.blast_target_address.port = (uint16_t)strtoul(argv[3], NULL, 10);

    start_message.transport_target = argv[4];
    if (GG_FAILED(GG_IpAddress_SetFromString(&start_message.transport_target_address.address,
                                             start_message.transport_target))) {
        fprintf(stderr, "ERROR: invalid IP address syntax\n");
        return 1;
    }
    start_message.transport_target_address.port = (uint16_t)strtoul(argv[5], NULL, 10);

    printf("=== Golden Gate UDP Blast - source=%s, target=%s:%d, transport=%s:%d ===\n",
           start_message.local_ip_address,
           start_message.blast_target,
           start_message.blast_target_address.port,
           start_message.transport_target,
           start_message.transport_target_address.port);

    // init the library (will init LWIP)
    GG_Result result = GG_Module_Initialize();
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_Module_Initialize failed (%d)\n", result);
        return 1;
    }

    // setup the loop
    GG_Loop* loop = NULL;
    result = GG_Loop_Create(&loop);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_Loop_BindToCurrentThread(loop);
    GG_ASSERT(GG_SUCCEEDED(result));

    // send a start message to the loop
    GG_IMPLEMENT_INTERFACE(StartMessage, GG_LoopMessage) {
        .Handle  = StartMessage_Handle,
        .Release = StartMessage_Release
    };
    GG_SET_INTERFACE(&start_message, StartMessage, GG_LoopMessage);
    start_message.loop = loop;
    GG_Loop_PostMessage(loop, GG_CAST(&start_message, GG_LoopMessage), 0);

    // run the loop
    printf("+++ running loop\n");
    result = GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}
