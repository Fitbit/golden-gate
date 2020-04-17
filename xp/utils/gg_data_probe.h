/**
 * @file
 * @brief General purpose Data probe interfaces
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Michael Hoyt
 *
 * @date 2018-10-01
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_strings.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Data Probe object.
 */
typedef struct GG_DataProbe GG_DataProbe;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

typedef struct GG_DataProbeReport {
    size_t       total_bytes;
    uint32_t     total_throughput;          //! bytes / seconds
    uint32_t     total_throughput_peak;     //! bytes / seconds
    uint32_t     window_throughput;         //! bytes / seconds
    uint32_t     window_throughput_peak;    //! bytes / seconds
    uint32_t     window_bytes_second;       //! bytes * seconds
    uint32_t     window_bytes_second_peak;  //! bytes * seconds
    GG_Timestamp time;
} GG_DataProbeReport;

//! The different report types
/**
 * When this flag is set in the options, the stats will include total throughput calcultion
 */
#define GG_DATA_PROBE_OPTION_TOTAL_THROUGHPUT   1

/**
 * When this flag is set in the options, the stats will include windowed throughput calcultion
 */
#define GG_DATA_PROBE_OPTION_WINDOW_THROUGHPUT  2

/**
 * When this flag is set in the options, the stats will include window integral calcultion
 */
#define GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL    4

//----------------------------------------------------------------------
//! Interface implemented by objects that listen for reports.
//!
//! @interface GG_DataProbeListener
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_DataProbeListener) {
    //! Called whenever a data probe provides a periodic report update.
    //!
    //! @param self The object on which this method is invoked.
    //! @param probe The reporting probe.
    void (*OnReportReady)(GG_DataProbeListener* self, GG_DataProbe* probe);
};

//! @var GG_DataProbeListener::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_DataProbeListenerInterface
//! Virtual function table for the GG_DataProbeListener interface

//! @relates GG_DataProbeListener
//! @copydoc GG_DataProbeListenerInterface::OnReport
void GG_DataProbe_OnReportReady(GG_DataProbeListener* self, GG_DataProbe* probe);

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a Data Probe.
 *
 * @param options Or'ed combination of option flags.
 * @param buffer_sample_count Amount of samples that are going to get stored. When buffer_sample_count
 * is too small, samples will be squashed. Must be at least 2 to support windowing mode.
 * @param window_size_ms Sliding window size in milliseconds. 0 to disable windowing.
 * @param report_interval_ms Reporting interval in milliseconds.
 * @param listener Data probe report listener. NULL for no reports.
 * @param probe Pointer to where the created object will be returned.
 *
 * @return GG_SUCCESS if the object was created, or a negative error code.
 */
GG_Result GG_DataProbe_Create(uint32_t              options,
                              uint32_t              buffer_sample_count,
                              uint32_t              window_size_ms,
                              uint32_t              report_interval_ms,
                              GG_DataProbeListener* listener,
                              GG_DataProbe**        probe);

/**
 * Destroy a Data Probe.
 *
 * @param self The object on which this method is called.
 */
void GG_DataProbe_Destroy(GG_DataProbe* self);

/**
 * Get the window size of the probe in milliseconds.
 *
 * @param self The object on which this method is called.
 */
uint32_t GG_DataProbe_GetWindowSize(GG_DataProbe* self);

/**
 * Reset accumulated stats and initialize the reference time,
 * for data calculation, to the provided time.
 *
 * @param self The object on which this method is called.
 * @param time reference time to begin calculating against.
 */
void GG_DataProbe_ResetWithTime(GG_DataProbe* self, GG_Timestamp time);

/**
* Reset accumulated stats in a Data Probe and initialize the reference time,
* for calculation, to the current system time.
*
* @param self The object on which this method is called.
*/
void GG_DataProbe_Reset(GG_DataProbe* self);

/**
 * Force the probe to invoke the listener that a report could be generated.
 *
 * @param self The object on which this method is called.
 */
void GG_DataProbe_ForceReport(GG_DataProbe* self);

/**
 * Retrieve a report using current time.
 *
 * @param self The object on which this method is called.
 *
 * @return The report.
 */
const GG_DataProbeReport* GG_DataProbe_GetReport(GG_DataProbe* self);

/**
 * Retrieve a report using specified time.
 *
 * @param self The object on which this method is called.
 * @param time The time to use for window calculations.
 *
 * @return The report.
 */
const GG_DataProbeReport* GG_DataProbe_GetReportWithTime(GG_DataProbe* self, GG_Timestamp time);

/**
 * Accumulate bytes for calculations using current time.
 *
 * @param self The object on which this method is called.
 * @param byte_count Number of bytes to accumulate.
 */
void GG_DataProbe_Accumulate(GG_DataProbe* self, size_t byte_count);

/**
 * Accumulate bytes for calculations using specified time.
 *
 * @param self The object on which this method is called.
 * @param byte_count Number of bytes to accumulate.
 * @param time Time bytes were captured.
 */
void GG_DataProbe_AccumulateWithTime(GG_DataProbe* self, size_t byte_count, GG_Timestamp time);

//!@}

#ifdef __cplusplus
}
#endif /* __cplusplus */
