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
 * @date 2018-01-07
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Utils Utilities
//! Blaster data source
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_timer.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_BlasterDataSource GG_BlasterDataSource;

typedef enum {
    /**
     * 4 byte header that contains a packet counter in big-endian byte order.
     */
    GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT,

    /**
     * 20 byte IP packet header, including a counter as part of one of the
     * IP header fields.
     */
    GG_BLASTER_IP_COUNTER_PACKET_FORMAT
} GG_BlasterDataSourcePacketFormat;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_BLASTER_BASIC_COUNTER_PACKET_MIN_SIZE   4
#define GG_BLASTER_IP_COUNTER_PACKET_MIN_SIZE      20

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a Blaster data source.
 *
 * @param packet_size Size of each packet to send.
 * @param packet_format Format for the payload of each packet.
 * @param max_packet_count Number of packets to send, or 0 for unlimited.
 * @param timer_scheduler Timer scheduler used for timing when sending at fixed intervals.
 *                        or NULL when send_interval is 0.
 * @param send_interval Time interval between packets, in milliseconds, or 0 for max speed.
 * @param source Pointer to the variable that will receive the object.
 *
 * @return GG_SUCCESS if the object was created, or a negative error code.
 */
GG_Result GG_BlasterDataSource_Create(size_t                           packet_size,
                                      GG_BlasterDataSourcePacketFormat packet_format,
                                      size_t                           max_packet_count,
                                      GG_TimerScheduler*               timer_scheduler,
                                      unsigned int                     send_interval,
                                      GG_BlasterDataSource**           source);

/**
 * Destroy a Blaster data source.
 *
 * @param self The object on which this method is invoked.
 */
void GG_BlasterDataSource_Destroy(GG_BlasterDataSource* self);

/**
 * Get the GG_DataSource interface for the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_DataSource interface for the object.
 */
GG_DataSource* GG_BlasterDataSource_AsDataSource(GG_BlasterDataSource* self);

/**
 * Start blasting.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_BlasterDataSource_Start(GG_BlasterDataSource* self);

/**
 * Stop blasting.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_BlasterDataSource_Stop(GG_BlasterDataSource* self);

//!@}

#if defined(__cplusplus)
}
#endif
