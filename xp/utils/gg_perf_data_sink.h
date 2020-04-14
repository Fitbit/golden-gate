/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-01-09
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Utils Utilities
//! Data sink that can be used for performance measurements
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
/**
 * Performance-measuring sink object.
 */
typedef struct GG_PerfDataSink GG_PerfDataSink;

/**
 * Stats maintained by the sink
 */
typedef struct {
    size_t   packets_received;              ///< Total number of packets received
    size_t   bytes_received;                ///< Total number of bytes received
    uint32_t throughput;                    ///< Throughput in bytes/s
    uint32_t last_received_counter;         ///< Last packet counter received
    uint32_t next_expected_counter;         ///< Expected next counter
    size_t   gap_count;                     ///< Number of detected counter gaps
    size_t   passthrough_would_block_count; ///< Number of times the passthrough sink returned GG_ERROR_WOULD_BLOCK
} GG_PerfDataSinkStats;

/**
 * Type of packets expected by the sink.
 */
typedef enum {
    GG_PERF_DATA_SINK_MODE_RAW,                ///< Can receive any packets with any payload,
    GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER ///< Can receive packets from a GG_BlasterDataSource
} GG_PerfDataSinkMode;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
/**
 * When this flag is set in the options, the stats will be printed on the console
 */
#define GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_CONSOLE   1

/**
 * When this flag is set in the options, the stats will be logged with level INFO
 */
#define GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_LOG       2

/**
 * When this flag is set in the options, the stats will be automatically reset
 * every time the stats are output
 */
#define GG_PERF_DATA_SINK_OPTION_AUTO_RESET_STATS         4

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a Perf data sink.
 *
 * @param mode What type of packets to expect.
 * @param options Or'ed combination of option flags.
 * @param stats_print_time_interval How frequently to print the stats, in milliseconds.
 * @param sink Address of the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the object was created, or a negative error code.
 */
GG_Result GG_PerfDataSink_Create(GG_PerfDataSinkMode mode,
                                 uint32_t            options,
                                 unsigned int        stats_print_time_interval,
                                 GG_PerfDataSink**   sink);

/**
 * Destroy a Perf data sink.
 *
 * @param self The object on which this method is invoked.
 */
void GG_PerfDataSink_Destroy(GG_PerfDataSink* self);

/**
 * Specify that the sink should pass buffers through to a GG_DataSink target
 *
 * @param self The object on which this method is invoked.
 * @param target The sink to which buffers should be passed through.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_PerfDataSink_SetPassthroughTarget(GG_PerfDataSink* self,
                                               GG_DataSink*     target);

/**
 * Get the GG_DataSink interface for the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_DataSink interface for the object.
 */
GG_DataSink* GG_PerfDataSink_AsDataSink(GG_PerfDataSink* self);

/**
 * Reset the statistics.
 *
 * @param self The object on which this method is invoked.
 */
void GG_PerfDataSink_ResetStats(GG_PerfDataSink* self);

/**
 * Get the current statistics.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The current statistics.
 */
const GG_PerfDataSinkStats* GG_PerfDataSink_GetStats(GG_PerfDataSink* self);

//!@}

#if defined(__cplusplus)
}
#endif
