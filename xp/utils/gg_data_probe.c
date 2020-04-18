/**
 *
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
 *
 * @details
 * Data Probe
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_ring_buffer.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_data_probe.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.utils.data-probe")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_DataProbeSample {
    GG_Timestamp time;
    double       byte_count; // double to improve byte second integral calculation when squashing
} GG_DataProbeSample;

struct GG_DataProbe {
    uint32_t              options;
    uint32_t              buffer_sample_count;         //! Buffer size in number of samples
    uint32_t              window_size_ms;              //! Window size in milliseconds
    uint32_t              report_interval_ms;          //! Update interval in milliseconds
    GG_DataProbeSample*   samples;                     //! Buffer of samples
    uint16_t              oldest_sample_index;
    uint32_t              num_samples;
    uint32_t              total_bytes_count;

    GG_DataProbeReport    report;
    GG_Timestamp          last_reset_time;

    GG_DataProbeListener* listener;                    //! optional listener implemented by user of probe
};

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/

#define GG_PROBE_SAFE_INDEX(probe, index, increment) ((index + increment) % probe->buffer_sample_count)

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

void
GG_DataProbe_OnReportReady(GG_DataProbeListener* self, GG_DataProbe* probe)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnReportReady(self, probe);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_DataProbe_Create(uint32_t              options,
                    uint32_t              buffer_sample_count,
                    uint32_t              window_size_ms,
                    uint32_t              report_interval_ms,
                    GG_DataProbeListener* listener,
                    GG_DataProbe**        probe)
{
    GG_ASSERT(probe);

    // allocate a new object
    *probe = (GG_DataProbe*)GG_AllocateZeroMemory(sizeof(GG_DataProbe) +
                                                  (sizeof(GG_DataProbeSample) * buffer_sample_count));
    if (*probe == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    (*probe)->samples = (GG_DataProbeSample*)(*probe + 1);

    // init the object fields
    (*probe)->options             = options;
    (*probe)->window_size_ms      = window_size_ms;
    (*probe)->buffer_sample_count = buffer_sample_count;
    (*probe)->report_interval_ms  = report_interval_ms;
    (*probe)->listener            = listener;

    if (window_size_ms > 0) {
        GG_ASSERT(buffer_sample_count > 1);
    }

    GG_DataProbe_Reset(*probe);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_DataProbe_Destroy(GG_DataProbe* self)
{
    if (self == NULL) return;

    // free the object memory
    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
uint32_t
GG_DataProbe_GetWindowSize(GG_DataProbe* self)
{
    return self->window_size_ms;
}

//----------------------------------------------------------------------
void
GG_DataProbe_Reset(GG_DataProbe* self)
{
    GG_ASSERT(self);
    GG_Timestamp current_time = GG_System_GetCurrentTimestamp();
    GG_DataProbe_ResetWithTime(self, current_time);
}

//----------------------------------------------------------------------
void
GG_DataProbe_ResetWithTime(GG_DataProbe* self, GG_Timestamp time)
{
    GG_ASSERT(self);

    memset(&self->report, 0, sizeof(self->report));
    self->report.time                 = time;
    self->total_bytes_count           = 0;
    self->last_reset_time             = time;
    self->oldest_sample_index         = 0;
}

//----------------------------------------------------------------------
void
GG_DataProbe_ForceReport(GG_DataProbe* self)
{
    if (self->listener) {
        GG_DataProbe_OnReportReady(self->listener, self);
    }
}

//----------------------------------------------------------------------
/* Trim samples, keeping only samples within current window except for
* one sample outside, needed for window integral calculation but skipped
* for windowed throuput calculation.
*/
static void
GG_DataProbe_TrimWindow(GG_DataProbe* self, GG_Timestamp now)
{
    GG_ASSERT(self->window_size_ms);
    GG_ASSERT(self->buffer_sample_count);

    GG_Timestamp window_start_time =
        now - ((uint64_t)self->window_size_ms * GG_NANOSECONDS_PER_MILLISECOND);

    // Remove all samples outside the window except for one.
    while (self->num_samples > 1) {
        uint16_t index = GG_PROBE_SAFE_INDEX(self, self->oldest_sample_index, 1);
        GG_DataProbeSample sample = self->samples[index];

        // Check if sample is inside window
        if (sample.time >= window_start_time) {
            break;
        }

        // Remove previous sample otherwise
        self->oldest_sample_index = index;
        --self->num_samples;
    }
}

//----------------------------------------------------------------------
/* This function will calculate the window integral using a Left Riemann Sum
*  Here is a diagram to help visualize the math going on:
*
*Sample  Sample Sample
*(1)     (2)    (3)
*|       |      |
*\/      |      |
* ______ \/     |
*|      |______ |
*| S1   | S2    |\/
*| *    |  *    |__________x (now)
*| time | time  | S3 * time|
*/
static GG_Result
GG_DataProbe_CalculateWindowIntegral(GG_DataProbe* self, GG_Timestamp now)
{
    GG_ASSERT(self->window_size_ms);
    GG_ASSERT(self->buffer_sample_count);

    GG_Timestamp window_start_time =
        now - ((uint64_t)self->window_size_ms * GG_NANOSECONDS_PER_MILLISECOND);

    // Calculate the total bytes-seconds
    double last_sample_byte_count = 0;
    GG_Timestamp last_sample_time = window_start_time;
    double total_byte_nanos = 0;
    bool first_sample_found = false;

    for (uint32_t i = 0, j = self->oldest_sample_index;
         i < self->num_samples;
         i++, j = GG_PROBE_SAFE_INDEX(self, j, 1)) {
        GG_DataProbeSample sample = self->samples[j];

        // Stop when reaching end of window
        if (sample.time > now) {
            break;
        }

        // Skip samples out of the window
        if (sample.time < window_start_time) {
            last_sample_byte_count = sample.byte_count;
            first_sample_found = true;
            continue;
        }

        // If the first sample is in the window, adjust window start time to compensate
        if (!first_sample_found) {
            last_sample_byte_count = sample.byte_count;
            last_sample_time = sample.time;
            first_sample_found = true;
            continue;
        }

        total_byte_nanos += (sample.time - last_sample_time) * last_sample_byte_count;
        last_sample_byte_count = sample.byte_count;
        last_sample_time = sample.time;
    }

    // Incorporate contribution of last sample
    total_byte_nanos += (now - last_sample_time) * last_sample_byte_count;

    self->report.window_bytes_second =
        (uint32_t)(total_byte_nanos / GG_NANOSECONDS_PER_SECOND); // bytes/sec
    self->report.window_bytes_second_peak =
        GG_MAX(self->report.window_bytes_second_peak, self->report.window_bytes_second);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_DataProbe_CalculateWindowThroughput(GG_DataProbe* self, GG_Timestamp now)
{
    GG_ASSERT(self->window_size_ms);
    GG_ASSERT(self->buffer_sample_count);

    // Calculate total bytes in window
    GG_Timestamp window_start_time = now - ((uint64_t)self->window_size_ms * GG_NANOSECONDS_PER_MILLISECOND);
    uint32_t window_byte_count = 0;
    for (uint32_t i = 0, j = self->oldest_sample_index; i < self->num_samples;
        ++i, j = GG_PROBE_SAFE_INDEX(self, j, 1)) {
        // Ignore samples outside the window
        if (self->samples[j].time < window_start_time) {
            continue;
        }

        window_byte_count += (uint32_t)self->samples[j].byte_count;
    }

    // Update latest stats
    double seconds = (double)self->window_size_ms / GG_MILLISECONDS_PER_SECOND;
    self->report.window_throughput =
        (uint32_t)((double)window_byte_count / seconds); // bytes/sec
    self->report.window_throughput_peak =
        GG_MAX(self->report.window_throughput_peak, self->report.window_throughput);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_DataProbe_CalculateTotalThroughput(GG_DataProbe* self, GG_Timestamp now)
{
    GG_Timestamp interval = now - self->last_reset_time;
    if (interval == 0) {
        return GG_SUCCESS;
    }

    double seconds = (double)interval / GG_NANOSECONDS_PER_SECOND;
    self->report.total_throughput =
        (uint32_t)((double)self->total_bytes_count / seconds); // bytes/sec
    self->report.total_throughput_peak =
        GG_MAX(self->report.total_throughput_peak, self->report.total_throughput);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Update the relevant stats and then return a report using current time.
const GG_DataProbeReport*
GG_DataProbe_GetReport(GG_DataProbe* self)
{
    return GG_DataProbe_GetReportWithTime(self, GG_System_GetCurrentTimestamp());
}

//----------------------------------------------------------------------
// Update the relevant stats and then return a report using specified time.
const GG_DataProbeReport*
GG_DataProbe_GetReportWithTime(GG_DataProbe* self, GG_Timestamp time)
{
    GG_ASSERT(self);

    if (self->options & GG_DATA_PROBE_OPTION_TOTAL_THROUGHPUT) {
        GG_DataProbe_CalculateTotalThroughput(self, time);
    }
    if (self->options & GG_DATA_PROBE_OPTION_WINDOW_THROUGHPUT) {
        GG_DataProbe_CalculateWindowThroughput(self, time);
    }
    if (self->options & GG_DATA_PROBE_OPTION_WINDOW_INTEGRAL) {
        GG_DataProbe_CalculateWindowIntegral(self, time);
    }

    self->report.total_bytes = self->total_bytes_count;
    self->report.time = time;
    return &self->report;
}

//----------------------------------------------------------------------
void
GG_DataProbe_Accumulate(GG_DataProbe* self, size_t byte_count)
{
    GG_DataProbe_AccumulateWithTime(self, byte_count, GG_System_GetCurrentTimestamp());
}

//----------------------------------------------------------------------
void
GG_DataProbe_AccumulateWithTime(GG_DataProbe* self, size_t byte_count, GG_Timestamp time)
{
    GG_ASSERT(self);
    GG_ASSERT(self->last_reset_time != 0);
    GG_ASSERT(time >= self->last_reset_time);
    GG_ASSERT(time >= self->report.time);

    GG_DataProbeReport* report = &self->report;

    // Update total stats
    self->total_bytes_count += (uint32_t)byte_count;

    GG_LOG_FINEST("Probe total bytes: %u", (int)self->total_bytes_count);

    // Are we keeping a sliding window?
    if (self->window_size_ms) {
        // This should already be enforced at creation time
        GG_ASSERT(self->buffer_sample_count > 1);

        // Remove oldest samples
        GG_DataProbe_TrimWindow(self, time);

        // Determine which sample to put the new value
        GG_DataProbeSample* target_sample;

        // Check if buffer is full
        if (self->num_samples == self->buffer_sample_count) {
            // Before replacing the most recent sample with the new value, update the value of the
            // previous sample so the overall window byte-second is unchanged.
            target_sample = &self->samples[GG_PROBE_SAFE_INDEX(self, self->oldest_sample_index, self->num_samples - 1)];
            GG_ASSERT(time >= target_sample->time);

            // Squash previous sample to match byte * second value
            GG_DataProbeSample* previous_sample =
                &self->samples[GG_PROBE_SAFE_INDEX(self, self->oldest_sample_index, self->num_samples - 2)];
            double squashed_byte_count =
                ((((double)previous_sample->byte_count * (target_sample->time - previous_sample->time)) +
                  ((double)target_sample->byte_count * (time - target_sample->time))) /
                    (time - previous_sample->time));
            previous_sample->byte_count = squashed_byte_count;
        } else {
            target_sample = &self->samples[GG_PROBE_SAFE_INDEX(self, self->oldest_sample_index, self->num_samples)];
            GG_ASSERT(time >= target_sample->time);
            self->num_samples++;
        }

        target_sample->byte_count = (double)byte_count;
        target_sample->time = time;
    }

    // Should we notify we have enough data for a new report?
    if (self->listener) {
        if (((time - report->time) / GG_NANOSECONDS_PER_MILLISECOND) >= self->report_interval_ms) {
            GG_DataProbe_OnReportReady(self->listener, self);
        }
    }
}
