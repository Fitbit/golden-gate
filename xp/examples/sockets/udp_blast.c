/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-29
 *
 * @details
 *
 * Example UDP echo server
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/utils/gg_blaster_data_source.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_LoopMessage);

    GG_Loop*         loop;
    GG_SocketAddress target_address;
    unsigned int     packet_size;
    unsigned int     packet_count;
    unsigned int     send_interval;
} StartMessage;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static void
StartMessage_Handle(GG_LoopMessage* _self) {
    StartMessage* self = GG_SELF(StartMessage, GG_LoopMessage);

    // create a UDP socket
    GG_DatagramSocket* socket = NULL;
    GG_Result result = GG_BsdDatagramSocket_Create(NULL, &self->target_address, true, 0, &socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        GG_Loop_RequestTermination(self->loop);
        return;
    }

    // attach the socket to the loop
    GG_DatagramSocket_Attach(socket, self->loop);

    // create a blaster source
    GG_BlasterDataSource* blaster = NULL;
    result = GG_BlasterDataSource_Create(self->packet_size,
                                         GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,
                                         self->packet_count,
                                         GG_Loop_GetTimerScheduler(self->loop),
                                         self->send_interval,
                                         &blaster);
    if (GG_FAILED(result)) {
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
int
main(int argc, char** argv)
{
    if (argc < 3) {
        printf("usage: gg-udp-blast-example <ip-addr> <port> [<packet-size> [<packet-count> [<send-interval]]] \n");
        return 1;
    }
    const char* target = argv[1];

    StartMessage start_message;
    if (argc >= 4) {
        start_message.packet_size = (unsigned int)strtoul(argv[3], NULL, 10);
    } else {
        start_message.packet_size = 100;
    }
    if (argc >= 5) {
        start_message.packet_count = (unsigned int)strtoul(argv[4], NULL, 10);
    } else {
        start_message.packet_count = 0;
    }
    if (argc >= 6) {
        start_message.send_interval = (unsigned int)strtoul(argv[5], NULL, 10);
    } else {
        start_message.send_interval = 0;
    }

    if (GG_FAILED(GG_IpAddress_SetFromString(&start_message.target_address.address, target))) {
        fprintf(stderr, "ERROR: invalid IP address syntax\n");
        return 1;
    }
    start_message.target_address.port = (uint16_t)strtoul(argv[2], NULL, 10);

    printf("=== Golden Gate UDP Blast - target=%s:%d ===\n",
           target,
           start_message.target_address.port);

    // send a message to the loop so that the start function can execute in the right context
    GG_Loop* loop;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);
    GG_IMPLEMENT_INTERFACE(StartMessage, GG_LoopMessage) {
        .Handle  = StartMessage_Handle,
        .Release = StartMessage_Release
    };
    GG_SET_INTERFACE(&start_message, StartMessage, GG_LoopMessage);
    start_message.loop = loop;
    GG_Loop_PostMessage(loop, GG_CAST(&start_message, GG_LoopMessage), 0);

    // loop
    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}
