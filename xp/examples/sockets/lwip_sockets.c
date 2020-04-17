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
 * Example usage of the LWIP sockets.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xp/module/gg_module.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_timer.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/lwip/gg_lwip_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/lwip/gg_lwip_generic_netif.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/loop/gg_loop.h"
#include "xp/protocols/gg_ipv4_protocol.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GATTLINK_BUFFER_SIZE 1500
#define GATTLINK_MTU         100
#define IP_MTU               1152

//----------------------------------------------------------------------
// Sender object that sends a message on a timer
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);
    GG_DataSink* sink;
} Sender;

static void
Sender_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_COMPILER_UNUSED(elapsed);
    Sender* self = GG_SELF(Sender, GG_TimerListener);

    // send a buffer over the socket
    GG_StaticBuffer message;
    GG_StaticBuffer_Init(&message, (const uint8_t*)"hello", 5);
    GG_DataSink_PutData(self->sink, GG_StaticBuffer_AsBuffer(&message), NULL);

    // reschedule
    GG_Timer_Schedule(timer, _self, 1000);
}

GG_IMPLEMENT_INTERFACE(Sender, GG_TimerListener) {
    Sender_OnTimerFired
};

//----------------------------------------------------------------------
// Object that prints the size of data it receives.
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
} Printer;

static GG_Result
Printer_PutData(GG_DataSink* self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(metadata);

    printf("=== got data, size=%u\n", (int)GG_Buffer_GetDataSize(data));
    return GG_SUCCESS;
}

static GG_Result
Printer_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(Printer, GG_DataSink) {
    Printer_PutData,
    Printer_SetListener
};


//----------------------------------------------------------------------
typedef enum {
    RAW_MODE,
    GATTLINK_CLIENT_MODE,
    GATTLINK_SERVER_MODE
} Mode;

int
main(int argc, char** argv)
{
    // parse command line arguments
    Mode mode = RAW_MODE;
    if (argc == 2) {
        // expect "gc" for Gattlink Client or "gs" for Gattlink Server
        if (!strcmp(argv[1], "gc")) {
            mode = GATTLINK_CLIENT_MODE;
        } else if (!strcmp(argv[1], "gs")) {
            mode = GATTLINK_SERVER_MODE;
        } else {
            fprintf(stderr, "ERROR: invalid mode\n");
            return 1;
        }
    } else if (argc != 1) {
        fprintf(stderr, "ERROR: invalid arguments\n");
        return 1;
    }

    // init the library (will init LWIP)
    GG_Result result = GG_Module_Initialize();
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_Module_Initialize failed (%d)\n", result);
        return 1;
    }

    // decide on addresses and port numbers depending on the mode
    const char* local_app_ip_addr;
    const char* remote_app_ip_addr;
    uint16_t    local_transport_port;
    uint16_t    remote_transport_port;
    const char* remote_transport_ip_addr = "127.0.0.1";
    switch (mode) {
        case RAW_MODE:
            printf("=== running in RAW mode\n");
            remote_transport_port = 9000;
            local_transport_port  = 9001;
            local_app_ip_addr     = "10.1.2.4";
            remote_app_ip_addr    = "10.1.2.3";
            break;

        case GATTLINK_CLIENT_MODE:
            printf("=== running in GATTLINK_CLIENT mode\n");
            remote_transport_port = 9000;
            local_transport_port  = 9001;
            local_app_ip_addr     = "10.1.2.4";
            remote_app_ip_addr    = "10.1.2.3";
            break;

        case GATTLINK_SERVER_MODE:
            printf("=== running in GATTLINK_SERVER mode\n");
            remote_transport_port = 9001;
            local_transport_port  = 9000;
            local_app_ip_addr     = "10.1.2.3";
            remote_app_ip_addr    = "10.1.2.4";
            break;
    }

    // create a loop
    GG_Loop* loop;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // create a BSD socket to use as a transport for the network interface
    printf("=== creating transport socket\n");
    GG_SocketAddress transport_remote_address;
    GG_IpAddress_SetFromString(&transport_remote_address.address, remote_transport_ip_addr);
    transport_remote_address.port = remote_transport_port;
    GG_SocketAddress transport_local_address;
    transport_local_address.address = GG_IpAddress_Any;
    transport_local_address.port = local_transport_port;
    GG_DatagramSocket* transport_socket;
    result = GG_BsdDatagramSocket_Create(&transport_local_address,
                                         &transport_remote_address,
                                         false,
                                         1500,
                                         &transport_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        return 1;
    }

    // attach the transport I/O to the loop
    GG_DatagramSocket_Attach(transport_socket, loop);

    // create a netif
    GG_LwipGenericNetworkInterface* lwip_if;
    result = GG_LwipGenericNetworkInterface_Create(IP_MTU, loop, &lwip_if);
    GG_ASSERT(GG_SUCCEEDED(result));

    // setup the transport connections
    GG_Ipv4FrameSerializer*   frame_serializer = NULL;
    GG_Ipv4FrameAssembler*    frame_assembler  = NULL;
    GG_GattlinkGenericClient* gattlink_client  = NULL;
    if (mode == RAW_MODE) {
        // in raw mode, just connect to the transport

        // set the netif as the sink for the transport socket
        GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(transport_socket),
                                  GG_LwipGenericNetworkInterface_AsDataSink(lwip_if));

        // set the transport socket as the sink for the netif
        GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if),
                              GG_DatagramSocket_AsDataSink(transport_socket));
    } else {
        // in Gattlink mode, create a Gattlink client and connect it to the transport
        GG_Ipv4FrameSerializer_Create(NULL, &frame_serializer);
        GG_Ipv4FrameAssembler_Create(IP_MTU, NULL, NULL, &frame_assembler);
        result = GG_GattlinkGenericClient_Create(GG_Loop_GetTimerScheduler(loop),
                                                 GATTLINK_BUFFER_SIZE,
                                                 0, 0,
                                                 GATTLINK_MTU,
                                                 NULL,
                                                 GG_Ipv4FrameSerializer_AsFrameSerializer(frame_serializer),
                                                 GG_Ipv4FrameAssembler_AsFrameAssembler(frame_assembler),
                                                 &gattlink_client);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_GattlinkGenericClient_Create failed (%d)\n", result);
            return 1;
        }

        // set the transport socket as the sink for the transport side of the gattlink client
        GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetTransportSideAsDataSource(gattlink_client),
                                  GG_DatagramSocket_AsDataSink(transport_socket));

        // set the transport side of the gattlink client the sink for the transport socket
        GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(transport_socket),
                                  GG_GattlinkGenericClient_GetTransportSideAsDataSink(gattlink_client));

        // set the netif as the sink for the user side of the gattlink client
        GG_DataSource_SetDataSink(GG_GattlinkGenericClient_GetUserSideAsDataSource(gattlink_client),
                                  GG_LwipGenericNetworkInterface_AsDataSink(lwip_if));

        // set the user side of the gattlink client as the sink for the netif
        GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if),
                                  GG_GattlinkGenericClient_GetUserSideAsDataSink(gattlink_client));
    }

    // register the netif
    GG_IpAddress my_addr;
    GG_IpAddress my_netmask;
    GG_IpAddress my_gateway;
    GG_IpAddress_SetFromString(&my_addr,    local_app_ip_addr);
    GG_IpAddress_SetFromString(&my_netmask, "255.255.255.0");
    GG_IpAddress_SetFromString(&my_gateway, remote_app_ip_addr);
    GG_LwipGenericNetworkInterface_Register(lwip_if, &my_addr, &my_netmask, &my_gateway, true);

    // create a socket to send data
    GG_DatagramSocket* socket;
    GG_SocketAddress local_address;
    local_address.address = GG_IpAddress_Any;
    local_address.port = 1234;
    GG_SocketAddress remote_address;
    GG_IpAddress_SetFromString(&remote_address.address, remote_app_ip_addr);
    remote_address.port = 1234;
    GG_LwipDatagramSocket_Create(&local_address, &remote_address, false, 1024, &socket);

    // setup a sender to send data to the socket
    Sender sender;
    sender.sink = GG_DatagramSocket_AsDataSink(socket);
    GG_SET_INTERFACE(&sender, Sender, GG_TimerListener);

    // setup a printer to print something when data is received
    Printer printer;
    GG_SET_INTERFACE(&printer, Printer, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(socket),
                              GG_CAST(&printer, GG_DataSink));

    // create a timer to send every second
    GG_Timer* timer;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &timer);
    GG_Timer_Schedule(timer, GG_CAST(&sender, GG_TimerListener), 1000);

    // start the gattlink client
    GG_GattlinkGenericClient_Start(gattlink_client);

    // run the loop
    GG_Loop_Run(loop);

    // cleanup
    GG_DatagramSocket_Destroy(socket);
    GG_LwipGenericNetworkInterface_Deregister(lwip_if);
    GG_LwipGenericNetworkInterface_Destroy(lwip_if);
    if (frame_serializer) {
        GG_Ipv4FrameSerializer_Destroy(frame_serializer);
    }
    if (frame_assembler) {
        GG_Ipv4FrameAssembler_Destroy(frame_assembler);
    }
    if (gattlink_client) {
        GG_GattlinkGenericClient_Destroy(gattlink_client);
    }
    GG_Loop_Destroy(loop);
}
