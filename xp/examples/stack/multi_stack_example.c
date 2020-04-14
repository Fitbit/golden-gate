/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2019 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-03-24
 *
 * @details
 *
 * Multi-Stack Example application
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>

#include "xp/common/gg_common.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/stack_builder/gg_stack_builder.h"

//----------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------
#define GG_MULTI_STACK_EXAMPLE_STACK_COUNT 3 // create 3 stacks

//----------------------------------------------------------------------
// Main entry point
//
// This example application creates 3 stacks.
// The bottom transport of each stack is configured to use a UDP socket
// with incoming packets on port 6000+i and outgoing packets on port 6100+i
// (with i being the stack index from 0 to 2).
// The top of each stack is connected to a UDP socket with incoming packets
// on port 7100+i and outgoing packets on port 7000+i
// The stacks are configured as 'SNG' in Hub mode.
// You can multiple instances of the `stack tool` with a matching configuration
// to communicate with the stacks.
//
//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);
    GG_Result result;

    // let's announce ourselves
    printf("=== Golden Gate Multi-Stack Example ===\n");

    // setup the loop
    GG_Loop* loop;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // create the stacks and connect them to a transport and top socket
    GG_Stack* stacks[GG_MULTI_STACK_EXAMPLE_STACK_COUNT];
    for (unsigned int i = 0; i < GG_MULTI_STACK_EXAMPLE_STACK_COUNT; i++) {
        // create the transport socket
        GG_SocketAddress transport_local_address  = { GG_IpAddress_Any, (uint16_t)(6000 + i) };
        GG_SocketAddress transport_remote_address = { GG_IpAddress_Any, (uint16_t)(6100 + i) };
        GG_IpAddress_SetFromString(&transport_remote_address.address, "127.0.0.1");
        GG_DatagramSocket* transport_socket;
        result = GG_BsdDatagramSocket_Create(&transport_local_address,
                                             &transport_remote_address,
                                             false,
                                             1280,
                                             &transport_socket);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
            return 1;
        }
        GG_DatagramSocket_Attach(transport_socket, loop);

        // create a top socket
        GG_SocketAddress top_local_address  = { GG_IpAddress_Any, (uint16_t)(7100 + i) };
        GG_SocketAddress top_remote_address = { GG_IpAddress_Any, (uint16_t)(7000 + i) };
        GG_IpAddress_SetFromString(&top_remote_address.address, "127.0.0.1");
        GG_DatagramSocket* top_socket;
        result = GG_BsdDatagramSocket_Create(&top_local_address,
                                             &top_remote_address,
                                             false,
                                             1280,
                                             &top_socket);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
            return 1;
        }
        GG_DatagramSocket_Attach(top_socket, loop);

        // create the stack
        result = GG_StackBuilder_BuildStack("SNG",
                                            NULL,
                                            0,
                                            GG_STACK_ROLE_HUB,
                                            NULL,
                                            loop,
                                            GG_DatagramSocket_AsDataSource(transport_socket),
                                            GG_DatagramSocket_AsDataSink(transport_socket),
                                            &stacks[i]);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_StackBuilder_BuildStack failed (%d)\n", result);
            return 1;
        }

        // connect the top socket to the stack
        GG_StackElementPortInfo top_port_info;
        GG_Stack_GetPortById(stacks[i], GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &top_port_info);
        GG_DataSource_SetDataSink(top_port_info.source, GG_DatagramSocket_AsDataSink(top_socket));
        GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(top_socket), top_port_info.sink);

        // start the stack
        GG_Stack_Start(stacks[i]);
    }

    // run the loop
    GG_Loop_Run(loop);

    // cleanup
    GG_Loop_Destroy(loop);
    return 0;
}
