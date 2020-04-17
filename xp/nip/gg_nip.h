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
 * NIP - Golden Gate Nano IP Stack
 *
 * NIP stands for "Nano IP". It is a simplistic, almost trivial IP stack that
 * serves a very limited purpose: send and receive UDP packets via a single
 * network interface.
 * As such, it shouldn't be used as general purpose IP stack, but rather
 * as an ad-hoc library for environments where simple UDP packet exchange
 * over a single network interface is sufficient. For more advanced use cases,
 * or for other protocols than UDP (TCP for example) use something like LWIP.
 *
 * This library is not re-entrant, so it must only be called from a single thread.
 * This library uses an internal singleton, so only one instance of the stack may
 * exist in a process (this could easily be changed if needed).
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_lists.h"
#include "xp/sockets/gg_sockets.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Object that can send and receive UDP datagrams
 */
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);         ///< to receive UDP packets
    GG_IMPLEMENTS(GG_DataSource);       ///< to send UDP packets
    GG_IMPLEMENTS(GG_DataSinkListener); ///< to receive notifications from the sink

    GG_LinkedListNode    list_node;          ///< to allow putting this struct in a list
    GG_DataSink*         data_sink;          ///< the sink to which data will be sent
    GG_DataSinkListener* data_sink_listener; ///< the listener interested in our data events
    GG_SocketAddress     local_address;      ///< local address/port of the socket
    GG_SocketAddress     remote_address;     ///< remote address/port of the socket
    bool                 local_port_bound;   ///< set to true when the socket is bound to a local port
    bool                 connected;          ///< set to true when the socket is bound to a remote port
} GG_NipUdpEndpoint;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Initialize the stack.
 * NOTE: it isn't necessary to call this function directly,
 * since calling GG_Nip_Configure will perform lazy initialization.
 */
GG_Result GG_Nip_Initialize(void);

/**
 * Terminate the stack.
 * All sockets must have been removed prior to calling this function.
 */
void GG_Nip_Terminate(void);

/**
 * Configure the stack.
 *
 * NOTE: the transport source *must* deliver buffers in exact increments
 * of complete IP packets, as the network will not accept partial packets
 * or more than one packet per buffer.
 *
 * @param netif_address IP Address to assign to the network interface.
 *
 * @return GG_SUCCESS if the call succeeds, or a negative error code.
 */
GG_Result GG_Nip_Configure(const GG_IpAddress* netif_address);

/**
 * Get the GG_DataSink interface for the network interface.
 */
GG_DataSink* GG_Nip_AsDataSink(void);

/**
 * Get the GG_DataSource interface for the network interface.
 */
GG_DataSource* GG_Nip_AsDataSource(void);

/**
 * Add a UDP endpoint to the stack.
 * UDP endpoints that are added to the stack may send and receive datagrams.
 * The same endpoint may only be added once.
 * Endpoints that have a local port set to 0 will automatically be assigned
 * a dynamic port number by the stack.
 *
 * @param udp_endpoint The endpoint to add to the stack.
 *
 * @return GG_SUCCESS if the endpoint could be added, or a negative error code.
 */
GG_Result GG_Nip_AddUdpEndpoint(GG_NipUdpEndpoint* udp_endpoint);

/**
 * Remove a UDP endpoint from the stack.
 * After removal, the endpoint will not longer be able to send or receive
 * datagrams.
 *
 * @param udp_endpoint The endpoint to remove from the stack.
 *
 * @return GG_SUCCESS if the endpoint could be removed, or a negative error code.
 */
GG_Result GG_Nip_RemoveEndpoint(GG_NipUdpEndpoint* udp_endpoint);

/**
 * Initialize a GG_NipUdpPoint structure.
 * This function must be just called once for each GG_NipUdpPoint structure
 * before it can be added to the stack.
 *
 * @param self The structure to initialize.
 * @param local_address The local address for the endpoint. The IP address should
 * be set to "any" (GG_IpAddress_Any). The port number maybe be either a non-zero
 * value to only receive packets sent to that port, or 0 to receive all packets.
 * If more than one endpoint has an unbound port (port == 0), only the first one
 * added to the stack will receive data. This parameter may be NULL, which is the
 * same as port == 0.
 * @param remote_address The remote address for the endpoint. If NULL, calling
 * GG_DataSink_PutData for the endpoint MUST have a non-null metadata pointer
 * to indicate the destination address and port. If non-NULL, this specifies the
 * IP address and port to send to.
 * @param connect_to_remote Pass `true` to indicate that only packets with a source
 * address and port matching the remote address should be received.
 */
void GG_NipUdpEndpoint_Init(GG_NipUdpEndpoint*      self,
                            const GG_SocketAddress* local_address,
                            const GG_SocketAddress* remote_address,
                            bool                    connect_to_remote);

#if defined(__cplusplus)
}
#endif
