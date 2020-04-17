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
 * @date 2017-11-13
 *
 * @details
 *
 * Generic LWIP netif.
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/loop/gg_loop.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Object that implements a generic network interface for the LWIP IP Stack.
 * The interface uses a transport to send and receive IP packets.
 * The network interface transmits IP packets that come from the IP stack
 * to the transport, and packets received from the transport are injected
 * into the IP stack.
 * To be configured with its transport, this object implements GG_DataSource
 * for outgoing packets and GG_DataSink for incoming packets.
 *
 *    +------------------+
 *    |                  |
 *    |       LWIP       |
 *    |                  |
 *    +----+--------^----+
 *         |        |
 *  output |        | input
 *         |        |
 *    +----v--------+----+
 *    |                  |
 *    |      netif       |
 *    |                  |
 *    +---------+--------+
 *    | source  |  sink  |
 *    +----+----+---^----+
 *         |        |
 * PutData |        | PutData
 *         |        |
 *    +----v----+---+----+
 *    |  sink   | source |
 *    +---------+--------+
 *    |                  |
 *    |     transport    |
 *    |                  |
 *    +------------------+
 */
typedef struct GG_LwipGenericNetworkInterface GG_LwipGenericNetworkInterface;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Create an instance of the class.
 *
 * @param mtu Maximum Transmission Unit for the interface.
 * @param loop Loop to which the network interface belongs. May be NULL if LWIP is running
 * in "direct" mode, where the thread context in which the network interface is used is
 * the same as the thread context in which the socket calls are made (ex when LwIP is
 * configured for "NO_SYS = 1" or "LWIP_TCPIP_CORE_LOCKING = 1")
 * @param netif Pointer to where the object will be returned.
 *
 * @result GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_LwipGenericNetworkInterface_Create(size_t                           mtu,
                                                GG_Loop*                         loop,
                                                GG_LwipGenericNetworkInterface** netif);

/**
 * Destroy an instance of the class.
 *
 * @param self The object on which this method is called.
 */
void GG_LwipGenericNetworkInterface_Destroy(GG_LwipGenericNetworkInterface* self);

/**
 * Get the GG_DataSink interface for the network interface transport side.
 *
 * @param self The object on which this method is called.
 */
GG_DataSink* GG_LwipGenericNetworkInterface_AsDataSink(GG_LwipGenericNetworkInterface* self);

/**
 * Get the GG_DataSource interface for the network interface tansport side.
 *
 * @param self The object on which this method is called.
 */
GG_DataSource* GG_LwipGenericNetworkInterface_AsDataSource(GG_LwipGenericNetworkInterface* self);

/**
 * Get the GG_Inspectable interface for the network interface.
 *
 * @param self The object on which this method is called.
 */
GG_Inspectable* GG_LwipGenericNetworkInterface_AsInspectable(GG_LwipGenericNetworkInterface* self);

/**
 * Register the network interface with the LWIP stack.
 *
 * @param self The object on which this method is called.
 * @param source_address IP Address assigned to the network interface.
 * @param netmask Netmask assigned to the network interface.
 * @param gateway Address of the gateway for the network interface.
 * @param is_default Whether or not to use this interface as the default.
 */
GG_Result GG_LwipGenericNetworkInterface_Register(GG_LwipGenericNetworkInterface* self,
                                                  const GG_IpAddress*             source_address,
                                                  const GG_IpAddress*             netmask,
                                                  const GG_IpAddress*             gateway,
                                                  bool                            is_default);

/**
 * Deregister the network interface.
 *
 * @param self The object on which this method is called.
 */
GG_Result GG_LwipGenericNetworkInterface_Deregister(GG_LwipGenericNetworkInterface* self);

#if defined(__cplusplus)
}
#endif
