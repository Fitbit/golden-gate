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
 * @date 2017-09-26
 *
 * @details
 *
 * Networking sockets interfaces.
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
 * Create a bound UDP socket.
 */
GG_Result GG_BsdDatagramSocket_Create(const GG_SocketAddress* local_address,
                                      const GG_SocketAddress* remote_address,
                                      bool                    connect,
                                      unsigned int            max_datagram_size,
                                      GG_DatagramSocket**     socket);

#if defined(__cplusplus)
}
#endif
