/**
 * @file
 * @brief Generic Gattlink Client
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-11-12
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_events.h"
#include "xp/common/gg_inspect.h"
#include "xp/gattlink/gg_gattlink.h"
#include "xp/protocols/gg_protocols.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Gattlink Gattlink
//! Generic Gattlink Client
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Generic Gattlink Client that exposes a "user side" data source and data sink
 * to communicate "user data", and a "transport side" data source and data sink
 * to connect to a "raw transport" that will transmit and receive the chunks
 * of protocol data.
 * This client manages an internal output buffer to allow the protocol to retransmit
 * data if needed, and an internal input buffer to re-assemble protocol data into
 * frames.
 * The client uses a frame serializer to convert IP packets to a serialized form,
 * and a frame assembler to reassemble received serialized frames into IP packets.
 *
 *
 *                      +         ^
 *         User         |         |
 *         Side         |         |
 *        +---------+---v----+----+----+
 *        |         |  sink  | source  |
 *        |         +--------+---------+
 *        |                            |     +------------------+
 *        |           buffer           <---->| frame assembler  |
 *        |                            |     +------------------+
 *    G   |                            |     +------------------+
 *    e   |                            <---->| frame serializer |
 *    n   |                            |     +------------------+
 *    e   |      +------+---------^----+
 *    r   |      |      |         |
 *    i   |      | +----v---------+----+
 *    c   |      | |                   |
 *        |      | | GattLink Protocol |
 *    C   |      | |                   |
 *    l   |      | +----+---------^----+
 *    i   |      |      |         |
 *    e   |      +------v---------+----+
 *    n   |                            |
 *    t   |         +--------+---------+
 *        |         | source |  sink   |
 *        +---------+---+----+----^----+
 *         Transport    |         |
 *         Side         |         |
 *                      v         +
 */
typedef struct GG_GattlinkGenericClient GG_GattlinkGenericClient;

/**
 * Event emitted by a Gattlink Client when it is detecting a stall.
 * This event may be emitted several times, as the stall continues, until the stall stops.
 *
 * (cast a GG_Event to that type when the event's type ID is GG_EVENT_TYPE_GATTLINK_SESSION_STALLED)
 */
typedef struct {
    GG_Event base;
    uint32_t stalled_time; ///< Stall time so far in milliseconds.
} GG_GattlinkStalledEvent;

typedef struct {
    uint32_t window_size_ms;         ///< Size of window in ms. 0 to disable windowing.
    uint32_t buffer_sample_count;    ///< Size of buffer to allocate for probe.
    uint32_t buffer_threshold;       ///< Threshold to use for when deciding to send event.
} GG_GattlinkProbeConfig;


/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_GENERIC_GATTLINK_CLIENT_DEFAULT_MAX_TX_WINDOW_SIZE    8
#define GG_GENERIC_GATTLINK_CLIENT_DEFAULT_MAX_RX_WINDOW_SIZE    8
#define GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_MONITOR_TIMEOUT 5000

/**
 * Event type emitted by a Gattlink client when the client buffer fullness is above
 * a specified threshold. Useful when wanting to indicate important activity that
 * could benefit from a faster connection.
 *
 * The event struct is just a plain GG_Event.
 * The event source is the GG_GattlinkGenericClient object that emits the event.
 */
#define GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_OVER_THRESHOLD   GG_4CC('g', 'l', 'b', '+')

/**
 * Event type emitted by a Gattlink client when the client buffer fullness is below
 * a specified threshold. Useful when wanting to indicate lower activity that
 * could justify using a slower connection.
 *
 * The event struct is just a plain GG_Event.
 * The event source is the GG_GattlinkGenericClient object that emits the event.
 */
#define GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_UNDER_THRESHOLD  GG_4CC('g', 'l', 'b', '-')

/**
 * Event type emitted by a Gattlink client when a session is ready.
 *
 * The event struct is just a plain GG_Event.
 * The event source is the GG_GattlinkGenericClient object that emits the event.
 */
#define GG_EVENT_TYPE_GATTLINK_SESSION_READY GG_4CC('g', 'l', 's', '+')

/**
 * Event type emitted by a Gattlink client when a session is being reset.
 *
 * The event struct is just a plain GG_Event.
 * The event source is the GG_GattlinkGenericClient object that emits the event.
 */
#define GG_EVENT_TYPE_GATTLINK_SESSION_RESET GG_4CC('g', 'l', 's', '-')

/**
 * Event type emitted by a Gattlink client when a session is stalled
 * usually an indication the underlying transport isn't sending or receiving packets.
 *
 * The event struct is a GG_GattlinkStalledEvent.
 * The event source is the GG_GattlinkGenericClient object that emits the event.
 */
#define GG_EVENT_TYPE_GATTLINK_SESSION_STALLED GG_4CC('g', 'l', 's', '#')

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a new Gattlink client object.
 *
 * @param timer_scheduler Timer scheduler used for retransmit and delayed ack timers.
 * @param buffer_size Size of the circular buffer where data is held before being sent to the transport.
 * @param max_tx_window_size Maximum outgoing window size, or 0 to use the default value.
 * @param max_rx_window_size Maximum incoming window size, or 0 to use the default value.
 * @param initial_max_transport_fragment_size Initial value of the maximum size that may be sent to the
 * transport in a single packet. This value may be changed later by calling
 * GG_GattlinkGenericClient_SetMaxTransportFragmentSize.
 * @param probe_config configuration of data probe. NULL to disable probe
 * @param frame_serializer Frame serializer object used to serialize packets/frames into outgoing stream data.
 * @param frame_assembler Frame assembler object used to assemble the incoming stream data into packets/frames.
 * @param client Pointer to the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_GattlinkGenericClient_Create(GG_TimerScheduler*            timer_scheduler,
                                          size_t                        buffer_size,
                                          uint8_t                       max_tx_window_size,
                                          uint8_t                       max_rx_window_size,
                                          size_t                        initial_max_transport_fragment_size,
                                          const GG_GattlinkProbeConfig* probe_config,
                                          GG_FrameSerializer*           frame_serializer,
                                          GG_FrameAssembler*            frame_assembler,
                                          GG_GattlinkGenericClient**    client);

/**
 * Destroy a Gattlink client object.
 *
 * @param self The object on which this method is invoked.
 */
void GG_GattlinkGenericClient_Destroy(GG_GattlinkGenericClient* self);

/**
 * Return the GG_EventEmitter interface of the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_EventEmitter interface of the object.
 */
GG_EventEmitter* GG_GattlinkGenericClient_AsEventEmitter(GG_GattlinkGenericClient* self);

/**
 * Return the GG_Inspectable interface of the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_Inspectable interface of the object.
 */
GG_Inspectable* GG_GattlinkGenericClient_AsInspectable(GG_GattlinkGenericClient* self);

/**
 * Start the session.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the request succeeded, or a negative error code.
 */
GG_Result GG_GattlinkGenericClient_Start(GG_GattlinkGenericClient* self);

/**
 * Reset the client.
 * This will flush internal buffers.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the request succeeded, or a negative error code.
 */
GG_Result GG_GattlinkGenericClient_Reset(GG_GattlinkGenericClient* self);

/**
 * Set the maximum transport fragment size.
 * This parameter has the same semantics as the one passed to the constructor.
 * This method should be called if the underlying transport MTU changes and
 * the maximum transport fragment size used by the Gattlink client should be
 * updated accordingly.
 *
 * @param self The object on which this method is invoked.
 * @param max_transport_fragment_size Maximum size that may be sent to the transport in a single packet.
 *
 * @return GG_SUCCESS if the request succeeded, or a negative error code.
 */
GG_Result
GG_GattlinkGenericClient_SetMaxTransportFragmentSize(GG_GattlinkGenericClient* self,
                                                     size_t                    max_transport_fragment_size);

GG_DataSink* GG_GattlinkGenericClient_GetUserSideAsDataSink(GG_GattlinkGenericClient* self);
GG_DataSource* GG_GattlinkGenericClient_GetUserSideAsDataSource(GG_GattlinkGenericClient* self);
GG_DataSink* GG_GattlinkGenericClient_GetTransportSideAsDataSink(GG_GattlinkGenericClient* self);
GG_DataSource* GG_GattlinkGenericClient_GetTransportSideAsDataSource(GG_GattlinkGenericClient* self);

//! @}

#if defined(__cplusplus)
}
#endif
