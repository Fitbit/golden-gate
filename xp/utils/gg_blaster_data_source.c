/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-01-07
 *
 * @details
 * Blaster data source
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_blaster_data_source.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.utils.blaster-data-source")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_BlasterDataSource {
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_TimerListener);

    GG_DataSink* sink;             ///< Sink to send packets to
    GG_Buffer*   pending_output;   ///< Packet waiting to be sent
    GG_Timer*    send_timer;       ///< Send timer, or NULL for no-wait blast
    unsigned int send_interval;    ///< Send timer interval, in ms
    size_t       packet_size;      ///< Size of each packet
    size_t       packet_count;     ///< Number of packets sent
    size_t       max_packet_count; ///< Maximum number of packets to send, or 0 for unlimited
    bool         running;          ///< True if the blaster is running
    GG_BlasterDataSourcePacketFormat packet_format;
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_BlasterDataSource_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_BlasterDataSource* self = GG_SELF(GG_BlasterDataSource, GG_DataSource);

    // de-register as a listener from the current sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    // keep a reference to the new sink
    self->sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(self, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_BlasterDataSource_FillBasicPacket(GG_BlasterDataSource* self, uint8_t* packet_data)
{
    GG_ASSERT(self->packet_size >= GG_BLASTER_BASIC_COUNTER_PACKET_MIN_SIZE);

    // counter (last one set to 0xFFFFFFFF to mark the end)
    uint32_t counter = (uint32_t)self->packet_count;
    if (counter + 1 == self->max_packet_count) {
        counter = 0xFFFFFFFF;
    }
    GG_BytesFromInt32Be(packet_data, counter);

    // pattern
    for (unsigned int i = 4; i < self->packet_size; i++) {
        packet_data[i] = (uint8_t)i;
    }
}

//----------------------------------------------------------------------
static void
GG_BlasterDataSource_FillIpPacket(GG_BlasterDataSource* self, uint8_t* packet_data)
{
    GG_ASSERT(self->packet_size >= GG_BLASTER_IP_COUNTER_PACKET_MIN_SIZE);

    packet_data[0] = (4 << 4) | (5); // Version | IHL
    packet_data[1] = 0; // TOS
    packet_data[2] = (uint8_t)(self->packet_size >> 8);
    packet_data[3] = (uint8_t)(self->packet_size);
    packet_data[4] = (uint8_t)(self->packet_count >> 8); // put the counter in the Identification field
    packet_data[5] = (uint8_t)(self->packet_count);
    uint8_t flags = 1 << 7; // flags
    if (self->packet_count + 1 != self->max_packet_count) {
        flags |= 1 << 6; // use the MF bit of the Flags field to indicate whether this is the last packet or not
    }
    packet_data[6] = flags;
    memset(&packet_data[7], 0, 20 - 7); // zero out the rest or the header

    // pattern
    for (unsigned int i = 20; i < self->packet_size; i++) {
        packet_data[i] = (uint8_t)i;
    }
}

//----------------------------------------------------------------------
static void
GG_BlasterDataSource_NextPacket(GG_BlasterDataSource* self)
{
    GG_ASSERT(self->pending_output == NULL);

    // do nothing if we're not running
    if (!self->running) {
        return;
    }

    // check if we have reached our max
    if (self->max_packet_count && self->packet_count == self->max_packet_count) {
        GG_LOG_INFO("blast packet count reached");
        return;
    }

    // create a new packet of the required size
    GG_LOG_FINER("next packet, packet_count = %u", (int)self->packet_count);
    GG_DynamicBuffer* packet = NULL;
    GG_Result result = GG_DynamicBuffer_Create(self->packet_size, &packet);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_DynamicBuffer_Create failed (%d)", result);
        return;
    }
    self->pending_output = GG_DynamicBuffer_AsBuffer(packet);

    // fill the packet according to the packet format
    GG_DynamicBuffer_SetDataSize(packet, self->packet_size);
    uint8_t* packet_data = GG_DynamicBuffer_UseData(packet);
    switch (self->packet_format) {
        case GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT:
            GG_BlasterDataSource_FillBasicPacket(self, packet_data);
            break;

        case GG_BLASTER_IP_COUNTER_PACKET_FORMAT:
            GG_BlasterDataSource_FillIpPacket(self, packet_data);
            break;
    }
}

//----------------------------------------------------------------------
static void
GG_BlasterDataSource_OnCanPut(GG_DataSinkListener* _self)
{
    GG_BlasterDataSource* self = GG_SELF(GG_BlasterDataSource, GG_DataSinkListener);

    // check that we have a sink
    if (!self->sink) {
        return;
    }

    // try to send as much as we can/should
    do {
        if (self->pending_output) {
            GG_LOG_FINE("trying to send packet %u", (int)self->packet_count);
            GG_Result result = GG_DataSink_PutData(self->sink, self->pending_output, NULL);
            if (GG_FAILED(result)) {
                GG_LOG_FINER("packet not sent");
                if (result != GG_ERROR_WOULD_BLOCK) {
                    GG_LOG_WARNING("GG_DataSink_PutData failed (%d)", result);
                }
                break;
            }

            // release the buffer we just sent and account for it
            GG_LOG_FINER("packet sent");
            GG_Buffer_Release(self->pending_output);
            self->pending_output = NULL;
            ++self->packet_count;

            // packet sent, move on to the next one, unless we're on a timer
            if (self->send_interval == 0) {
                GG_BlasterDataSource_NextPacket(self);
            }
        }
    } while (self->pending_output && self->send_interval == 0);
}

//----------------------------------------------------------------------
static void
GG_BlasterDataSource_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t time_elapsed)
{
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(time_elapsed);
    GG_BlasterDataSource* self = GG_SELF(GG_BlasterDataSource, GG_TimerListener);

    GG_LOG_FINER("tick - packet_count=%u", (int)self->packet_count);

    // exit early if we're not running anymore
    if (!self->running) {
        return;
    }

    // try to move on to the next packet if we can
    if (self->pending_output == NULL) {
        GG_BlasterDataSource_NextPacket(self);
    }

    // try to flush anything that's pending
    GG_BlasterDataSource_OnCanPut(GG_CAST(self, GG_DataSinkListener));

    // re-arm the timer
    GG_Timer_Schedule(self->send_timer, GG_CAST(self, GG_TimerListener), self->send_interval);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_BlasterDataSource, GG_DataSource) {
    .SetDataSink = GG_BlasterDataSource_SetDataSink
};

GG_IMPLEMENT_INTERFACE(GG_BlasterDataSource, GG_DataSinkListener) {
    .OnCanPut = GG_BlasterDataSource_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_BlasterDataSource, GG_TimerListener) {
    .OnTimerFired = GG_BlasterDataSource_OnTimerFired
};

//----------------------------------------------------------------------
GG_Result
GG_BlasterDataSource_Create(size_t                           packet_size,
                            GG_BlasterDataSourcePacketFormat packet_format,
                            size_t                           max_packet_count,
                            GG_TimerScheduler*               timer_scheduler,
                            unsigned int                     send_interval,
                            GG_BlasterDataSource**           source)
{
    GG_ASSERT(source);

    // check parameters
    if (packet_size == 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    if (packet_format == GG_BLASTER_IP_COUNTER_PACKET_FORMAT &&
        packet_size < GG_BLASTER_IP_COUNTER_PACKET_MIN_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }
    if (packet_format == GG_BLASTER_BASIC_COUNTER_PACKET_FORMAT &&
        packet_size < GG_BLASTER_BASIC_COUNTER_PACKET_MIN_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // create a timer if required
    GG_Timer* send_timer = NULL;
    if (send_interval) {
        if (timer_scheduler == NULL) {
            return GG_ERROR_INVALID_PARAMETERS;
        }
        GG_Result result = GG_TimerScheduler_CreateTimer(timer_scheduler, &send_timer);
        if (GG_FAILED(result)) {
            return result;
        }
    }

    // allocate a new object
    *source = (GG_BlasterDataSource*)GG_AllocateZeroMemory(sizeof(GG_BlasterDataSource));
    if (*source == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // init the object fields
    (*source)->running          = false;
    (*source)->send_timer       = send_timer;
    (*source)->send_interval    = send_interval;
    (*source)->packet_size      = packet_size;
    (*source)->packet_format    = packet_format;
    (*source)->max_packet_count = max_packet_count;

    // initialize the object interfaces
    GG_SET_INTERFACE(*source, GG_BlasterDataSource, GG_DataSource);
    GG_SET_INTERFACE(*source, GG_BlasterDataSource, GG_DataSinkListener);
    GG_SET_INTERFACE(*source, GG_BlasterDataSource, GG_TimerListener);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_BlasterDataSource_Destroy(GG_BlasterDataSource* self)
{
    if (self == NULL) return;

    // de-register as a listener from the sink
    if (self->sink) {
        GG_DataSink_SetListener(self->sink, NULL);
    }

    // destroy our timer if we have one
    GG_Timer_Destroy(self->send_timer);

    // release any pending buffer
    if (self->pending_output) {
        GG_Buffer_Release(self->pending_output);
    }

    // free the object memory
    GG_ClearAndFreeObject(self, 3);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_BlasterDataSource_AsDataSource(GG_BlasterDataSource* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_Result
GG_BlasterDataSource_Start(GG_BlasterDataSource* self)
{
    // start the timer if we have one
    if (self->send_timer && !GG_Timer_IsScheduled(self->send_timer)) {
        GG_Timer_Schedule(self->send_timer, GG_CAST(self, GG_TimerListener), self->send_interval);
    }

    // reset the state in case we are re-starting
    self->packet_count = 0;
    if (self->pending_output) {
        GG_Buffer_Release(self->pending_output);
        self->pending_output = NULL;
    }

    // we're now running
    self->running = true;

    // create the first packet
    GG_BlasterDataSource_NextPacket(self);

    // try to start sending
    GG_BlasterDataSource_OnCanPut(GG_CAST(self, GG_DataSinkListener));

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_BlasterDataSource_Stop(GG_BlasterDataSource* self)
{
    // we're now stopped
    self->running = false;

    // unschedule the timer
    if (self->send_timer) {
        GG_Timer_Unschedule(self->send_timer);
    }

    // release any pending packet
    if (self->pending_output) {
        GG_Buffer_Release(self->pending_output);
        self->pending_output = NULL;
    }

    return GG_SUCCESS;
}
