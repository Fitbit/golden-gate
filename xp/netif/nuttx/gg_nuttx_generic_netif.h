/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-04-15
 *
 * @details
 *
 * Generic NuttX netif.
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/sockets/gg_sockets.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Object that implements a generic network interface for the NuttX IP Stack.
 * The interface uses a transport to send and receive IP packets.
 * The network interface transmits IP packets that come from the IP stack
 * to the transport, and packets received from the transport are injected
 * into the IP stack.
 * To be configured with its transport, this object implements GG_DataSource
 * for outgoing packets and GG_DataSink for incoming packets.
 *
 *    +------------------+
 *    |                  |
 *    |  NuttX IP Stack  |
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
typedef struct GG_NuttxGenericNetworkInterface GG_NuttxGenericNetworkInterface;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Create an instance of the class.
 */
GG_Result GG_NuttxGenericNetworkInterface_Create(size_t mtu, GG_NuttxGenericNetworkInterface** netif);

/**
 * Destroy an instance of the class.
 */
void GG_NuttxGenericNetworkInterface_Destroy(GG_NuttxGenericNetworkInterface* self);

/**
 * Get the GG_DataSink interface for the network interface transport side.
 *
 * @param self The object on which this method is called.
 */
GG_DataSink* GG_NuttxGenericNetworkInterface_AsDataSink(GG_NuttxGenericNetworkInterface* self);

/**
 * Get the GG_DataSource interface for the network interface tansport side.
 *
 * @param self The object on which this method is called.
 */
GG_DataSource* GG_NuttxGenericNetworkInterface_AsDataSource(GG_NuttxGenericNetworkInterface* self);

/**
 * Get the GG_Inspectable interface for the network interface.
 *
 * @param self The object on which this method is called.
 */
GG_Inspectable* GG_NuttxGenericNetworkInterface_AsInspectable(GG_NuttxGenericNetworkInterface* self);

/**
 * Register the network interface with the NuttX stack.
 *
 * @param self The object on which this method is called.
 * @param source_address IP Address assigned to the network interface.
 * @param netmask Netmask assigned to the network interface.
 * @param gateway Address of the gateway for the network interface.
 * @param is_default Whether or not to use this interface as the default.
 */
GG_Result GG_NuttxGenericNetworkInterface_Register(GG_NuttxGenericNetworkInterface* self,
                                                   const GG_IpAddress*              source_address,
                                                   const GG_IpAddress*              netmask,
                                                   const GG_IpAddress*              gateway,
                                                   bool                             is_default);

/**
 * Deregister the network interface.
 *
 * @param self The object on which this method is called.
 */
GG_Result GG_NuttxGenericNetworkInterface_Deregister(GG_NuttxGenericNetworkInterface* self);

#if defined(__cplusplus)
}
#endif
