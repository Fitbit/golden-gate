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
 * Sockets API port on top of the "Nano IP" stack
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/sockets/gg_sockets.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Create a UDP socket for the NIP IP stack.
 *
 * @see GG_DatagramSocket_Create
 */
GG_Result GG_NipDatagramSocket_Create(const GG_SocketAddress* local_address,
                                      const GG_SocketAddress* remote_address,
                                      bool                    connect_to_remote,
                                      unsigned int            max_datagram_size,
                                      GG_DatagramSocket**     socket);

#if defined(__cplusplus)
}
#endif
