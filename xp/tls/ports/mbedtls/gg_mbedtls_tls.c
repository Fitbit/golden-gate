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
 * @date 2017-12-01
 *
 * @details
 *
 * mbedtls port of the TLS protocol.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mbedtls/version.h"
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
#if !defined(MBEDTLS_ALLOW_PRIVATE_ACCESS)
#define MBEDTLS_ALLOW_PRIVATE_ACCESS // Needed to access mbedtls_ssl_context.state
#endif
#endif

#include "mbedtls/ssl.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/entropy.h"
#include "mbedtls/debug.h"

#include "xp/annotations/gg_annotations.h"
#include "xp/common/gg_common.h"
#include "xp/sockets/gg_sockets.h"
#include "gg_mbedtls_tls.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.tls.mbedtls")

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
#define MBEDTLS_RESULT_PRINT_ARGS(x) ((x) < 0 ? "-0x" : "0x"), (int)((x) < 0 ? -(x) : (x))

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/

/*
 * Logging level
 *  - 0 No debug
 *  - 1 Error
 *  - 2 State change
 *  - 3 Informational
 *  - 4 Verbose
 */
#if defined(GG_CONFIG_ENABLE_LOGGING)
#if !defined(GG_CONFIG_MBEDTLS_LOGGING_LEVEL)
#define GG_CONFIG_MBEDTLS_LOGGING_LEVEL 4
#endif
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_DtlsProtocol {
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    struct {
        GG_IMPLEMENTS(GG_DataSink);
        GG_IMPLEMENTS(GG_DataSource);
        GG_IMPLEMENTS(GG_DataSinkListener);
        GG_DataSink*         sink;
        GG_DataSinkListener* sink_listener;
        GG_Buffer*           pending_out;
    } user_side;
    struct {
        GG_IMPLEMENTS(GG_DataSink);
        GG_IMPLEMENTS(GG_DataSource);
        GG_IMPLEMENTS(GG_DataSinkListener);
        GG_DataSink*             sink;
        GG_DataSinkListener*     sink_listener;
        GG_Buffer*               pending_out;
        GG_Buffer*               pending_in;
        size_t                   pending_in_offset;
        GG_SocketAddressMetadata socket_metadata;
    } transport_side;
    GG_TlsProtocolRole       role;
    GG_TlsProtocolState      state;
    GG_Result                last_error;
    bool                     in_advance;
    size_t                   max_datagram_size;
    GG_TimerScheduler*       timer_scheduler;
    int*                     cipher_suites;
    GG_DynamicBuffer*        psk_identity; // only set when in server role, to save some memory
    GG_TlsKeyResolver*       key_resolver;
    mbedtls_ssl_context      ssl_context;
    mbedtls_ssl_config       ssl_config;
#if !defined(GG_CONFIG_MBEDTLS_USE_PLATFORM_RNG)
    mbedtls_ctr_drbg_context ssl_ctr_drbg_context;
    mbedtls_entropy_context  ssl_entropy_context;
#endif
    GG_EventEmitterBase      event_emitter;

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static void GG_DtlsProtocol_AdvanceHandshake(GG_DtlsProtocol* self);
static void GG_DtlsProtocol_TransportSide_TryToFlush(GG_DtlsProtocol* self);

/*----------------------------------------------------------------------
|   2.x/3.x compatibility
+---------------------------------------------------------------------*/
#if MBEDTLS_VERSION_NUMBER < 0x020d0000
#define mbedtls_ssl_get_max_out_record_payload mbedtls_ssl_get_max_frag_len
#endif

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
MapErrorCode(int ssl_result)
{
    switch (ssl_result) {
        case 0:
            return GG_SUCCESS;

        case MBEDTLS_ERR_SSL_WANT_READ:
        case MBEDTLS_ERR_SSL_WANT_WRITE:
            return GG_ERROR_WOULD_BLOCK;

        case MBEDTLS_ERR_SSL_ALLOC_FAILED:
            return GG_ERROR_OUT_OF_MEMORY;

        case MBEDTLS_ERR_SSL_FATAL_ALERT_MESSAGE:
            return GG_ERROR_TLS_FATAL_ALERT_MESSAGE;

        case MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY:
            return GG_ERROR_TLS_UNKNOWN_IDENTITY;

#if defined(MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER)
        case MBEDTLS_ERR_SSL_ILLEGAL_PARAMETER:
            return GG_ERROR_TLS_ILLEGAL_PARAMETER;
#endif

#if defined(MBEDTLS_ERR_SSL_DECODE_ERROR)
        case MBEDTLS_ERR_SSL_DECODE_ERROR:
            return GG_ERROR_TLS_DECODE_ERROR;
#endif

#if defined(MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO)
        case MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO:
            return GG_ERROR_TLS_DECODE_ERROR;
#endif

#if defined(MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO)
        case MBEDTLS_ERR_SSL_BAD_HS_SERVER_HELLO:
            return GG_ERROR_TLS_DECODE_ERROR;
#endif

        default:
            GG_LOG_FINER("GG_FAILURE shadowing finer error: %d", ssl_result);
            return GG_FAILURE;
    }
}

#if defined(GG_CONFIG_ENABLE_LOGGING) && defined(MBEDTLS_DEBUG_C)
//----------------------------------------------------------------------
// Adapter to convert mbedtls logs to GG logs
//----------------------------------------------------------------------
static void
GG_DtlsProtocol_PrintDebugLog(void*       context,
                              int         level,
                              const char* file,
                              int         line,
                              const char* message)
{
    GG_COMPILER_UNUSED(context);

    // strip ending newlines
    int message_length = (int)strlen(message);
    while (message_length &&
           (message[message_length-1] == '\r' || message[message_length-1] == '\n')) {
        --message_length;
    }

    // map logging levels
    unsigned int gg_level;
    switch (level) {
        case 0:
            gg_level = GG_LOG_LEVEL_INFO;
            break;

        case 1:
        case 2:
            gg_level = GG_LOG_LEVEL_FINE;
            break;

        case 3:
            gg_level = GG_LOG_LEVEL_FINER;
            break;

        default:
            gg_level = GG_LOG_LEVEL_FINEST;
    }

    // send to the logger
    GG_LOG_STRING(_GG_LocalLogger,
                  level,
                  (&_GG_LocalLogger, gg_level, file, line, "", "%.*s", message_length, message));
}
#endif

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);

    GG_DtlsProtocol* self = GG_SELF(GG_DtlsProtocol, GG_Inspectable);
    const char* state_name = "";
    switch (self->state) {
        case GG_TLS_STATE_INIT:
            state_name = "INIT";
            break;

        case GG_TLS_STATE_HANDSHAKE:
            state_name = "HANDSHAKE";
            break;

        case GG_TLS_STATE_SESSION:
            state_name = "SESSION";
            break;

        case GG_TLS_STATE_ERROR:
            state_name = "ERROR";
            break;
    }
    GG_Inspector_OnString(inspector, "state",              state_name);
    GG_Inspector_OnInteger(inspector, "last_error",        self->last_error, GG_INSPECTOR_FORMAT_HINT_NONE);
    GG_Inspector_OnBoolean(inspector, "in_advance",        self->in_advance);
    GG_Inspector_OnInteger(inspector, "max_datagram_size", self->max_datagram_size, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);

    if (self->state == GG_TLS_STATE_SESSION) {
        GG_Inspector_OnString(inspector, "cipher_suite", mbedtls_ssl_get_ciphersuite(&self->ssl_context));
        GG_Inspector_OnString(inspector, "tls_version",  mbedtls_ssl_get_version(&self->ssl_context));
    }

    if (self->psk_identity) {
        GG_Inspector_OnBytes(inspector,
                             "psk_identity",
                             GG_DynamicBuffer_GetData(self->psk_identity),
                             GG_DynamicBuffer_GetDataSize(self->psk_identity));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_DtlsProtocol, GG_Inspectable) {
    .Inspect = GG_DtlsProtocol_Inspect
};

//----------------------------------------------------------------------
GG_Inspectable*
GG_DtlsProtocol_AsInspectable(GG_DtlsProtocol* self)
{
    return GG_CAST(self, GG_Inspectable);
}
#endif

//----------------------------------------------------------------------
static void
GG_DtlsProtocol_SetTimer(void* _self, uint32_t intermediate_delay, uint32_t final_delay)
{
    GG_DtlsProtocol* self = (GG_DtlsProtocol*)_self;
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(intermediate_delay);
    GG_COMPILER_UNUSED(final_delay);

    // not implemented yet
}

//----------------------------------------------------------------------
//   returns:
//  -1 if cancelled (fin_ms == 0),
//   0 if none of the delays have passed,
//   1 if only the intermediate delay has passed,
//   2 if the final delay has passed.
//----------------------------------------------------------------------
static int
GG_DtlsProtocol_GetTimer(void* _self)
{
    GG_DtlsProtocol* self = (GG_DtlsProtocol*)_self;
    GG_COMPILER_UNUSED(self);

    // not implemented yet

    return 0;
}

//----------------------------------------------------------------------
// Try to deliver any pensing data on the user side
//----------------------------------------------------------------------
static void
GG_DtlsProtocol_UserSide_TryToFlush(GG_DtlsProtocol* self)
{
    if (self->user_side.pending_out) {
        const GG_BufferMetadata* metadata = NULL;
        if (self->transport_side.socket_metadata.socket_address.port) {
            // we have transport socket metadata
            metadata = &self->transport_side.socket_metadata.base;
        }
        GG_Result result = GG_DataSink_PutData(self->user_side.sink,
                                               self->user_side.pending_out,
                                               metadata);
        if (result == GG_ERROR_WOULD_BLOCK) {
            GG_LOG_FINER("user data not delivered, will retry later");
        } else {
            if (GG_SUCCEEDED(result)) {
                GG_LOG_FINER("user data delivered");
            } else {
                GG_LOG_WARNING("user data not delivered (%d), dropping", result);
            }
            // the data was delivered or dropped, we don't need to hold on to it anymore
            GG_Buffer_Release(self->user_side.pending_out);
            self->user_side.pending_out = NULL;
        }
    }
}

//----------------------------------------------------------------------
// Read any data that may be available from the mbedtls object and
// try to deliver it to the user side, or keep it buffered if not possible
//----------------------------------------------------------------------
static void
GG_DtlsProtocol_UserSide_PumpData(GG_DtlsProtocol* self)
{
    // check that we have a sink
    if (self->user_side.sink == NULL) {
        return;
    }

    // if we have some data pending, try to deliver it now
    if (self->user_side.pending_out) {
        GG_DtlsProtocol_UserSide_TryToFlush(self);
    }

    // read as much as we can from mbedtls and deliver it to the user side
    while (self->user_side.pending_out == NULL) {
        // allocate a buffer to read into
        GG_DynamicBuffer* buffer = NULL;
        GG_Result result = GG_DynamicBuffer_Create(self->max_datagram_size, &buffer);
        if (GG_FAILED(result)) {
            GG_LOG_WARNING("can't allocate buffer");
            GG_LOG_COMMS_ERROR_CODE(GG_LIB_TLS_DATA_DROPPED, result);
            return;
        }

        // read the data that's available
        int bytes_read = mbedtls_ssl_read(&self->ssl_context,
                                          GG_DynamicBuffer_UseData(buffer),
                                          self->max_datagram_size);
        if (bytes_read < 0) {
            if (bytes_read != MBEDTLS_ERR_SSL_WANT_READ) {
                GG_LOG_WARNING("mbedtls_ssl_read failed (%s%x)", MBEDTLS_RESULT_PRINT_ARGS(bytes_read));
                GG_LOG_COMMS_ERROR_CODE(GG_LIB_TLS_READ_FAILED, bytes_read);
            }
            GG_DynamicBuffer_Release(buffer);
            return;
        }
        if (bytes_read == 0) {
            // no data read? strange...
            GG_DynamicBuffer_Release(buffer);
            return;
        }
        GG_DynamicBuffer_SetDataSize(buffer, (size_t)bytes_read);

        // try to send the data
        self->user_side.pending_out = GG_DynamicBuffer_AsBuffer(buffer);
        GG_DtlsProtocol_UserSide_TryToFlush(self);
    }
}

//----------------------------------------------------------------------
// Method called when the user wants to send data over the TLS session
//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_UserSide_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_DtlsProtocol* self = GG_SELF_M(user_side, GG_DtlsProtocol, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);
    GG_COMPILER_UNUSED(metadata);

    // check if the session is established
    if (self->state == GG_TLS_STATE_ERROR) {
        // we can't continue
        return GG_ERROR_EOS;
    } else if (self->state != GG_TLS_STATE_SESSION) {
        // we're waiting for the handshake to finish
        return GG_ERROR_WOULD_BLOCK;
    }

    // check that we can write this buffer
    if ((int)GG_Buffer_GetDataSize(data) > (int)mbedtls_ssl_get_max_out_record_payload(&self->ssl_context)) {
        return GG_ERROR_OUT_OF_RANGE;
    }

    // if there's still a buffer pending on the transport side, don't bother processing this
    // buffer now, as it will just block
    if (self->transport_side.pending_out) {
        return GG_ERROR_WOULD_BLOCK;
    }

    // write everything at once (this is over UDP, so we don't want to lose our original framing)
    size_t data_size = GG_Buffer_GetDataSize(data);
    int ssl_result = mbedtls_ssl_write(&self->ssl_context, GG_Buffer_GetData(data), data_size);
    if (ssl_result == MBEDTLS_ERR_SSL_WANT_READ || ssl_result == MBEDTLS_ERR_SSL_WANT_WRITE) {
        return GG_ERROR_WOULD_BLOCK;
    } else if (ssl_result < 0) {
        // error
        return MapErrorCode(ssl_result);
    } else if ((size_t)ssl_result != data_size) {
        GG_LOG_WARNING("mbedtls_ssl_write only accepted part of the data (%u our of %u)",
                       (int)ssl_result, (int)data_size);
        GG_LOG_COMMS_ERROR_CODE(GG_LIB_TLS_WRITE_FAILED, ssl_result);
        return GG_ERROR_INTERNAL;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_UserSide_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_DtlsProtocol* self = GG_SELF_M(user_side, GG_DtlsProtocol, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    self->user_side.sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_DtlsProtocol_UserSide, GG_DataSink) {
    GG_DtlsProtocol_UserSide_PutData,
    GG_DtlsProtocol_UserSide_SetListener
};

//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_UserSide_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_DtlsProtocol* self = GG_SELF_M(user_side, GG_DtlsProtocol, GG_DataSource);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // de-register as a listener from the current sink
    if (self->user_side.sink) {
        GG_DataSink_SetListener(self->user_side.sink, NULL);
    }

    // keep a reference to the new sink
    self->user_side.sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(&self->user_side, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_DtlsProtocol_UserSide, GG_DataSource) {
    GG_DtlsProtocol_UserSide_SetDataSink
};


//----------------------------------------------------------------------
// Method called when it may be possible to deliver data to the user side
//----------------------------------------------------------------------
static void
GG_DtlsProtocol_UserSide_OnCanPut(GG_DataSinkListener* _self)
{
    GG_DtlsProtocol* self = GG_SELF_M(user_side, GG_DtlsProtocol, GG_DataSinkListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // process any data that's available
    GG_DtlsProtocol_UserSide_PumpData(self);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_DtlsProtocol_UserSide, GG_DataSinkListener) {
    GG_DtlsProtocol_UserSide_OnCanPut
};

//----------------------------------------------------------------------
// Method called when data arrives from the transport side
//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_TransportSide_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_DtlsProtocol* self = GG_SELF_M(transport_side, GG_DtlsProtocol, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_LOG_FINER("received %d bytes from the transport", (int)GG_Buffer_GetDataSize(data));

    // remember the socket address if this packet came from a socket (only once)
    if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS) {
        if (self->transport_side.socket_metadata.socket_address.port == 0) {
            self->transport_side.socket_metadata = *(const GG_SocketAddressMetadata*)metadata;
#if defined(GG_CONFIG_ENABLE_LOGGING)
            char address_str[20];
            GG_SocketAddress_AsString(&self->transport_side.socket_metadata.socket_address,
                                      address_str,
                                      sizeof(address_str));
            GG_LOG_FINER("transport socket metadata set to %s", address_str);
#endif
        }
    }

    // if we already have pending data, don't accept this new buffer
    if (self->transport_side.pending_in) {
        GG_LOG_FINER("transport data already pending");
        return GG_ERROR_WOULD_BLOCK;
    }

    // keep this data so we can return it when asked
    self->transport_side.pending_in = GG_Buffer_Retain(data);
    self->transport_side.pending_in_offset = 0;

    // try to advance our handshake state
    if (self->state == GG_TLS_STATE_HANDSHAKE) {
        GG_DtlsProtocol_AdvanceHandshake(self);
    }

    // if we have a session, process data on the user side
    if (self->state == GG_TLS_STATE_SESSION) {
        GG_DtlsProtocol_UserSide_PumpData(self);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_TransportSide_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_DtlsProtocol* self = GG_SELF_M(transport_side, GG_DtlsProtocol, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    self->transport_side.sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_DtlsProtocol_TransportSide, GG_DataSink) {
    GG_DtlsProtocol_TransportSide_PutData,
    GG_DtlsProtocol_TransportSide_SetListener
};

//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_TransportSide_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_DtlsProtocol* self = GG_SELF_M(transport_side, GG_DtlsProtocol, GG_DataSource);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // de-register as a listener from the current sink
    if (self->transport_side.sink) {
        GG_DataSink_SetListener(self->transport_side.sink, NULL);
    }

    // keep a reference to the new sink
    self->transport_side.sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(&self->transport_side, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_DtlsProtocol_TransportSide, GG_DataSource) {
    GG_DtlsProtocol_TransportSide_SetDataSink
};

//----------------------------------------------------------------------
// Try to deliver any data that's ready for the transport side
//----------------------------------------------------------------------
static void
GG_DtlsProtocol_TransportSide_TryToFlush(GG_DtlsProtocol* self)
{
    if (self->transport_side.pending_out) {
        GG_Result result = GG_DataSink_PutData(self->transport_side.sink,
                                               self->transport_side.pending_out,
                                               NULL);
        if (GG_SUCCEEDED(result)) {
            // the data was delivered, we don't need to hold on to it anymore
            GG_LOG_FINER("transport data delivered");
            GG_Buffer_Release(self->transport_side.pending_out);
            self->transport_side.pending_out = NULL;
        } else {
            GG_LOG_FINER("transport data not delivered");
        }
    }
}

//----------------------------------------------------------------------
// Method called when it may be possible to deliver data to the transport
//----------------------------------------------------------------------
static void
GG_DtlsProtocol_TransportSide_OnCanPut(GG_DataSinkListener* _self)
{
    GG_DtlsProtocol* self = GG_SELF_M(transport_side, GG_DtlsProtocol, GG_DataSinkListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // try to send what we have until it would block
    while (self->transport_side.pending_out) {
        GG_DtlsProtocol_TransportSide_TryToFlush(self);

        if (self->transport_side.pending_out) {
            // no data could be delivered, we're done
            break;
        }

        // there's space in the buffer now, we may be able to advance our state
        // and/or notify the user side listener that it can send again
        if (self->state == GG_TLS_STATE_HANDSHAKE) {
            GG_DtlsProtocol_AdvanceHandshake(self);
        }
        if (self->state == GG_TLS_STATE_SESSION) {
            if (self->user_side.sink_listener) {
                GG_DataSinkListener_OnCanPut(self->user_side.sink_listener);
            }
        }
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_DtlsProtocol_TransportSide, GG_DataSinkListener) {
    GG_DtlsProtocol_TransportSide_OnCanPut
};

//----------------------------------------------------------------------
// Callback invoked by mbedtls when it needs to send data to the transport
//----------------------------------------------------------------------
static int
GG_DtlsProtocol_Send(void* _self, const unsigned char* buffer, size_t buffer_size)
{
    GG_DtlsProtocol* self = (GG_DtlsProtocol*)_self;

    GG_LOG_FINER("mbedtls wants to write %u bytes", (int)buffer_size);

    // check that we have a sink
    if (self->transport_side.sink == NULL) {
        return 0;
    }

    // shortcut, just in case
    if (buffer_size == 0) {
        return 0;
    }

    // if we still have pending data, return now
    if (self->transport_side.pending_out) {
        GG_LOG_FINEST("no transport space available to write");
        return MBEDTLS_ERR_SSL_WANT_WRITE;
    }

    // create a buffer to copy the data into
    GG_DynamicBuffer* data = NULL;
    GG_Result result = GG_DynamicBuffer_Create(buffer_size, &data);
    if (GG_FAILED(result)) {
        GG_LOG_COMMS_ERROR_CODE(GG_LIB_TLS_WRITE_FAILED, MBEDTLS_ERR_SSL_ALLOC_FAILED);
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }
    GG_DynamicBuffer_SetData(data, buffer, buffer_size);

    // try to send the data now
    self->transport_side.pending_out = GG_DynamicBuffer_AsBuffer(data);
    GG_DtlsProtocol_TransportSide_TryToFlush(self);

    // indicate that we took everything
    return (int)buffer_size;
}

//----------------------------------------------------------------------
// Callback invoked by mbedtls when it needs to read data from the transport
//----------------------------------------------------------------------
static int
GG_DtlsProtocol_Receive(void* _self, unsigned char* buffer, size_t buffer_size)
{
    GG_DtlsProtocol* self = (GG_DtlsProtocol*)_self;

    GG_LOG_FINER("mbedtls wants to read up to %u bytes", (int)buffer_size);

    // check if we have some data ready
    if (self->transport_side.pending_in == NULL) {
        GG_LOG_FINEST("no transport data available to read");
        return MBEDTLS_ERR_SSL_WANT_READ;
    }

    // return as much data as we can
    size_t pending_data_size = GG_Buffer_GetDataSize(self->transport_side.pending_in);
    GG_ASSERT(pending_data_size >= self->transport_side.pending_in_offset);
    size_t bytes_to_copy = GG_MIN(buffer_size, pending_data_size - self->transport_side.pending_in_offset);
    memcpy(buffer,
           GG_Buffer_GetData(self->transport_side.pending_in) + self->transport_side.pending_in_offset,
           bytes_to_copy);

    // adjust counters and check if we're done with the current buffer
    GG_LOG_FINER("returning %u bytes", (int)bytes_to_copy);
    self->transport_side.pending_in_offset += bytes_to_copy;
    if (self->transport_side.pending_in_offset == pending_data_size) {
        GG_LOG_FINER("pending data fully consumed");
        GG_Buffer_Release(self->transport_side.pending_in);
        self->transport_side.pending_in = NULL;
        self->transport_side.pending_in_offset = 0;
    }

    return (int)bytes_to_copy;
}

//----------------------------------------------------------------------
// Callback invoked by mbedtls when it needs a server key for a given identity
//----------------------------------------------------------------------
static int
GG_DtlsProtocol_ResolvePsk(void*               _self,
                          mbedtls_ssl_context* ssl_context,
                          const unsigned char* psk_identity,
                          size_t               psk_identity_size)
{
    GG_DtlsProtocol* self = (GG_DtlsProtocol*)_self;

    GG_LOG_FINE("resolving PSK identity, size=%u", (int)psk_identity_size);

    if (!self->key_resolver) {
        GG_LOG_WARNING("GG_TlsKeyResolver_ResolveKey failed...mo key resolver set.");
        return MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY;
    }

    uint8_t psk[GG_DTLS_MAX_PSK_SIZE];
    size_t  psk_size = sizeof(psk);
    GG_Result result = GG_TlsKeyResolver_ResolveKey(self->key_resolver,
                                                    psk_identity,
                                                    psk_identity_size,
                                                    psk,
                                                    &psk_size);
    GG_LOG_FINE("GG_TlsKeyResolver_ResolveKey returned %d", result);
    if (GG_FAILED(result)) {
#if defined(GG_CONFIG_ENABLE_LOGGING)
        GG_String identity = GG_String_Create("");
        GG_BytesToHexString(psk_identity, psk_identity_size, &identity, true);
        GG_LOG_WARNING("GG_TlsKeyResolver_ResolveKey failed to resolve key with identity %s with %d",
                       GG_CSTR(identity), result);
        GG_String_Destruct(&identity);
#endif
        return MBEDTLS_ERR_SSL_UNKNOWN_IDENTITY;
    }

    // remember the identity
    result = GG_DynamicBuffer_SetData(self->psk_identity, psk_identity, psk_identity_size);
    if (GG_FAILED(result)) {
        GG_LOG_SEVERE("GG_TlsKeyResolver_ResolveKey couldn't allocate buffer.");
        return MBEDTLS_ERR_SSL_ALLOC_FAILED;
    }

    // set the handshake key value
    return mbedtls_ssl_set_hs_psk(ssl_context, (const unsigned char*)psk, psk_size);
}

//----------------------------------------------------------------------
// Init the client-specific parts of the object
//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_InitClient(GG_DtlsProtocol* self, const GG_TlsClientOptions* options)
{
    // the client stats in an idle state, because the handshake procedure can't
    // start until a transport has been connected
    self->state = GG_TLS_STATE_INIT;

    // configure mbedtls with default settings
    int ssl_result = mbedtls_ssl_config_defaults(&self->ssl_config,
                                                 MBEDTLS_SSL_IS_CLIENT,
                                                 MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                                 MBEDTLS_SSL_PRESET_DEFAULT);
    if (ssl_result != 0) {
        GG_LOG_WARNING("mbedtls_ssl_config_defaults failed (%s%x)", MBEDTLS_RESULT_PRINT_ARGS(ssl_result));
        return MapErrorCode(ssl_result);
    }

    // psk config
    ssl_result = mbedtls_ssl_conf_psk(&self->ssl_config,
                                      (const unsigned char*)options->psk,
                                      options->psk_size,
                                      (const unsigned char*)options->psk_identity,
                                      options->psk_identity_size);
    if (ssl_result != 0) {
        GG_LOG_WARNING("mbedtls_ssl_conf_psk failed (%s%x)", MBEDTLS_RESULT_PRINT_ARGS(ssl_result));
        return MapErrorCode(ssl_result);
    }

    // remember the PSK identity
    GG_DynamicBuffer_SetData(self->psk_identity, options->psk_identity, options->psk_identity_size);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Init the server-specific parts of the object
//----------------------------------------------------------------------
static GG_Result
GG_DtlsProtocol_InitServer(GG_DtlsProtocol* self, const GG_TlsServerOptions* options)
{
    self->state = GG_TLS_STATE_INIT;

    // configure mbedtls with default settings
    int ssl_result = mbedtls_ssl_config_defaults(&self->ssl_config,
                                                 MBEDTLS_SSL_IS_SERVER,
                                                 MBEDTLS_SSL_TRANSPORT_DATAGRAM,
                                                 MBEDTLS_SSL_PRESET_DEFAULT);
    if (ssl_result != 0) {
        GG_LOG_WARNING("mbedtls_ssl_config_defaults failed (%s%x)", MBEDTLS_RESULT_PRINT_ARGS(ssl_result));
        return MapErrorCode(ssl_result);
    }

    // init other server-only fields
    self->key_resolver = options->key_resolver;

    // setup the PSK callbacks
    mbedtls_ssl_conf_psk_cb(&self->ssl_config, GG_DtlsProtocol_ResolvePsk, self);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Create a new instance
//----------------------------------------------------------------------
GG_Result
GG_DtlsProtocol_Create(GG_TlsProtocolRole   role,
                       const GG_TlsOptions* options,
                       size_t               max_datagram_size,
                       GG_TimerScheduler*   timer_scheduler,
                       GG_DtlsProtocol**    protocol)
{
    GG_Result result;

    GG_LOG_FINE("GG_DtlsProtocol_Create, sizeof(GG_DtlsProtocol) = %u", (int)sizeof(GG_DtlsProtocol));
#if defined(MBEDTLS_VERSION_C)
    char mbedtls_version_string[12];
    mbedtls_version_get_string(mbedtls_version_string);
    GG_LOG_FINE("MbedTLS compile time version = %s, runtime version = %s",
                MBEDTLS_VERSION_STRING,
                mbedtls_version_string);
#endif

    // check the arguments
    if (max_datagram_size < GG_DTLS_MIN_DATAGRAM_SIZE ||
        max_datagram_size > GG_DTLS_MAX_DATAGRAM_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // allocate a new object
    GG_DtlsProtocol* self = (GG_DtlsProtocol*)GG_AllocateZeroMemory(sizeof(GG_DtlsProtocol));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    // allocate a buffer for the PSK identity
    result = GG_DynamicBuffer_Create(0, &self->psk_identity);
    if (GG_FAILED(result)) {
        GG_FreeMemory(self);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // allocate space for the cipher suite list (0-terminated int list)
    if (options->cipher_suites_count) {
        self->cipher_suites = GG_AllocateZeroMemory((1 + options->cipher_suites_count) * sizeof(int));
        if (self->cipher_suites == NULL) {
            GG_FreeMemory(self);
            return GG_ERROR_OUT_OF_MEMORY;
        }
    }

    // init the new object
    GG_EventEmitterBase_Init(&self->event_emitter);
    self->role              = role;
    self->max_datagram_size = max_datagram_size;
    self->timer_scheduler   = timer_scheduler;

    // init the mbedtls parts
    int ssl_result;
#if defined(GG_CONFIG_MBEDTLS_USE_PLATFORM_RNG)
    // the RNG initialization is provided by the host platform
    extern GG_Result GG_mbedtls_ssl_conf_rng(mbedtls_ssl_config* ssl_config);
    result = GG_mbedtls_ssl_conf_rng(&self->ssl_config);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_mbedtls_ssl_conf_rng failed (%d)", result);
        goto end;
    }
#else
    // initialize a default local RNG
    mbedtls_ssl_init(&self->ssl_context);
    mbedtls_ssl_config_init(&self->ssl_config);

    // RNG init
    mbedtls_entropy_init(&self->ssl_entropy_context);
    mbedtls_ctr_drbg_init(&self->ssl_ctr_drbg_context);
    ssl_result = mbedtls_ctr_drbg_seed(&self->ssl_ctr_drbg_context,
                                       mbedtls_entropy_func,
                                       &self->ssl_entropy_context,
                                       (const unsigned char *)"JUST_FOR_TESTING",
                                       16);
    if (ssl_result != 0) {
        GG_LOG_WARNING("mbedtls_ctr_drbg_seed failed (%s%x)", MBEDTLS_RESULT_PRINT_ARGS(ssl_result));
        result = MapErrorCode(ssl_result);
        goto end;
    }
    mbedtls_ssl_conf_rng(&self->ssl_config, mbedtls_ctr_drbg_random, &self->ssl_ctr_drbg_context);
#endif

    // client/server specific init
    if (role == GG_TLS_ROLE_CLIENT) {
        result = GG_DtlsProtocol_InitClient(self, (const GG_TlsClientOptions*)options);
    } else {
        result = GG_DtlsProtocol_InitServer(self, (const GG_TlsServerOptions*)options);
    }
    if (GG_FAILED(result)) {
        goto end;
    }

#if defined(GG_CONFIG_ENABLE_LOGGING) && defined(MBEDTLS_DEBUG_C)
    // debugging/logging support
    mbedtls_debug_set_threshold(GG_CONFIG_MBEDTLS_LOGGING_LEVEL);
    mbedtls_ssl_conf_dbg(&self->ssl_config, GG_DtlsProtocol_PrintDebugLog, NULL);
#endif

    // do not enable cookies (NULL callbacks)
    mbedtls_ssl_conf_dtls_cookies(&self->ssl_config, NULL, NULL, NULL);

    // cipher suites config
    if (options->cipher_suites_count) {
        for (size_t i = 0; i < options->cipher_suites_count; i++) {
            self->cipher_suites[i] = options->cipher_suites[i];
        }
        mbedtls_ssl_conf_ciphersuites(&self->ssl_config, self->cipher_suites);
    }

    // enable anti-replay
    mbedtls_ssl_conf_dtls_anti_replay(&self->ssl_config, MBEDTLS_SSL_ANTI_REPLAY_ENABLED);

    // context setup
#if defined(GG_CONFIG_MBEDTLS_USE_PLATFORM_SETUP)
    // platform-provided setup
    extern int GG_mbedtls_ssl_setup(mbedtls_ssl_context* ssl_context, const mbedtls_ssl_config* ssl_config);
    ssl_result = GG_mbedtls_ssl_setup(&self->ssl_context, &self->ssl_config);
#else
    // standard setup
    ssl_result = mbedtls_ssl_setup(&self->ssl_context, &self->ssl_config);
#endif
    if (ssl_result != 0) {
        GG_LOG_WARNING("mbedtls_ssl_setup failed (%s%x)", MBEDTLS_RESULT_PRINT_ARGS(ssl_result));
        result = MapErrorCode(ssl_result);
        goto end;
    }

    // timer callbacks
    mbedtls_ssl_set_timer_cb(&self->ssl_context, self, GG_DtlsProtocol_SetTimer, GG_DtlsProtocol_GetTimer);

    // I/O callbacks
    mbedtls_ssl_set_bio(&self->ssl_context,
                        self,
                        GG_DtlsProtocol_Send,
                        GG_DtlsProtocol_Receive,
                        NULL);

    // set the interfaces
    GG_SET_INTERFACE(&self->user_side,      GG_DtlsProtocol_UserSide,      GG_DataSink);
    GG_SET_INTERFACE(&self->user_side,      GG_DtlsProtocol_UserSide,      GG_DataSource);
    GG_SET_INTERFACE(&self->user_side,      GG_DtlsProtocol_UserSide,      GG_DataSinkListener);
    GG_SET_INTERFACE(&self->transport_side, GG_DtlsProtocol_TransportSide, GG_DataSink);
    GG_SET_INTERFACE(&self->transport_side, GG_DtlsProtocol_TransportSide, GG_DataSource);
    GG_SET_INTERFACE(&self->transport_side, GG_DtlsProtocol_TransportSide, GG_DataSinkListener);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(self, GG_DtlsProtocol, GG_Inspectable));

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

end:
    if (result == GG_SUCCESS) {
        // return the object
        *protocol = self;
    } else {
        // return the error
        if (self) {
            GG_DtlsProtocol_Destroy(self);
        }
        *protocol = NULL;
    }

    return result;
}

//----------------------------------------------------------------------
void
GG_DtlsProtocol_Destroy(GG_DtlsProtocol* self)
{
    if (self == NULL) return;

    GG_THREAD_GUARD_CHECK_BINDING(self);

    // de-init the object
    mbedtls_ssl_free(&self->ssl_context);
    mbedtls_ssl_config_free(&self->ssl_config);

#if !defined(GG_CONFIG_MBEDTLS_USE_PLATFORM_RNG)
    mbedtls_ctr_drbg_free(&self->ssl_ctr_drbg_context);
    mbedtls_entropy_free(&self->ssl_entropy_context);
#endif

    // de-register as a listener from the sinks
    if (self->user_side.sink) {
        GG_DataSink_SetListener(self->user_side.sink, NULL);
    }
    if (self->transport_side.sink) {
        GG_DataSink_SetListener(self->transport_side.sink, NULL);
    }

    // other cleanup
    if (self->transport_side.pending_out) {
        GG_Buffer_Release(self->transport_side.pending_out);
    }
    if (self->transport_side.pending_in) {
        GG_Buffer_Release(self->transport_side.pending_in);
    }
    if (self->user_side.pending_out) {
        GG_Buffer_Release(self->user_side.pending_out);
    }
    if (self->cipher_suites) {
        GG_FreeMemory(self->cipher_suites);
    }
    if (self->psk_identity) {
        GG_DynamicBuffer_Release(self->psk_identity);
    }

    // de-allocate the object
    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
static void
GG_DtlsProtocol_EmitStateChangeEvent(GG_DtlsProtocol* self)
{
    if (self->event_emitter.listener) {
        GG_Event event = {
            .type   = GG_EVENT_TYPE_TLS_STATE_CHANGE,
            .source = self
        };
        GG_EventListener_OnEvent(self->event_emitter.listener, &event);
    }
}

//----------------------------------------------------------------------
// Method called while we're in the handshake phase.
// It will make all the steps it can until either
//   - No progress can be made because there's not enough transport data
//     available or transport data could not be sent without blocking
//   - An error occurred
//   - The handshake has completed successfully
//----------------------------------------------------------------------
static void
GG_DtlsProtocol_AdvanceHandshake(GG_DtlsProtocol* self)
{
    // check that we're not re-entering
    if (self->in_advance) {
        GG_LOG_WARNING("unexpected re-entrance");
        return;
    }

    // mark that we're entering this function, to detect any re-entrance
    self->in_advance = true;

    // keep working until something would block
    for (;;) {
        GG_TlsProtocolState previous_state = self->state;
        switch (self->state) {
            case GG_TLS_STATE_INIT:
                GG_LOG_FINEST("state = GG_TLS_STATE_INIT");
                self->state = GG_TLS_STATE_HANDSHAKE;
                break;

            case GG_TLS_STATE_HANDSHAKE: {
                GG_LOG_FINEST("state = GG_TLS_STATE_HANDSHAKE");
                if (self->ssl_context.state == MBEDTLS_SSL_HANDSHAKE_OVER) {
                    GG_LOG_FINE("ssl handshake completed");
                    GG_LOG_FINE("ssl cipher suite: %s", mbedtls_ssl_get_ciphersuite(&self->ssl_context));
                    GG_LOG_FINE("ssl version: %s", mbedtls_ssl_get_version(&self->ssl_context));

                    self->state = GG_TLS_STATE_SESSION;

                    // in case there's pending data waiting to be sent after the handshake, notify
                    // the sink listener
                    if (self->user_side.sink_listener) {
                        GG_DataSinkListener_OnCanPut(self->user_side.sink_listener);
                    }
                    break;
                }

                GG_LOG_FINE("calling mbedtls_ssl_handshake_step");
                int ssl_result = mbedtls_ssl_handshake_step(&self->ssl_context);

                switch (ssl_result) {
                    case 0:
                        GG_LOG_FINE("mbedtls_ssl_handshake_step returned 0");
                        break;

                    case MBEDTLS_ERR_SSL_WANT_READ:
                        GG_LOG_FINE("mbedtls_ssl_handshake_step returned MBEDTLS_ERR_SSL_WANT_READ");
                        self->in_advance = false;
                        return;

                    case MBEDTLS_ERR_SSL_WANT_WRITE:
                        GG_LOG_FINE("mbedtls_ssl_handshake_step returned MBEDTLS_ERR_SSL_WANT_WRITE");
                        self->in_advance = false;
                        return;

                    default:
                        GG_LOG_FINE("mbedtls_ssl_handshake_step returned %s%x", MBEDTLS_RESULT_PRINT_ARGS(ssl_result));
                        self->state = GG_TLS_STATE_ERROR;
                        self->last_error = MapErrorCode(ssl_result);
                        GG_LOG_COMMS_ERROR_CODE(GG_LIB_TLS_HANDSHAKE_ERROR, self->last_error);
                        break;
                }
                break;
            }

            case GG_TLS_STATE_ERROR:
                GG_LOG_FINEST("state = GG_TLS_STATE_ERROR");
                self->in_advance = false;

                return;

            case GG_TLS_STATE_SESSION:
                GG_LOG_FINEST("state = GG_TLS_STATE_SESSION");
                self->in_advance = false;

                // show how much expansion we can expect given the selected cipher
#if defined(GG_CONFIG_ENABLE_LOGGING)
                int expansion = mbedtls_ssl_get_record_expansion(&self->ssl_context);
                GG_LOG_FINE("max record expansion = %d", expansion);
#endif
                return;
        }

        // emit an event on state change
        if (previous_state != self->state) {
            GG_DtlsProtocol_EmitStateChangeEvent(self);
        }

        // after an error in server mode, go back to init and re-handshake
        if (self->role == GG_TLS_ROLE_SERVER && self->state == GG_TLS_STATE_ERROR) {
            GG_LOG_FINE("resetting session");
            GG_Result result = GG_DtlsProtocol_Reset(self);
            if (GG_SUCCEEDED(result)) {
                self->state = GG_TLS_STATE_HANDSHAKE;
                GG_DtlsProtocol_EmitStateChangeEvent(self);
            }
        }
    }
}

//----------------------------------------------------------------------
// Put the object in a state where the handshake can happen.
// For clients, this may send some data to the transport side.
//----------------------------------------------------------------------
GG_Result
GG_DtlsProtocol_StartHandshake(GG_DtlsProtocol* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    if (self->state != GG_TLS_STATE_INIT) {
        return GG_ERROR_INVALID_STATE;
    }

    self->state = GG_TLS_STATE_HANDSHAKE;
    GG_DtlsProtocol_EmitStateChangeEvent(self);
    GG_DtlsProtocol_AdvanceHandshake(self);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_DtlsProtocol_Reset(GG_DtlsProtocol* self)
{
    if (self->state == GG_TLS_STATE_INIT) {
        GG_LOG_FINE("ignoring reset, we're already in the INIT state");
        return GG_SUCCESS;
    }

    int ssl_result = mbedtls_ssl_session_reset(&self->ssl_context);
    if (ssl_result != 0) {
        GG_LOG_WARNING("mbedtls_ssl_session_reset failed (%s%x)", MBEDTLS_RESULT_PRINT_ARGS(ssl_result));
        return MapErrorCode(ssl_result);
    } else {
        self->last_error = GG_SUCCESS;
        self->state = GG_TLS_STATE_INIT;
        GG_DtlsProtocol_EmitStateChangeEvent(self);

        return GG_SUCCESS;
    }
}

//----------------------------------------------------------------------
GG_EventEmitter*
GG_DtlsProtocol_AsEventEmitter(GG_DtlsProtocol* self)
{
    return GG_CAST(&self->event_emitter, GG_EventEmitter);
}

//----------------------------------------------------------------------
void
GG_DtlsProtocol_GetStatus(GG_DtlsProtocol* self, GG_DtlsProtocolStatus* status)
{
    *status = (GG_DtlsProtocolStatus) {
        .state      = self->state,
        .last_error = self->last_error
    };

    // the PSK identity depends on the state and the role
    if (status->state == GG_TLS_STATE_SESSION) {
        status->psk_identity_size = GG_DynamicBuffer_GetDataSize(self->psk_identity);
        status->psk_identity      = status->psk_identity_size ? GG_DynamicBuffer_GetData(self->psk_identity) : NULL;
    }
}

//----------------------------------------------------------------------
GG_DataSink*
GG_DtlsProtocol_GetUserSideAsDataSink(GG_DtlsProtocol* self)
{
    return GG_CAST(&self->user_side, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_DtlsProtocol_GetUserSideAsDataSource(GG_DtlsProtocol* self)
{
    return GG_CAST(&self->user_side, GG_DataSource);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_DtlsProtocol_GetTransportSideAsDataSink(GG_DtlsProtocol* self)
{
    return GG_CAST(&self->transport_side, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource* GG_DtlsProtocol_GetTransportSideAsDataSource(GG_DtlsProtocol* self)
{
    return GG_CAST(&self->transport_side, GG_DataSource);
}
