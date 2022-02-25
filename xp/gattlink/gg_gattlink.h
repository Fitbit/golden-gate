/**
 * @file
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2017-08-21
 * @details
 * The API for Fitbit's BLE reliable serial streaming protocol (aka 'GattLink')
 * Detailed information can be found at this link:
 *  https://docs.google.com/document/d/15xBVdk7_2PxIqQX5yHb-6_unxYi7rfSRytUcInXAwNA
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_timer.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Gattlink Gattlink Protocol
//! Gattlink Protocol
//! @{

/*----------------------------------------------------------------------
 |   constants
 +---------------------------------------------------------------------*/
#define GG_GATTLINK_MAX_PACKET_SIZE 512

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_GattlinkProtocol GG_GattlinkProtocol;

/*----------------------------------------------------------------------
|   error code
+---------------------------------------------------------------------*/
#define GG_ERROR_GATTLINK_UNEXPECTED_PSN (GG_ERROR_BASE_GATTLINK - 0)

/*----------------------------------------------------------------------
|   GG_GattlinkClient interface
+---------------------------------------------------------------------*/
GG_DECLARE_INTERFACE(GG_GattlinkClient) {
    //! The functions that deal with sending data from the local client out to the peer
    //! @{
    //! @param[in] self The object on which this method is invoked.
    //! @return The number of bytes ready to be sent
    size_t (*GetOutgoingDataAvailable)(GG_GattlinkClient* self);

    //! Copies the requested data ready to be transferred out to the peer device.
    //! @note The data cannot be free'd upon completion of this call. It can only be free'd when
    //!        ConsumeOutgoingData() is called
    //!
    //! @param[in] self The object on which this method is invoked.
    //! @param[in] offset The index within the currently outstanding send data to start copying from
    //! @param[out] buffer The buffer to copy the send data into
    //! @param[in] size The number of bytes to copy
    //! @return #GG_SUCCESS on success, or an error code
    GG_Result (*GetOutgoingData)(GG_GattlinkClient* self, size_t offset, void* buffer, size_t size);

    //! Called once the bytes have been successfully sent. Informs the client that it may free any
    //! allocations associated with these bytes
    //!
    //! @param[in] self The object on which this method is invoked.
    //! @param[in] size The number of bytes that can be free'd
    void (*ConsumeOutgoingData)(GG_GattlinkClient* self, size_t size);

    //! @}

    //! The functions that deal with receiving data from the peer and forwarding it to the client
    //! @{

    //! Called every time data has been successfully received by the peer and converted to a data
    //! payload that is ready for reading by the client
    //! @param[in] self The object on which this method is invoked.
    void (*NotifyIncomingDataAvailable)(GG_GattlinkClient* self);

    //! @}

    //! The low level APIs for sending & receiving raw 'GattLink' data
    //! @{

    //! Informs GattLink the maximum payload size it can use. (i.e For a transport over GATT,
    //! this would be the negotiated MTU size minus the ATT Overhead)
    //!
    //! @param[in] self The object on which this method is invoked.
    //! @return the max size that can be provided to GetOutgoingData()
    size_t (*GetTransportMaxPacketSize)(GG_GattlinkClient* self);

    //! Called in order to send data over the underlying transport. See "Sending / Receiving Data
    //! Overview" in doc linked in header details
    //!
    //! @param[in] self The object on which this method is invoked.
    //! @param[in] buffer The gattlink data to send over the underlying transport
    //! @param[in] size The length of buffer
    //! @return #GG_SUCCESS on success, or an error code
    GG_Result (*SendRawData)(GG_GattlinkClient* self, const void* buffer, size_t size);

    //! @}

    //! The APIs used to open/close a GattLink session
    //! @{

    //! Called when a session has been established and is ready to send/receive data
    //! @param[in] self The object on which this method is invoked.
    void (*NotifySessionReady)(GG_GattlinkClient* self);

    //! Called when a session has been reset. This is an indication to the client that
    //! it should assume that the peer has changed and/or reset its state.
    //! As a result, there is no guarantee that data sent prior to the reset has been
    //! successfully received by the peer, so the client is likely to need to flush
    //! some of its buffers and/or reset its state as well.
    //! @param[in] self The object on which this method is invoked.
    void (*NotifySessionReset)(GG_GattlinkClient* self);

    //! Called when the communication with the peer is stalled for some period of time.
    //! It is not an indication that an error occurred, but an indication that there may
    //! be something wrong with the transport. It is up to the client to decide what to
    //! do when that happens, as the protocol will not automatically reset.
    //! This method may be invoked repeatedly by the protocol as it tries to continue to
    //! deliver packets to the peer and isn't receiving responses.
    //!
    //! @param[in] self The object on which this method is invoked.
    //! @param[in] stalled_time Number of milliseconds we have been stalled for so far.
    void (*NotifySessionStalled)(GG_GattlinkClient* self, uint32_t stalled_time);
};

//! @var GG_GattlinkClient::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_GattlinkClientInterface
//! Virtual function table for the GG_GattlinkClient interface

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::GetOutgoingDataAvailable
size_t GG_GattlinkClient_GetOutgoingDataAvailable(GG_GattlinkClient* self);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::GetOutgoingData
GG_Result GG_GattlinkClient_GetOutgoingData(GG_GattlinkClient* self, size_t offset, void* buffer, size_t size);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::ConsumeOutgoingData
void GG_GattlinkClient_ConsumeOutgoingData(GG_GattlinkClient* self, size_t size);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::NotifyIncomingDataAvailable
void GG_GattlinkClient_NotifyIncomingDataAvailable(GG_GattlinkClient* self);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::GetTransportMaxPacketSize
size_t GG_GattlinkClient_GetTransportMaxPacketSize(GG_GattlinkClient* self);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::SendRawData
GG_Result GG_GattlinkClient_SendRawData(GG_GattlinkClient* self, const void* buffer, size_t size);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::NotifySessionReady
void GG_GattlinkClient_NotifySessionReady(GG_GattlinkClient* self);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::NotifySessionReset
void GG_GattlinkClient_NotifySessionReset(GG_GattlinkClient* self);

//! @relates GG_GattlinkClient
//! @copydoc GG_GattlinkClientInterface::NotifySessionStalled
void GG_GattlinkClient_NotifySessionStalled(GG_GattlinkClient* self, uint32_t stalled_time);

//! Configuration information for the a GattLink Session
typedef struct {
    //! The max number of transport in-flight outbound packets at any given time
    uint8_t max_tx_window_size;
    //! The max number of transport in-flight inbound packets at any given time
    uint8_t max_rx_window_size;
} GG_GattlinkSessionConfig;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//! Create a Gattlink Protocol object.
//!
//! @param[in] client pointer to struct implementing the GG_GattlinkClient interface
//! @param[in] config The configuration for the gattlink session. See GG_GattlinkSessionConfig
//!                   for more details
//! @param[in] scheduler instance of a timer scheduler to use for timers
//! @param[out] protocol Pointer to where the new object instance will be returned
//! @return #GG_SUCCESS if the object could be created, or an error code.
GG_Result GG_GattlinkProtocol_Create(GG_GattlinkClient*              client,
                                     const GG_GattlinkSessionConfig* config,
                                     GG_TimerScheduler*              scheduler,
                                     GG_GattlinkProtocol**           protocol);

//! Clear up any state associated with the object. After this call is made the object
//! handle is no longer valid
//!
//! @param[in] protocol The protocol to destroy
void GG_GattlinkProtocol_Destroy(GG_GattlinkProtocol* protocol);

//! Open a GattLink session.
//!
//! Should be called when the underlying transport is ready for communication. This will kick off a
//! "Port Reset" sequence and whether or not it is successful will be communicated via the
//! a call to the client's NotifySessionUp implementation
//! @param[in] protocol The protocol to act on
//! @return #GG_SUCCESS on success, or an error code
GG_Result GG_GattlinkProtocol_Start(GG_GattlinkProtocol* protocol);

//! Reset a GattLink session.
//!
//! @param[in] protocol The protocol to act on
//! @return #GG_SUCCESS on success, or an error code
GG_Result GG_GattlinkProtocol_Reset(GG_GattlinkProtocol* protocol);

//! Inform the GattLink layer that there is more data ready to be sent. This effectively primes
//! GattLink to call the client's GattlinkGetOutgoingData implementation
//! @param[in] protocol The protocol to notify
void GG_GattlinkProtocol_NotifyOutgoingDataAvailable(GG_GattlinkProtocol* protocol);

//! Return the number of incoming bytes available for retrieving
//!
//! @param[in] protocol The protocol to receieve data on
//! @return The number of bytes available to be retrieved
size_t GG_GattlinkProtocol_GetIncomingDataAvailable(GG_GattlinkProtocol* protocol);

//! Copy the requested amount of incoming data ready to be retrieved
//! @note The data cannot be free'd upon completion of this call. It can only be free'd when
//!        GG_GattlinkProtocol_ConsumeIncomingData() is called
//!
//! @param[in] protocol The protocol to retrieve data from
//! @param[in] offset The index within the data buffer to start copying from
//! @param[out] buffer Pointer where to copy data to
//! @param[in] size Number of bytes to retrieve
//! @return #GG_SUCCESS on success, or an error code
GG_Result GG_GattlinkProtocol_GetIncomingData(GG_GattlinkProtocol* protocol,
                                              size_t               offset,
                                              void*                buffer,
                                              size_t               size);

//! Inform Gattlink when the received data has been successfully consumed.
//!
//! Gattlink may now free any allocations associated with these bytes
//! @param[in] protocol The protocol to consume receive data from
//! @param[in] size The number of bytes to consume
//! @return #GG_SUCCESS on success, or an error code
GG_Result GG_GattlinkProtocol_ConsumeIncomingData(GG_GattlinkProtocol* protocol, size_t size);

//! Inform Gattlink when new data is received over the transport layer. This data will be decoded
//! and eventually result in a call to the client GattlinkNotifyIncomingDataAvailable implementation.
//!
//! @param[in] protocol The protocol that should handle raw data which is received
//! @param[in] buffer The data just received over the transport layer
//! @param[in] size The number of bytes to receive
//! @return #GG_SUCCESS on success, or an error code
GG_Result GG_GattlinkProtocol_HandleIncomingRawData(GG_GattlinkProtocol* protocol, const void* buffer, size_t size);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
