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
 * @details
 * Performance-measuring data sink
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_system.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "xp/utils/gg_data_probe.h"
#include "gg_perf_data_sink.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.utils.perf-data-sink")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_PerfDataSink {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_DataProbeListener);

    GG_PerfDataSinkMode  mode;
    uint32_t             options;
    GG_PerfDataSinkStats stats;
    GG_Timestamp         start_time;
    GG_DataSink*         passthrough_target;
    GG_DataSinkListener* passthrough_listener;
    GG_DataProbe*        probe;
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_PERF_SINK_LAST_PACKET_COUNTER    0xFFFFFFFF
#define GG_PERF_SINK_MAX_LOG_STRING_LENGTH  128

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
GG_PerfDataSink_ParseCounterPacket(GG_PerfDataSink* self, const uint8_t* packet, size_t packet_size)
{
    if (packet_size >= 20) {
        // maybe a packet formatted as an IP packet
        if (packet[0] == ((4 << 4) | 5)) {
            // IPv4 and header size == 20
            uint16_t ip_size = GG_BytesToInt16Be(packet + 2);
            if (ip_size == packet_size) {
                // the size matches, check other fields
                bool match = true;
                for (unsigned int i = 10; i < 20; i++) {
                    if (packet[i]) {
                        match = false;
                        break;
                    }
                }
                if (match) {
                    // everything looks good, extract the counter and flags
                    uint16_t counter = GG_BytesToInt16Be(packet + 4);
                    uint8_t  flags   = packet[6];

                    if ((flags & (1 << 6)) == 0) {
                        // last packet marker
                        self->stats.last_received_counter = GG_PERF_SINK_LAST_PACKET_COUNTER;
                    } else {
                        self->stats.last_received_counter = counter;
                    }

                    return;
                }
            }
        }
    }

    // not an IP packet, so this is a basic counter packet
    self->stats.last_received_counter = GG_BytesToInt32Be(packet);
}

//----------------------------------------------------------------------
static GG_Result
GG_PerfDataSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_PerfDataSink* self = GG_SELF(GG_PerfDataSink, GG_DataSink);

    // if we have a passthrough target, try to deliver the buffer to it
    if (self->passthrough_target) {
        GG_Result result = GG_DataSink_PutData(self->passthrough_target, data, metadata);
        if (GG_FAILED(result)) {
            // count the number of GG_ERROR_WOULD_BLOCK results
            if (result == GG_ERROR_WOULD_BLOCK) {
                ++self->stats.passthrough_would_block_count;
            }
            return result;
        }
    }

    // get the packet payload and size
    const uint8_t* packet      = GG_Buffer_GetData(data);
    size_t         packet_size = GG_Buffer_GetDataSize(data);
    GG_LOG_FINEST("got packet, size=%u - packets_received=%u, bytes_received=%u",
                  (int)packet_size,
                  (int)self->stats.packets_received,
                  (int)self->stats.bytes_received);

    // reset if we had previously received an end of sequence
    if (self->stats.last_received_counter == GG_PERF_SINK_LAST_PACKET_COUNTER) {
        GG_PerfDataSink_ResetStats(self);
    }

    // parse the payload depending on the mode
    switch (self->mode) {
        case GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER:
            GG_PerfDataSink_ParseCounterPacket(self, packet, packet_size);

            // check for gaps
            if (self->stats.last_received_counter == GG_PERF_SINK_LAST_PACKET_COUNTER) {
                self->stats.next_expected_counter = 0;
            } else if (self->stats.last_received_counter == 0) {
                // reset the stats on packet 0
                GG_LOG_INFO("packet 0 received, resetting stats");
                GG_PerfDataSink_ResetStats(self);
                self->stats.next_expected_counter = 1;
            } else {
                if (self->stats.last_received_counter != self->stats.next_expected_counter) {
                    GG_LOG_FINEST("gap detected, got %u, expected %u",
                                  (int)self->stats.last_received_counter,
                                  (int)self->stats.next_expected_counter);
                    ++self->stats.gap_count;
                }

                // update expectations
                self->stats.next_expected_counter = self->stats.last_received_counter + 1;
            }
            break;

        case GG_PERF_DATA_SINK_MODE_RAW:
            // no counters
            break;
    }

    // get the current timestamp
    GG_Timestamp now = GG_System_GetCurrentTimestamp();

    // update stats
    if (self->start_time) {
        self->stats.bytes_received += packet_size;
        ++self->stats.packets_received;
        GG_DataProbe_AccumulateWithTime(self->probe, packet_size, now);
    } else {
        self->start_time = now;
        GG_DataProbe_ResetWithTime(self->probe, now);
    }

    if (self->stats.last_received_counter == GG_PERF_SINK_LAST_PACKET_COUNTER) {
        GG_DataProbe_ForceReport(self->probe);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_PerfDataSink_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_PerfDataSink* self = GG_SELF(GG_PerfDataSink, GG_DataSink);
    self->passthrough_listener = listener;
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_PerfDataSink, GG_DataSink) {
    .PutData     = GG_PerfDataSink_PutData,
    .SetListener = GG_PerfDataSink_SetListener
};

//----------------------------------------------------------------------
static void
GG_PerfDataSink_OnCanPut(GG_DataSinkListener* _self)
{
    GG_PerfDataSink* self = GG_SELF(GG_PerfDataSink, GG_DataSinkListener);

    // pass the call through
    if (self->passthrough_listener) {
        GG_DataSinkListener_OnCanPut(self->passthrough_listener);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_PerfDataSink, GG_DataSinkListener) {
    .OnCanPut = GG_PerfDataSink_OnCanPut
};

//----------------------------------------------------------------------
static void
GG_PerfDataSink_OnDataProbeReportReady(GG_DataProbeListener* _self, GG_DataProbe* probe)
{
    GG_PerfDataSink* self = GG_SELF(GG_PerfDataSink, GG_DataProbeListener);

    // retrieve report
    const GG_DataProbeReport* report = GG_DataProbe_GetReport(probe);

    // print the stats
    char message[GG_PERF_SINK_MAX_LOG_STRING_LENGTH + 1];
    snprintf(message,
             sizeof(message),
             "%u Bps - %u packets - %u bytes - %u gaps",
             (int)(report->total_throughput),
             (int)self->stats.packets_received,
             (int)self->stats.bytes_received,
             (int)self->stats.gap_count);
    if (self->options & GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_LOG) {
        GG_LOG_INFO("%s", message);
    }

    if (self->options & GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_CONSOLE) {
        GG_System_ConsoleOutput(message);
        GG_System_ConsoleOutput("\r\n");
    }

    // auto-reset stats if needed
    if (self->options & GG_PERF_DATA_SINK_OPTION_AUTO_RESET_STATS) {
        GG_LOG_INFO("auto-resetting stats");

        // perform a partial reset (keep some relevant fields intact)
        unsigned int next_expected_counter = self->stats.next_expected_counter;
        GG_PerfDataSink_ResetStats(self);
        self->stats.next_expected_counter = next_expected_counter;
    }
}

GG_IMPLEMENT_INTERFACE(GG_PerfDataSink, GG_DataProbeListener) {
    .OnReportReady = GG_PerfDataSink_OnDataProbeReportReady
};

//----------------------------------------------------------------------
GG_Result
GG_PerfDataSink_Create(GG_PerfDataSinkMode mode,
                       uint32_t            options,
                       unsigned int        stats_print_time_interval,
                       GG_PerfDataSink**   sink)
{
    GG_ASSERT(sink);

    // allocate a new object
    *sink = (GG_PerfDataSink*)GG_AllocateZeroMemory(sizeof(GG_PerfDataSink));
    if (*sink == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object fields
    (*sink)->mode    = mode;
    (*sink)->options = options;
    GG_Result result = GG_DataProbe_Create(GG_DATA_PROBE_OPTION_TOTAL_THROUGHPUT,
                                           0,
                                           0,
                                           stats_print_time_interval,
                                           GG_CAST(*sink, GG_DataProbeListener),
                                           (&(*sink)->probe));
    GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
    GG_ASSERT(result == GG_SUCCESS);

    // initialize the object interfaces
    GG_SET_INTERFACE(*sink, GG_PerfDataSink, GG_DataSink);
    GG_SET_INTERFACE(*sink, GG_PerfDataSink, GG_DataSinkListener);
    GG_SET_INTERFACE(*sink, GG_PerfDataSink, GG_DataProbeListener);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_PerfDataSink_Destroy(GG_PerfDataSink* self)
{
    if (self == NULL) return;

    // de-register as a listener from the target
    if (self->passthrough_target) {
        GG_DataSink_SetListener(self->passthrough_target, NULL);
    }

    // free the probe
    GG_DataProbe_Destroy(self->probe);

    // free the object memory
    GG_ClearAndFreeObject(self, 3);
}

//----------------------------------------------------------------------
GG_Result
GG_PerfDataSink_SetPassthroughTarget(GG_PerfDataSink* self, GG_DataSink* target)
{
    // sanity check: we don't want to pass through to ourself
    if (target == GG_CAST(self, GG_DataSink)) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // de-register as a listener from the current target
    if (self->passthrough_target) {
        GG_DataSink_SetListener(self->passthrough_target, NULL);
    }

    // set the new target
    self->passthrough_target = target;

    // register as a listener
    if (target) {
        return GG_DataSink_SetListener(target, GG_CAST(self, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_DataSink*
GG_PerfDataSink_AsDataSink(GG_PerfDataSink* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
void
GG_PerfDataSink_ResetStats(GG_PerfDataSink* self)
{
    memset(&self->stats, 0, sizeof(self->stats));
    self->start_time = 0;
    GG_DataProbe_Reset(self->probe);
}

//----------------------------------------------------------------------
const GG_PerfDataSinkStats*
GG_PerfDataSink_GetStats(GG_PerfDataSink* self)
{
    // retrieve report from the data probe
    const GG_DataProbeReport* report = GG_DataProbe_GetReport(self->probe);

    self->stats.throughput = report->total_throughput;
    self->stats.bytes_received = report->total_bytes;
    return &self->stats;
}
