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
 * @date 2018-04-10
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Services Services
//! Blast service
//! @{

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/loop/gg_loop.h"
#include "xp/remote/gg_remote.h"
#include "xp/utils/gg_blaster_data_source.h"
#include "xp/utils/gg_perf_data_sink.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_BlastService GG_BlastService;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_BLAST_SERVICE_START_METHOD       "blast/start"
#define GG_BLAST_SERVICE_STOP_METHOD        "blast/stop"
#define GG_BLAST_SERVICE_GET_STATS_METHOD   "blast/get_stats"
#define GG_BLAST_SERVICE_RESET_STATS_METHOD "blast/reset_stats"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Create a Blast service object.
 *
 * @param loop The loop in which the service will run.
 * @param service [out] Address of the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_BlastService_Create(GG_Loop* loop, GG_BlastService** service);

/**
 * Destroy a Blast service object.
 *
 * @param self The object on which the method is invoked.
 */
void GG_BlastService_Destroy(GG_BlastService* self);

/**
 * Get a reference to the blast service GG_RemoteSmoHandler object
 *
 * @param self The object on which the method is invoked.
 *
 * @return Pointer to GG_RemoteSmoHandler object
 */
GG_RemoteSmoHandler* GG_BlastService_AsRemoteSmoHandler(GG_BlastService* self);

/**
 * Register the Blast service with a remote API shell.
 * This function may only be called from the same thread as the one in which the shell is
 * running.
 */
GG_Result GG_BlastService_Register(GG_BlastService* self, GG_RemoteShell* shell);

/**
 * Unregister the Blast service from a remote API shell.
 *
 * NOTE: this method may be called from any thread.
 */
GG_Result GG_BlastService_Unregister(GG_BlastService* self, GG_RemoteShell* shell);

/**
 * Set the source and sink for the service.
 * This may be used to attach/detach the service to/from a stack.
 *
 * NOTE: this method may be called from any thread.
 *
 * @param self The object on which the method is invoked.
 * @param source The source to use for receiving data (pass NULL when detaching)
 * @param sink The sink to use for sending data (pass NULL when detaching)
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_BlastService_Attach(GG_BlastService* self, GG_DataSource* source, GG_DataSink* sink);

/**
 * Get the stats measured by the service's performance-measuring sink.
 * @see GG_PerfDataSinkStats
 *
 * NOTE: this method may be called from any thread.
 *
 * @param self The objet on which the method is invoked.
 * @param stats Pointer to the struct in which the stats will be returned.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_BlastService_GetStats(GG_BlastService* self, GG_PerfDataSinkStats* stats);

/**
 * Reset the stats measured by the service's performance-measuring sink.
 *
 * NOTE: this method may be called from any thread.
 *
 * @param self The objet on which the method is invoked.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_BlastService_ResetStats(GG_BlastService* self);

/**
 * Start the service's Blaster source.
 * @see GG_BlasterDataSource
 *
 * NOTE: this method may be called from any thread.
 *
 * @param self The objet on which the method is invoked.
 * @param packet_size Size of the packets sent by the blaster.
 * @param packet_count Number of packets to send, or 0 for unlimited.
 * @param packet_interval Time interval, in milliseconds, between packets.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_BlastService_StartBlaster(GG_BlastService* self,
                                       size_t           packet_size,
                                       size_t           packet_count,
                                       size_t           packet_interval);

/**
 * Stop the service's Blaster source.
 *
 * NOTE: this method may be called from any thread.
 *
 * @param self The objet on which the method is invoked.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_BlastService_StopBlaster(GG_BlastService* self);

//!@}

#if defined(__cplusplus)
}
#endif
