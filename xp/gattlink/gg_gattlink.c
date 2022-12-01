/**
 * @file
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2017-09-14
 * @details
 * See header for more details about the externally facing API
 *
 */

#include "gg_gattlink.h"
#include "xp/annotations/gg_annotations.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_threads.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.gattlink.protocol")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_GATTLINK_SN_WINDOW_SIZE              (1 << 5) ///< Number of packet serial numbers we can track
#define GG_GATTLINK_SEND_ACK_TIMEOUT            200      ///< Time to wait before ack'ing a received packet, in ms
#define GG_GATTLINK_RESET_COMPLETE_TIMEOUT      1000     ///< Time to wait for a reset completion, in ms
#define GG_GATTLINK_RESET_COMPLETE_ACK_TIMEOUT  2000     ///< Time to wait for a reset completion ack, in ms
#define GG_GATTLINK_EXPECTED_ACK_TIMEOUT        4000     ///< Time to wait for a data packet ack before resending
#define GG_GATTLINK_STALL_NOTIFICATION_INTERVAL 12000    ///< Time after which we notify of a stall, in ms
#define GG_GATTLINK_MIN_VERSION                 0x0
#define GG_GATTLINK_MAX_VERSION                 0x0

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 *
 * Handshake state transitions
 *
 *          +----------+
 *          |          |
 *          |         ---
 *          |         any
 *          v         ---
 *   +-------------+   |
 *   | INITIALIZED |---+
 *   +-------------+
 *          |
 *          |      +-------------------------------------------------------+
 *       {start}   |      +------------+                                   |
 *          |      |      |            |                                   |
 *          v      v      v      --------------                            |
 *       ( send reset request )  on reset timer                            |
 *       (schedule reset timer)  --------------                            |
 *                 |                   |                                   |
 *                 v                   |                                   |
 * +------------------------------------------+                            |
 * |  AWAITING_RESET_COMPLETE_SELF_INITIATED  |                            |
 * +------------------------------------------+             +---------+    |
 *           |                      |                       |         |    |
 *    ----------------      -----------------       ----------------- |    |
 *    on reset request      on reset complete       on reset complete |    |
 *    ----------------      -----------------       ----------------- |    |
 *           |                      |                       |         |    |
 *           |            (send reset complete)             |         |    |
 *           |                      |                       |         v    |
 *           |                      +------------>+---------------------+  |
 *           |                                    |        READY        |  |
 *  (send reset complete)           +------------>+---------------------+  |
 *           |                      |                       |              |
 *           |              -----------------        ----------------      |
 *           |              on reset complete        on reset request      |
 *           |              -----------------        ----------------      |
 *           v                      |                       |              |
 * +------------------------------------------+             |              |
 * | AWAITING_RESET_COMPLETE_REMOTE_INITIATED |   (send reset complete)    |
 * +------------------------------------------+             |              |
 *           |           ^        |         ^               |              |
 *    ----------------   |  --------------  |               |              |
 *    on reset request   |  on reset timer  +---------------+              |
 *    ----------------   |  --------------                                 |
 *           |           |        |                                        |
 * (send reset complete) |        |                                        |
 *           |           |        |                                        |
 *           +-----------+        |                                        |
 *                                +----------------------------------------+
 */
typedef enum {
    GG_GATTLINK_STATE_INITIALIZED = 0x0,

    /**
     * The client has sent a 'reset request' and is now awaiting
     * a 'reset complete' from the remote
     */
    GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_SELF_INITIATED,

    /**
     * The client received a 'reset request', responded with its own
     * 'reset complete' and is now awaiting a 'reset complete' from the remote
     */
    GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_REMOTE_INITIATED,

    /**
     * The link is ready for transmission!
     */
    GG_GATTLINK_STATE_READY
} GG_GattlinkState;

/**
 * Details of the outbound state
 */
typedef struct {
    uint8_t   psn_to_ack_with;
    uint8_t   outstanding_unacked_packets;
    uint8_t   next_expected_ack_sn;
    uint8_t   next_data_sn;
    uint8_t   send_buf[GG_GATTLINK_MAX_PACKET_SIZE];
    size_t    payload_sizes[GG_GATTLINK_SN_WINDOW_SIZE];
    GG_Timer* ack_timer;
    GG_Timer* retransmit_timer;
    bool      ack_now;
} GG_GattlinkOutboundPayloadInfo;

/**
 * Details of the inbound state
 */
typedef struct {
    uint8_t next_expected_data_psn;
    bool    payload_buffer_full;
    uint8_t payload_buf[GG_GATTLINK_MAX_PACKET_SIZE];
    size_t  payload_len;
    size_t  bytes_consumed;
} GG_GattlinkInboundPayloadInfo;

/**
 * Protocol state
 */
struct GG_GattlinkProtocol {
    GG_IMPLEMENTS(GG_TimerListener);

    GG_GattlinkClient*             client;
    GG_TimerScheduler*             scheduler;
    GG_GattlinkState               state;
    GG_GattlinkSessionConfig       desired_session_cfg;
    GG_GattlinkSessionConfig       actual_session_cfg;
    uint32_t                       stall_time;               ///< Stall time in ms
    uint32_t                       last_notified_stall_time; ///< Last notified stall time

    GG_GattlinkInboundPayloadInfo  in;
    GG_GattlinkOutboundPayloadInfo out;

    GG_THREAD_GUARD_ENABLE_BINDING
};

/**
 * There are two types of packets: data packets and control packets
 */
typedef enum {
    GG_GATTLINK_PACKET_TYPE_DATA    = 0x00,
    GG_GATTLINK_PACKET_TYPE_CONTROL = 0x80
} GG_GattlinkPacketType;

#define GG_GATTLINK_PACKET_TYPE_MASK  0x80 ///< AND with a packet's first byte to get its type

/**
 * There are two types of control packets: 'reset request' and 'reset complete'
 */
typedef enum  {
    GG_GATTLINK_CONTROL_PACKET_TYPE_RESET_REQUEST  = 0x0,
    GG_GATTLINK_CONTROL_PACKET_TYPE_RESET_COMPLETE = 0x1
} GG_GattlinkControlPacketType;

#define GG_GATTLINK_CONTROL_PACKET_TYPE_MASK    0x7f  ///< AND with a control packet's first byte to get its subtype
#define GG_GATTLINK_CONTROL_PACKET_TYPE_HEADER  0x80  ///< Set in a packet's first byte to indicate a control packet

/*
 * Reset complete control packet
 */
typedef struct {
    uint8_t control_cmd_and_type;
    uint8_t gattlink_min_version;
    uint8_t gattlink_max_version;
    uint8_t max_rx_window_size;
    uint8_t max_tx_window_size;
} GG_GattlinkResetCompletePacket;

/*
 * Data packet
 */
typedef enum {
    GG_GATTLINK_DATA_PACKET_TYPE_WITHOUT_ACK     = 0x00,
    GG_GATTLINK_DATA_PACKET_TYPE_WITH_ACK        = 0x40,
    GG_GATTLINK_DATA_PACKET_TYPE_MASK            = 0x40,
    GG_GATTLINK_DATA_PACKET_TYPE_ACK_OR_PSN_MASK = 0x1f
} GG_GattlinkDataPacketType;

/*----------------------------------------------------------------------
|   GG_GattlinkClient thunks
+---------------------------------------------------------------------*/
size_t
GG_GattlinkClient_GetOutgoingDataAvailable(GG_GattlinkClient* self)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->GetOutgoingDataAvailable(self);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_GattlinkClient_GetOutgoingData(GG_GattlinkClient* self, size_t offset, void* buffer, size_t size)
{
    GG_ASSERT(self);
    GG_ASSERT(buffer);
    return GG_INTERFACE(self)->GetOutgoingData(self, offset, buffer, size);
}

/*--------------------------------------------------------------------*/
void
GG_GattlinkClient_ConsumeOutgoingData(GG_GattlinkClient* self, size_t size)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->ConsumeOutgoingData(self, size);
}

/*--------------------------------------------------------------------*/
void
GG_GattlinkClient_NotifyIncomingDataAvailable(GG_GattlinkClient* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->NotifyIncomingDataAvailable(self);
}

/*--------------------------------------------------------------------*/
size_t
GG_GattlinkClient_GetTransportMaxPacketSize(GG_GattlinkClient* self)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->GetTransportMaxPacketSize(self);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_GattlinkClient_SendRawData(GG_GattlinkClient* self, const void* buffer, size_t size)
{
    GG_ASSERT(self);
    GG_ASSERT(buffer);
    return GG_INTERFACE(self)->SendRawData(self, buffer, size);
}

/*--------------------------------------------------------------------*/
void
GG_GattlinkClient_NotifySessionReady(GG_GattlinkClient* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->NotifySessionReady(self);
}

/*--------------------------------------------------------------------*/
void
GG_GattlinkClient_NotifySessionReset(GG_GattlinkClient* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->NotifySessionReset(self);
}

/*--------------------------------------------------------------------*/
void
GG_GattlinkClient_NotifySessionStalled(GG_GattlinkClient* self, uint32_t stalled_time)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->NotifySessionStalled(self, stalled_time);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
#if defined(GG_CONFIG_ENABLE_LOGGING)

static void LogPacket(int level, char* prefix, const uint8_t* bytes, size_t length)
{
    if (level >= _GG_LocalLogger.logger.level) {
        // Check if this is a control packet
        if ((bytes[0] & GG_GATTLINK_PACKET_TYPE_MASK) == GG_GATTLINK_PACKET_TYPE_CONTROL) {
            // Extract control packet type
            GG_GattlinkControlPacketType type =
                (GG_GattlinkControlPacketType)(bytes[0] & GG_GATTLINK_CONTROL_PACKET_TYPE_MASK);
            GG_LOG_LL(_GG_LocalLogger, level, "%s %s",
                      prefix,
                      type == GG_GATTLINK_CONTROL_PACKET_TYPE_RESET_REQUEST
                      ? "Reset Request"
                      : "Reset Complete");
            return;
        }

        // Data packet
        char ack_info[20] = { 0 };
        bool has_ack = bytes[0] & GG_GATTLINK_DATA_PACKET_TYPE_WITH_ACK;
        if (has_ack) {
            snprintf(ack_info,
                     sizeof(ack_info),
                     "Ack: PSN=%d",
                     bytes[0] & GG_GATTLINK_DATA_PACKET_TYPE_ACK_OR_PSN_MASK);
            ack_info[sizeof(ack_info)-1] = 0;
        }

        char payload_info[60] = { 0 };
        uint8_t payload_offset = has_ack ? 1 : 0;
        size_t payload_size = length - payload_offset;
        if (payload_size > 0) {
            // If there's a payload, it should be at least 2 bytes (PSN + data)
            GG_ASSERT(payload_size > 1);

            snprintf(payload_info,
                     sizeof(payload_info),
                     "Payload: PSN=%d, Data=0x%02hhx, Size=%d Byte%s %s",
                     bytes[payload_offset] & GG_GATTLINK_DATA_PACKET_TYPE_ACK_OR_PSN_MASK,
                     bytes[payload_offset+1],
                     (int)payload_size-1,
                     payload_size > 1 ? "s" : "",
                     ack_info[0] != 0 ? ", " : "");
            payload_info[sizeof(payload_info)-1] = 0;
        }

        GG_LOG_LL(_GG_LocalLogger, level, "%s %s%s", prefix, ack_info, payload_info);
    }
}

#define GG_LOG_PACKET(_level, _prefix, _bytes, _length)  LogPacket((_level), (_prefix), (_bytes), (_length))

#else /* GG_CONFIG_ENABLE_LOGGING */

#define GG_LOG_PACKET(_level, _prefix, _bytes, _length)

#endif /* GG_CONFIG_ENABLE_LOGGING */

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_ScheduleTimer(GG_GattlinkProtocol* self, GG_Timer* timer, uint32_t ms_timeout)
{
    GG_TimerListener* listener = GG_CAST(self, GG_TimerListener);
    return GG_Timer_Schedule(timer, listener, ms_timeout);
}

//----------------------------------------------------------------------
static uint8_t
GG_GattlinkProtocol_BuildControlPacket(GG_GattlinkControlPacketType type)
{
    type = (type & GG_GATTLINK_CONTROL_PACKET_TYPE_MASK);
    return ((uint8_t)type | GG_GATTLINK_CONTROL_PACKET_TYPE_HEADER);
}

//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_CreateTimers(GG_GattlinkProtocol* self)
{
    GG_Result result = GG_TimerScheduler_CreateTimer(self->scheduler, &self->out.ack_timer);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_ASSERT(self->out.ack_timer != NULL);

    result = GG_TimerScheduler_CreateTimer(self->scheduler, &self->out.retransmit_timer);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_ASSERT(self->out.retransmit_timer != NULL);
}

//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_DestroyTimers(GG_GattlinkProtocol* self)
{
    GG_Timer_Destroy(self->out.ack_timer);
    self->out.ack_timer = NULL;
    GG_Timer_Destroy(self->out.retransmit_timer);
    self->out.retransmit_timer = NULL;
}

//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_UnscheduleTimers(GG_GattlinkProtocol* self)
{
    GG_Timer_Unschedule(self->out.ack_timer);
    GG_Timer_Unschedule(self->out.retransmit_timer);
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_SendRawData(GG_GattlinkProtocol* self, const void* buffer, size_t size)
{
    GG_LOG_PACKET(GG_LOG_LEVEL_FINEST, "Sending", buffer, size);
    return GG_GattlinkClient_SendRawData(self->client, buffer, size);
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_SendResetPacket(GG_GattlinkProtocol* self)
{
    // Schedule retransmit timer
    GG_LOG_FINER("scheduling reset retransmit timer");
    GG_Result result = GG_GattlinkProtocol_ScheduleTimer(self,
                                                         self->out.retransmit_timer,
                                                         GG_GATTLINK_RESET_COMPLETE_TIMEOUT);
    GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
    GG_ASSERT(GG_SUCCEEDED(result));

    // Build the packet (a reset request does not have a payload)
    uint8_t packet = GG_GattlinkProtocol_BuildControlPacket(GG_GATTLINK_CONTROL_PACKET_TYPE_RESET_REQUEST);

    // Sanity check before sending
    GG_ASSERT(GG_GattlinkClient_GetTransportMaxPacketSize(self->client) >= sizeof(packet));

    // Send the packet
    return GG_GattlinkProtocol_SendRawData(self, &packet, sizeof(packet));
}

//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_SendResetCompletePacket(GG_GattlinkProtocol* self)
{
    // Schedule retransmit timer
    GG_LOG_FINER("scheduling reset complete retransmit timer");
    GG_Result result = GG_GattlinkProtocol_ScheduleTimer(self,
                                                         self->out.retransmit_timer,
                                                         GG_GATTLINK_RESET_COMPLETE_ACK_TIMEOUT);
    GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
    GG_ASSERT(GG_SUCCEEDED(result));

    // Build the packet
    GG_GattlinkResetCompletePacket packet = {
        .control_cmd_and_type = GG_GattlinkProtocol_BuildControlPacket(GG_GATTLINK_CONTROL_PACKET_TYPE_RESET_COMPLETE),
        .gattlink_min_version = GG_GATTLINK_MIN_VERSION,
        .gattlink_max_version = GG_GATTLINK_MAX_VERSION,
        .max_rx_window_size   = self->desired_session_cfg.max_rx_window_size,
        .max_tx_window_size   = self->desired_session_cfg.max_tx_window_size,
    };

    // Sanity check before sending
    GG_ASSERT(GG_GattlinkClient_GetTransportMaxPacketSize(self->client) >= sizeof(packet));

    // Send the packet
    return GG_GattlinkProtocol_SendRawData(self, &packet, sizeof(packet));
}

//----------------------------------------------------------------------
GG_Result
GG_GattlinkProtocol_Reset(GG_GattlinkProtocol* self)
{
    // Reset any connection state
    GG_GattlinkProtocol_UnscheduleTimers(self);

    // Mark session as self initiated
    bool was_ready = (self->state == GG_GATTLINK_STATE_READY);
    self->state = GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_SELF_INITIATED;

    if (was_ready) {
        // Notify client to purge internal buffers
        GG_GattlinkClient_NotifySessionReset(self->client);
    }

    // Fire off a 'reset request'
    return GG_GattlinkProtocol_SendResetPacket(self);
}

//----------------------------------------------------------------------
// Called when a 'reset request' packet has been received
//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_HandleResetRequest(GG_GattlinkProtocol* self)
{
    // Reset any connection state
    GG_GattlinkProtocol_UnscheduleTimers(self);

    // Mark session as remote initiated
    bool was_ready = (self->state == GG_GATTLINK_STATE_READY);
    self->state = GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_REMOTE_INITIATED;

    if (was_ready) {
        // Remote initiated reset, notify client to purge partially received/sent data.
        GG_GattlinkClient_NotifySessionReset(self->client);
    }

    // Send 'reset complete' to the peer and wait for its 'reset complete' back.
    return GG_GattlinkProtocol_SendResetCompletePacket(self);
}

//----------------------------------------------------------------------
// Called when a 'reset complete' packet has been received
//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_HandleResetComplete(GG_GattlinkProtocol* self,
                                        const void*          rx_raw_data,
                                        size_t               rx_raw_data_len)
{
    if (rx_raw_data_len < sizeof(GG_GattlinkResetCompletePacket)) {
        GG_LOG_COMMS_ERROR(GG_LIB_GATTLINK_INVALID_RESET_PACKET);
        return GG_ERROR_INVALID_PARAMETERS; // something is seriously wrong - we got garbage data
    }

    const GG_GattlinkResetCompletePacket* pkt = (const GG_GattlinkResetCompletePacket*)rx_raw_data;
    // TODO: Version Check - Should we add a way to communicate that a reset failed?

    // Window sizes
    GG_GattlinkSessionConfig* actual_cfg = &self->actual_session_cfg;
    GG_GattlinkSessionConfig* desired_cfg = &self->desired_session_cfg;
    actual_cfg->max_tx_window_size = GG_MIN(desired_cfg->max_tx_window_size, pkt->max_rx_window_size);
    actual_cfg->max_rx_window_size = GG_MIN(desired_cfg->max_rx_window_size, pkt->max_tx_window_size);

    if (self->state == GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_SELF_INITIATED) {
        // We sent a 'reset request', and have now gotten a 'reset complete' from the peer.
        // We now need to send our own 'reset complete' back to the peer.
        GG_GattlinkProtocol_SendResetCompletePacket(self);
    }

    // As far as we are concerned, this session is ready! ... tell the client they can send stuff!
    // Reset any connection state including timers.
    // If our 'reset complete' back to the peer got lost, expect the peer to resend reset request.
    GG_GattlinkProtocol_DestroyTimers(self);
    memset(&self->out, 0x0, sizeof(self->out));
    memset(&self->in, 0x0, sizeof(self->in));
    GG_GattlinkProtocol_CreateTimers(self);

    self->state = GG_GATTLINK_STATE_READY;
    GG_GattlinkClient_NotifySessionReady(self->client);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Called by GG_GattlinkProtocol_HandleIncomingRawData when the incoming
// packet is a control packet.
//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_HandleControlPacket(GG_GattlinkProtocol*         self,
                                        GG_GattlinkControlPacketType type,
                                        const void*                  rx_raw_data,
                                        size_t                       rx_raw_data_len)
{
    GG_GattlinkState state = self->state;

    if (type == GG_GATTLINK_CONTROL_PACKET_TYPE_RESET_COMPLETE) {
        if (state == GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_SELF_INITIATED ||
            state == GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_REMOTE_INITIATED) {
            // We were waiting for this, the reset is now complete
            return GG_GattlinkProtocol_HandleResetComplete(self, rx_raw_data, rx_raw_data_len);
        } else {
            // This is unexpected in our current state
            GG_LOG_WARNING("unexpected reset complete received while in state %d ... ignoring", (int)state);
            GG_LOG_COMMS_ERROR(GG_LIB_GATTLINK_UNEXPECTED_RESET);
            return GG_SUCCESS;
        }
    } else if (type == GG_GATTLINK_CONTROL_PACKET_TYPE_RESET_REQUEST) {
        // The peer requested a reset
        return GG_GattlinkProtocol_HandleResetRequest(self);
    } else {
        // Not a valid type
        GG_LOG_WARNING("received unknown control packet, type=%d", (int)type);
        return GG_ERROR_INVALID_PARAMETERS;
    }
}

//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_SetPayloadSize(GG_GattlinkProtocol* self, uint32_t sn, size_t payload_size)
{
    self->out.payload_sizes[sn] = payload_size;
}

//----------------------------------------------------------------------
static uint32_t
GG_GattlinkProtocol_CalculateDistance(uint8_t sn_begin_incl, uint32_t sn_end_excl)
{
    return ((uint32_t) GG_GATTLINK_SN_WINDOW_SIZE + sn_end_excl - sn_begin_incl) % GG_GATTLINK_SN_WINDOW_SIZE;
}

//----------------------------------------------------------------------
static uint8_t
GG_GattlinkProtocol_GetNextSn(uint32_t current_sn)
{
    return (current_sn + 1) % GG_GATTLINK_SN_WINDOW_SIZE;
}

//----------------------------------------------------------------------
//! @return Number of packets in flight, *excluding* packets that are pending retransmission.
static uint32_t
GG_GattlinkProtocol_GetNumPacketsInFlight(GG_GattlinkProtocol* self)
{
    return GG_GattlinkProtocol_CalculateDistance(self->out.next_expected_ack_sn, self->out.next_data_sn);
}

//----------------------------------------------------------------------
static size_t
GG_GattlinkProtocol_GetPayloadSize(GG_GattlinkProtocol* self, uint32_t sn)
{
    return self->out.payload_sizes[sn];
}

//----------------------------------------------------------------------
// Returns `true` if the packet is awaiting an ACK
//----------------------------------------------------------------------
static size_t
GG_GattlinkProtocol_PacketIsAwaitingAck(GG_GattlinkProtocol* self, uint32_t sn)
{
    return self->out.payload_sizes[sn] != 0;
}

//----------------------------------------------------------------------
static size_t
GG_GattlinkProtocol_GetTotalNumBytesAwaitingAckUpTo(GG_GattlinkProtocol* self, uint32_t sn_end_excl)
{
    size_t num_bytes = 0;
    for (uint32_t sn = self->out.next_expected_ack_sn;
         sn != sn_end_excl;
         sn = GG_GattlinkProtocol_GetNextSn(sn)) {
        num_bytes += GG_GattlinkProtocol_GetPayloadSize(self, sn);
    }
    return num_bytes;
}

//----------------------------------------------------------------------
static size_t
GG_GattlinkProtocol_GetTotalNumBytesAwaitingAck(GG_GattlinkProtocol* self)
{
    return GG_GattlinkProtocol_GetTotalNumBytesAwaitingAckUpTo(self, self->out.next_data_sn);
}

//----------------------------------------------------------------------
static void
ClearPayloadSizesUpTo(GG_GattlinkProtocol* self, uint32_t sn_end_excl)
{
    for (uint32_t sn = self->out.next_expected_ack_sn;
         sn != sn_end_excl;
         sn = GG_GattlinkProtocol_GetNextSn(sn)) {
        GG_GattlinkProtocol_SetPayloadSize(self, sn, 0);
    }
}

//----------------------------------------------------------------------
// Prepare the next packet to send, if any.
// Returns `true` if there is something to send, or `false` if not.
//----------------------------------------------------------------------
static bool
GG_GattlinkProtocol_PrepareNextPacket(GG_GattlinkProtocol* self,
                                      uint8_t**            send_bufp,
                                      size_t*              buf_len,
                                      size_t*              payload_size)
{
    size_t max_packet_size = GG_GattlinkClient_GetTransportMaxPacketSize(self->client);
    GG_ASSERT(max_packet_size  >= sizeof(GG_GattlinkResetCompletePacket));

    // we want to ACK data before the other side is blocked waiting for an ACK
    bool ack_now = self->out.ack_now;
    if (self->out.outstanding_unacked_packets > (self->actual_session_cfg.max_rx_window_size / 2)) {
        GG_LOG_FINER("acking now: %u unacked packets > window/2", (int)self->out.outstanding_unacked_packets);
        ack_now = true;
    }

    uint8_t* send_buf = &self->out.send_buf[0];
    *send_bufp = send_buf;
    *buf_len = 0;
    *payload_size = 0;

    if (ack_now) {
        *send_buf = GG_GATTLINK_DATA_PACKET_TYPE_WITH_ACK;
        *send_buf |= self->out.psn_to_ack_with;
        max_packet_size -= 1;
        // for now, we will assume only an ACK is being sent. If we find out otherwise,
        // we will update accordingly below
        *buf_len += 1;
        send_buf++;
    }

    // do we have available windows for data too?
    if (GG_GattlinkProtocol_GetNumPacketsInFlight(self) >= self->actual_session_cfg.max_tx_window_size) {
        return ack_now; // nope, just send the ack if there is one
    }

    size_t data_to_send = GG_GattlinkClient_GetOutgoingDataAvailable(self->client);
    if (data_to_send == 0) { // no data to send
        return ack_now;
    }

    size_t offset = GG_GattlinkProtocol_GetTotalNumBytesAwaitingAck(self);

    // If retransmitting, we need to use the same fragmentation as the previous transmission.
    // The payload_sizes field will still contain the previously used size, unless it was zero'ed
    // out because it got ack'd.
    size_t data_size = GG_GattlinkProtocol_GetPayloadSize(self, self->out.next_data_sn);
    if (data_size == 0) {
        GG_ASSERT(data_to_send >= offset);
        data_size = data_to_send - offset;
        if (data_size == 0) {
            // All the data we have to send is already in flight
            return ack_now;
        }

        // Cap the size to the max of what the transport allows - 1 for the header itself:
        data_size = GG_MIN(data_size, max_packet_size - 1);
    }
    *send_buf = self->out.next_data_sn;
    send_buf++;
    GG_GattlinkClient_GetOutgoingData(self->client, offset, send_buf, data_size);
    *buf_len += (1 + data_size);
    *payload_size = data_size;

    // We are about to send data, prep the retransmit timer if it isn't already set
    if (!GG_Timer_IsScheduled(self->out.retransmit_timer)) {
        GG_LOG_FINER("scheduling retransmit timer");
        GG_TimerListener* listener = GG_CAST(self, GG_TimerListener);
        GG_Result result = GG_Timer_Schedule(self->out.retransmit_timer, listener, GG_GATTLINK_EXPECTED_ACK_TIMEOUT);
        GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
        GG_ASSERT(GG_SUCCEEDED(result));
    }

    return true;
}

//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_SendNextPackets(GG_GattlinkProtocol* self)
{
    GG_Result result;
    uint8_t*  buf_to_send;
    size_t    buf_to_send_size;
    size_t    payload_size;

    // Note: with a large window size we could be in this loop until we flush 31 packets

    while (GG_GattlinkProtocol_PrepareNextPacket(self, &buf_to_send, &buf_to_send_size, &payload_size)) {
        if ((result = GG_GattlinkProtocol_SendRawData(self, buf_to_send, buf_to_send_size)) != 0) {
            // error occurred
            GG_LOG_FATAL("Failed to send raw data over transport");
            GG_LOG_COMMS_ERROR_CODE(GG_LIB_GATTLINK_SEND_FAILED, result);

            break;
        }

        // do some book keeping since things were successfully sent
        if ((buf_to_send[0] & GG_GATTLINK_DATA_PACKET_TYPE_WITH_ACK) == GG_GATTLINK_DATA_PACKET_TYPE_WITH_ACK) {
            self->out.outstanding_unacked_packets = 0;
            self->out.ack_now = false;

            // Remove pending ack timer if any
            GG_Timer_Unschedule(self->out.ack_timer);
        }

        // did we just send data as well?
        if (payload_size > 0) {
            uint8_t psn = self->out.next_data_sn;
            GG_GattlinkProtocol_SetPayloadSize(self, psn, payload_size);
            self->out.next_data_sn = GG_GattlinkProtocol_GetNextSn(psn);
        }
    }
}

//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_OnAckTimerFired(GG_GattlinkProtocol* self, uint32_t elapsed)
{
    GG_COMPILER_UNUSED(elapsed);

    // We haven't sent any ack in our specified period
    self->out.ack_now = true;
    GG_GattlinkProtocol_SendNextPackets(self);
}

//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_OnSendTimeoutFired(GG_GattlinkProtocol* self, uint32_t elapsed)
{
    GG_COMPILER_UNUSED(elapsed);

    // Check if we're stalled
    self->stall_time += elapsed;
    if (self->stall_time < self->last_notified_stall_time) {
        // deal with possible wraparound, just in case
        self->last_notified_stall_time = 0;
        self->stall_time = elapsed;
    }
    if (self->stall_time - self->last_notified_stall_time > GG_GATTLINK_STALL_NOTIFICATION_INTERVAL) {
        self->last_notified_stall_time = self->stall_time;
        GG_GattlinkClient_NotifySessionStalled(self->client, self->stall_time);
        GG_LOG_COMMS_ERROR(GG_LIB_GATTLINK_STALL);
    }

    // Check if we need to resend a reset
    GG_GattlinkState state = self->state;
    bool awaiting_reset_complete =
        (state == GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_SELF_INITIATED) ||
        (state == GG_GATTLINK_STATE_AWAITING_RESET_COMPLETE_REMOTE_INITIATED);
    if (awaiting_reset_complete) {
        // If we never received the 'reset complete' in time, (re-)perform self initiated reset
        GG_LOG_FINE("Reset Complete Timeout");
        GG_GattlinkProtocol_Reset(self);
        return;
    }

    // Verify that the session is still ready
    if (state != GG_GATTLINK_STATE_READY) {
        GG_LOG_WARNING("Timeout fired in Invalid state");
        return;
    }

    // retransmit un-acked data
    uint8_t sn = self->out.next_expected_ack_sn;

    GG_LOG_WARNING("Data Ack Timeout: Rolling back from (%d, %d) to %d",
                   (int)self->out.next_data_sn,
                   (int)self->out.next_expected_ack_sn, (int)sn);

    self->out.next_data_sn = sn;
    GG_GattlinkProtocol_SendNextPackets(self);
}

//----------------------------------------------------------------------
// Called by GG_GattlinkProtocol_HandleIncomingRawData when the incoming
// packet is a data packet.
//----------------------------------------------------------------------
static GG_Result
GG_GattlinkProtocol_HandleDataPacket(GG_GattlinkProtocol* self,
                                     const void*          rx_raw_data,
                                     size_t               rx_raw_data_len)
{
    // sanity check
    if (rx_raw_data_len > sizeof(self->in.payload_buf)) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    const uint8_t* data = (const uint8_t*)rx_raw_data;
    if ((data[0] & GG_GATTLINK_DATA_PACKET_TYPE_WITH_ACK) == GG_GATTLINK_DATA_PACKET_TYPE_WITH_ACK) {
        uint8_t ackd_psn = data[0] & GG_GATTLINK_DATA_PACKET_TYPE_ACK_OR_PSN_MASK;

        // Handle this ACK if we haven't done so already
        if (GG_GattlinkProtocol_PacketIsAwaitingAck(self, ackd_psn)) {
            // Get the number of bytes ack'd before we wipe the counters
            uint32_t next_psn = GG_GattlinkProtocol_GetNextSn(ackd_psn);
            size_t num_bytes_acked = GG_GattlinkProtocol_GetTotalNumBytesAwaitingAckUpTo(self, next_psn);

            GG_LOG_FINER("Received Ack PSN: %d for %d byte(s), Next expected Ack PSN: %d",
                         (int)ackd_psn, (int)num_bytes_acked, (int)next_psn);

            // We know the bytes are received so clear the sizes
            ClearPayloadSizesUpTo(self, next_psn);

            if (!GG_GattlinkProtocol_PacketIsAwaitingAck(self, next_psn)) {
                // Not awaiting any ack
                GG_Timer_Unschedule(self->out.retransmit_timer);
            } else {
                // Still awaiting for an ack .. re-arm our retransmit timer
                GG_Result result = GG_Timer_Schedule(self->out.retransmit_timer,
                                                     GG_CAST(self, GG_TimerListener),
                                                     GG_GATTLINK_EXPECTED_ACK_TIMEOUT);
                GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
                GG_ASSERT(GG_SUCCEEDED(result));
            }

            self->out.next_expected_ack_sn = (uint8_t)next_psn;
            GG_GattlinkClient_ConsumeOutgoingData(self->client, num_bytes_acked);
        } else {
            GG_LOG_FINE("Ignoring retransmitted Ack PSN: %d", (int)ackd_psn);
        }

        data++;
        rx_raw_data_len--;
    }

    if (rx_raw_data_len == 0) {
        // It was just an ack
        GG_GattlinkProtocol_SendNextPackets(self);
        return GG_SUCCESS;
    }

    if (self->in.payload_buffer_full) {
        // Client still hasn't consumed the data ... drop and rely on a retransmit
        GG_LOG_WARNING("Our receive buffer is full because the client hasn't consumed the data yet");
        GG_LOG_COMMS_ERROR(GG_LIB_GATTLINK_BUFFER_FULL);

        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    uint8_t psn = data[0] & GG_GATTLINK_DATA_PACKET_TYPE_ACK_OR_PSN_MASK;
    if (psn == self->in.next_expected_data_psn) {
        // That's the PSN we expected, grab the underlying data
        data++;
        rx_raw_data_len--;

        memcpy(&self->in.payload_buf[0], data, rx_raw_data_len);
        self->in.payload_len = rx_raw_data_len;
        self->in.bytes_consumed = 0;
        self->in.payload_buffer_full = true;

        self->in.next_expected_data_psn = GG_GattlinkProtocol_GetNextSn((uint32_t)psn);
        GG_LOG_FINER("Received %d Byte(s): 0x%02hhx... PSN: %d, Next expected PSN: %d",
                    (int)rx_raw_data_len, data[0], (int)psn, (int)self->in.next_expected_data_psn);
        GG_GattlinkClient_NotifyIncomingDataAvailable(self->client);

        self->out.psn_to_ack_with = psn;
    } else {
        // Not what we expected, check if it's a resent packet we've already received and acked
        uint32_t psn_distance = GG_GattlinkProtocol_CalculateDistance(psn, self->in.next_expected_data_psn);
        if (psn_distance >= self->actual_session_cfg.max_rx_window_size) {
            // Not a restransmission, ignore. Should we reset gattlink if too far in the future?
            GG_LOG_WARNING("Received PSN (%d) != Expected PSN (%d)", (int)psn,
                           (int)self->in.next_expected_data_psn);
            GG_LOG_COMMS_ERROR(GG_LIB_GATTLINK_UNEXPECTED_PSN);

            return GG_ERROR_GATTLINK_UNEXPECTED_PSN;
        }

        // The ack for this will be sent later (either when the ack timer expires, or with our outgoing data)
        GG_LOG_WARNING("Received previously received PSN (%d) != Expected (%d), Re-acking with last received PSN (%d)",
                       (int)psn, (int)self->in.next_expected_data_psn, self->out.psn_to_ack_with);
    }

    // Increment the number of unacked packets
    self->out.outstanding_unacked_packets++;
    GG_LOG_FINEST("%u unacked packets", (int)self->out.outstanding_unacked_packets);

    // Schedule the ack timer
    GG_Result result = GG_Timer_Schedule(self->out.ack_timer,
                                         GG_CAST(self, GG_TimerListener),
                                         GG_GATTLINK_SEND_ACK_TIMEOUT);
    GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
    GG_ASSERT(GG_SUCCEEDED(result));

    // Flush data/acks if necessary
    GG_GattlinkProtocol_SendNextPackets(self);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Called when any timer fires. This method will dispatch to the appropriate
// handler depending on which timer fired.
//----------------------------------------------------------------------
static void
GG_GattlinkProtocol_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    GG_GattlinkProtocol* self = GG_SELF(GG_GattlinkProtocol, GG_TimerListener);
    if (timer == self->out.ack_timer) {
        GG_GattlinkProtocol_OnAckTimerFired(self, elapsed);
    } else if (timer == self->out.retransmit_timer) {
        GG_GattlinkProtocol_OnSendTimeoutFired(self, elapsed);
    }
}

/*----------------------------------------------------------------------
|   function table
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_GattlinkProtocol, GG_TimerListener) {
    GG_GattlinkProtocol_OnTimerFired
};

//----------------------------------------------------------------------
GG_Result
GG_GattlinkProtocol_Create(GG_GattlinkClient*              client,
                           const GG_GattlinkSessionConfig* config,
                           GG_TimerScheduler*              scheduler,
                           GG_GattlinkProtocol**           protocol)
{
    GG_ASSERT(client != NULL);
    GG_ASSERT(scheduler != NULL);

    GG_GattlinkProtocol* self = (GG_GattlinkProtocol*)GG_AllocateZeroMemory(sizeof(GG_GattlinkProtocol));
    if (self == NULL) {
        *protocol = NULL;
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // Init the object
    self->client              = client;
    self->desired_session_cfg = *config;
    self->scheduler           = scheduler;

    GG_GattlinkProtocol_CreateTimers(self);

    // Init the function tables
    GG_SET_INTERFACE(self, GG_GattlinkProtocol, GG_TimerListener);

    // Bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    // return the object
    *protocol = self;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_GattlinkProtocol_Destroy(GG_GattlinkProtocol* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_GattlinkProtocol_DestroyTimers(self);
    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
size_t
GG_GattlinkProtocol_GetIncomingDataAvailable(GG_GattlinkProtocol* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    if (!self->in.payload_buffer_full) {
        return 0;
    }

    return self->in.payload_len - self->in.bytes_consumed;
}

//----------------------------------------------------------------------
// Called by the client to read data from the protocol buffer.
// NOTE: this does not remove the data from the buffer. Removing data
// from the buffer is done in GG_GattlinkProtocol_ConsumeIncomingData
//----------------------------------------------------------------------
GG_Result
GG_GattlinkProtocol_GetIncomingData(GG_GattlinkProtocol* self,
                                    size_t               offset,
                                    void*                buffer,
                                    size_t               size)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    size_t bytes_ready = GG_GattlinkProtocol_GetIncomingDataAvailable(self);
    if ((bytes_ready < size) || ((offset + size) > bytes_ready)) {
        GG_LOG_FATAL("Invalid receive request");
        return GG_ERROR_INVALID_PARAMETERS;
    }

    uint8_t* ptr = &self->in.payload_buf[self->in.bytes_consumed];
    memcpy(buffer, &ptr[offset], size);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Called by the client when it has consumed data. It allows the protocol
// object to empty its internal buffer
//----------------------------------------------------------------------
GG_Result
GG_GattlinkProtocol_ConsumeIncomingData(GG_GattlinkProtocol* self, size_t num_bytes)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    if (num_bytes > GG_GattlinkProtocol_GetIncomingDataAvailable(self)) {
        GG_LOG_WARNING("Trying to consume more data than is ready to be");
        return GG_ERROR_INVALID_PARAMETERS;
    }

    self->in.bytes_consumed += num_bytes;
    if (self->in.bytes_consumed == self->in.payload_len) {
        self->in.payload_buffer_full = false;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Called when data has been received by the transport
//----------------------------------------------------------------------
GG_Result
GG_GattlinkProtocol_HandleIncomingRawData(GG_GattlinkProtocol* self,
                                          const void*          rx_raw_data,
                                          size_t               rx_raw_data_len)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // Shortcut if there's nothing to do
    if (rx_raw_data_len == 0) {
        return GG_SUCCESS;
    }

    GG_LOG_PACKET(GG_LOG_LEVEL_FINEST, "Received", rx_raw_data, rx_raw_data_len);

    // Ignore the packet if we haven't started yet
    if (self->state == GG_GATTLINK_STATE_INITIALIZED) {
        GG_LOG_FINE("Ignoring packet, we're not started yet");
        return GG_SUCCESS;
    }

    // If previously stalled, notify session Un-stalled
    if (self->stall_time > GG_GATTLINK_STALL_NOTIFICATION_INTERVAL) {
        GG_GattlinkClient_NotifySessionStalled(self->client, 0);
    }

    // Clear the inactivity counter
    self->stall_time = 0;
    self->last_notified_stall_time = 0;

    // The header is the first byte
    const uint8_t hdr = *((const uint8_t*)rx_raw_data);

    // Check if this is a control packet
    if ((hdr & GG_GATTLINK_PACKET_TYPE_MASK) == GG_GATTLINK_PACKET_TYPE_CONTROL) {
        // Control packet
        GG_GattlinkControlPacketType type = (GG_GattlinkControlPacketType)(hdr & GG_GATTLINK_CONTROL_PACKET_TYPE_MASK);
        return GG_GattlinkProtocol_HandleControlPacket(self, type, rx_raw_data, rx_raw_data_len);
    }

    // Check that we expect a data packet
    if (self->state != GG_GATTLINK_STATE_READY) {
        // Drop the data until the connection is open again
        GG_LOG_FATAL("Received data but the connection is not open! ... dropping");
        GG_LOG_COMMS_ERROR(GG_LIB_GATTLINK_DATA_ON_CLOSED);

        return GG_ERROR_INVALID_STATE;
    }

    // Data packet
    return GG_GattlinkProtocol_HandleDataPacket(self, rx_raw_data, rx_raw_data_len);
}

//----------------------------------------------------------------------
// Called by the client when it has data ready to be sent.
// The actual data will be fetched from the client by calling
// GG_GattlinkClient_GetOutgoingDataAvailable and GG_GattlinkClient_GetOutgoingData
//----------------------------------------------------------------------
void
GG_GattlinkProtocol_NotifyOutgoingDataAvailable(GG_GattlinkProtocol* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_GattlinkProtocol_SendNextPackets(self);
}

//----------------------------------------------------------------------
GG_Result
GG_GattlinkProtocol_Start(GG_GattlinkProtocol* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);
    GG_ASSERT(self->state == GG_GATTLINK_STATE_INITIALIZED);

    return GG_GattlinkProtocol_Reset(self);
}
