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
 * NIP implementation of the socket interface.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_threads.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/nip/gg_nip.h"
#include "gg_nip_sockets.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DatagramSocket);

    GG_NipUdpEndpoint udp_endpoint;

    GG_THREAD_GUARD_ENABLE_BINDING
} GG_NipDatagramSocket;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_DataSink*
GG_NipDatagramSocket_AsDataSink(GG_DatagramSocket* _self)
{
    GG_NipDatagramSocket* self = GG_SELF(GG_NipDatagramSocket, GG_DatagramSocket);

    return GG_CAST(&self->udp_endpoint, GG_DataSink);
}

//----------------------------------------------------------------------
static GG_DataSource*
GG_NipDatagramSocket_AsDataSource(GG_DatagramSocket* _self)
{
    GG_NipDatagramSocket* self = GG_SELF(GG_NipDatagramSocket, GG_DatagramSocket);

    return GG_CAST(&self->udp_endpoint, GG_DataSource);
}

//----------------------------------------------------------------------
static void
GG_NipDatagramSocket_Destroy(GG_DatagramSocket* _self)
{
    if (!_self) return;
    GG_NipDatagramSocket* self = GG_SELF(GG_NipDatagramSocket, GG_DatagramSocket);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // remove the socket from the stack
    GG_Nip_RemoveEndpoint(&self->udp_endpoint);

    // free the memory
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
static GG_Result
GG_NipDatagramSocket_Attach(GG_DatagramSocket* self, GG_Loop* loop)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(loop);
    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   function tables
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_NipDatagramSocket, GG_DatagramSocket) {
    GG_NipDatagramSocket_AsDataSink,
    GG_NipDatagramSocket_AsDataSource,
    GG_NipDatagramSocket_Destroy,
    GG_NipDatagramSocket_Attach
};

//----------------------------------------------------------------------
GG_Result
GG_NipDatagramSocket_Create(const GG_SocketAddress* local_address,
                            const GG_SocketAddress* remote_address,
                            bool                    connect_to_remote,
                            unsigned int            max_datagram_size,
                            GG_DatagramSocket**     socket_object)
{
    GG_COMPILER_UNUSED(max_datagram_size);

    // default return value
    *socket_object = NULL;

    // allocate a new object
    GG_NipDatagramSocket* self = (GG_NipDatagramSocket*)GG_AllocateZeroMemory(sizeof(GG_NipDatagramSocket));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the instance
    GG_NipUdpEndpoint_Init(&self->udp_endpoint,
                           local_address,
                           remote_address,
                           connect_to_remote);
    GG_Result result = GG_Nip_AddUdpEndpoint(&self->udp_endpoint);
    if (GG_FAILED(result)) {
        GG_FreeMemory(self);
        return result;
    }

    // init the function tables
    GG_SET_INTERFACE(self, GG_NipDatagramSocket, GG_DatagramSocket);

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *socket_object = GG_CAST(self, GG_DatagramSocket);

    return GG_SUCCESS;
}
