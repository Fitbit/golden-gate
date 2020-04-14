/**
 * @file
 *
 * @copyright
 * Copyright 2017-2019 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-12-01
 *
 * @details
 *
 * CoAP splitter
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_coap_splitter.h"
#include "xp/coap/gg_coap.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.coap.handlers.proxy")

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSource);
    GG_IMPLEMENTS(GG_DataSinkListener);

    GG_DataSink*         sink;
    GG_DataSinkListener* sink_listener;
} GG_CoapSplitterPort;

struct GG_CoapSplitter {
    GG_IMPLEMENTS(GG_CoapRequestHandler);

    GG_CoapSplitterPort      top_port;
    GG_SocketAddressMetadata top_response_metadata;
    GG_CoapSplitterPort      bottom_port;
    GG_SocketAddressMetadata bottom_response_metadata;
    GG_CoapSplitterPort      side_port;
    GG_CoapEndpoint*         endpoint;

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_CoapSplitterPort_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_CoapSplitterPort* self = GG_SELF(GG_CoapSplitterPort, GG_DataSink);

    // store a reference to the listener
    self->sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_CoapSplitterPort_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_CoapSplitterPort* self = GG_SELF(GG_CoapSplitterPort, GG_DataSource);

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
static bool
GG_CoapSplitter_DatagramIsRequest(GG_Buffer* data)
{
    if (data && GG_Buffer_GetDataSize(data) >= 4) {
        uint8_t coap_code = GG_Buffer_GetData(data)[1];
        return coap_code >= 1 && coap_code <= 4;
    }
    return false;
}

//----------------------------------------------------------------------
static const uint8_t*
GG_CoapSplitter_GetTokenFromDatagram(GG_Buffer* data, size_t token_size)
{
    // check that the datagram has enough data to include a token of that size
    if (GG_Buffer_GetDataSize(data) < 4 + token_size) {
        return NULL;
    }

    // check that the declared token size is large enough
    const uint8_t* datagram = GG_Buffer_GetData(data);
    size_t token_length = datagram[0] & 0xF;
    if (token_length < token_size) {
        return NULL;
    }

    return datagram + 4;
}

//----------------------------------------------------------------------
static void
GG_CoapSplitter_UpdateResponseMetadata(const GG_BufferMetadata*  new_metadata,
                                       GG_SocketAddressMetadata* existing_metadata,
                                       const char*               port_name)
{
    if (new_metadata && new_metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS) {
        const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)new_metadata;
        if (!GG_IpAddress_Equal(&socket_metadata->socket_address.address,
                                &existing_metadata->socket_address.address) ||
            socket_metadata->socket_address.port != existing_metadata->socket_address.port) {
            // new/updated response metadata
#if defined(GG_CONFIG_ENABLE_LOGGING)
            char address_str[20];
            GG_SocketAddress_AsString(&socket_metadata->socket_address, address_str, sizeof(address_str));
            GG_LOG_FINE("binding %s port to %s", port_name, address_str);
#else
            GG_COMPILER_UNUSED(port_name);
#endif
            existing_metadata->socket_address = socket_metadata->socket_address;
        }
    }
}

//----------------------------------------------------------------------
// Called when a datagram is received from the top port.
// If the datagram is a request, just pass it through to the bottom port,
// and "bind" the top port to the address of the requester so that we can send it
// a response later on.
// If it is a response, send it to the bottom port.
//----------------------------------------------------------------------
static GG_Result
GG_CoapSplitter_Top_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_CoapSplitter* self = GG_SELF_M(top_port, GG_CoapSplitter, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    if (self->bottom_port.sink) {
        if (GG_CoapSplitter_DatagramIsRequest(data)) {
            GG_LOG_FINE("forwarding request from top to bottom");
            GG_CoapSplitter_UpdateResponseMetadata(metadata, &self->top_response_metadata, "top");
            return GG_DataSink_PutData(self->bottom_port.sink, data, NULL);
        } else {
            GG_LOG_FINE("forwarding response from top to bottom");
            return GG_DataSink_PutData(self->bottom_port.sink,
                                       data,
                                       self->bottom_response_metadata.socket_address.port ?
                                       &self->bottom_response_metadata.base :
                                       NULL);
        }
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_CoapSplitter_Top_OnCanPut(GG_DataSinkListener* _self)
{
    GG_CoapSplitter* self = GG_SELF_M(top_port, GG_CoapSplitter, GG_DataSinkListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // give a chance to the other ports to put data if they have any pending
    if (self->bottom_port.sink_listener) {
        GG_DataSinkListener_OnCanPut(self->bottom_port.sink_listener);
    }
    if (self->side_port.sink_listener) {
        GG_DataSinkListener_OnCanPut(self->side_port.sink_listener);
    }
}

//----------------------------------------------------------------------
// Called when a datagram is received from the bottom port.
// If the datagram is a request, or a response targetting the side CoAP, endpoint,
// route it to the side. Otherwise, it is a response for the top port.
//----------------------------------------------------------------------
static GG_Result
GG_CoapSplitter_Bottom_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_CoapSplitter* self = GG_SELF_M(bottom_port, GG_CoapSplitter, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check if this is a request or a response
    if (GG_CoapSplitter_DatagramIsRequest(data)) {
        // this is a request, forward it to the side
        GG_LOG_FINE("forwarding request from bottom to side");
        GG_CoapSplitter_UpdateResponseMetadata(metadata, &self->bottom_response_metadata, "bottom");
        if (self->side_port.sink) {
            return GG_DataSink_PutData(self->side_port.sink, data, metadata);
        }
    } else {
        // this is a response, decide whether it should go to the side or top
        bool route_to_side = false;
        size_t token_prefix_size = 0;
        const uint8_t* token_prefix = GG_CoapEndpoint_GetTokenPrefix(self->endpoint, &token_prefix_size);
        if (token_prefix_size) {
            // compare the prefix
            const uint8_t* token = GG_CoapSplitter_GetTokenFromDatagram(data, token_prefix_size);
            if (token && !memcmp(token_prefix, token, token_prefix_size)) {
                route_to_side = true;
            }
        }
        if (route_to_side) {
            if (self->side_port.sink) {
                GG_LOG_FINE("forwarding response from bottom to side");
                return GG_DataSink_PutData(self->side_port.sink, data, metadata);
            }
        } else {
            if (self->top_port.sink) {
                GG_LOG_FINE("forwarding response from bottom to top");
                return GG_DataSink_PutData(self->top_port.sink,
                                           data,
                                           self->top_response_metadata.socket_address.port ?
                                           &self->top_response_metadata.base :
                                           NULL);
            }
        }
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_CoapSplitter_Bottom_OnCanPut(GG_DataSinkListener* _self)
{
    GG_CoapSplitter* self = GG_SELF_M(bottom_port, GG_CoapSplitter, GG_DataSinkListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // give a chance to the other ports to put data if they have any pending
    if (self->top_port.sink_listener) {
        GG_DataSinkListener_OnCanPut(self->top_port.sink_listener);
    }
    if (self->side_port.sink_listener) {
        GG_DataSinkListener_OnCanPut(self->side_port.sink_listener);
    }
}

//----------------------------------------------------------------------
// Called when a datagram is received from the side CoAP endpoint.
// Just forward the datagram to the bottom port.
//----------------------------------------------------------------------
static GG_Result
GG_CoapSplitter_Side_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_CoapSplitter* self = GG_SELF_M(side_port, GG_CoapSplitter, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    if (self->bottom_port.sink) {
        GG_LOG_FINE("forwarding request from side to bottom");
        return GG_DataSink_PutData(self->bottom_port.sink, data, metadata);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_CoapSplitter_Side_OnCanPut(GG_DataSinkListener* _self)
{
    GG_CoapSplitter* self = GG_SELF_M(side_port, GG_CoapSplitter, GG_DataSinkListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // give a chance to bottom port to put data if it has any pending
    if (self->bottom_port.sink_listener) {
        GG_DataSinkListener_OnCanPut(self->bottom_port.sink_listener);
    }
}

//----------------------------------------------------------------------
// Called when the side CoAP endpoint has a request that none of its
// handlers is registered for. The request is forwarded to the top port.
//----------------------------------------------------------------------
static GG_CoapRequestHandlerResult
GG_CoapSplitter_OnRequest(GG_CoapRequestHandler*   _self,
                          GG_CoapEndpoint*         endpoint,
                          const GG_CoapMessage*    request,
                          GG_CoapResponder*        responder,
                          const GG_BufferMetadata* transport_metadata,
                          GG_CoapMessage**         response)
{
    GG_CoapSplitter* self = GG_SELF(GG_CoapSplitter, GG_CoapRequestHandler);
    GG_COMPILER_UNUSED(endpoint);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    // send the request datagram out to the top sink
    GG_Buffer* datagram = NULL;
    GG_Result result = GG_CoapMessage_ToDatagram(request, &datagram);
    if (GG_FAILED(result)) {
        return result;
    }
    if (self->top_port.sink) {
        GG_LOG_FINE("forwarding request to top port");
        result = GG_DataSink_PutData(self->top_port.sink, datagram, NULL);
        if (GG_FAILED(result)) {
            // we couldn't send, consider this packet dropped (don't send an error back)
            GG_LOG_WARNING("GG_DataSink_PutData returned %d, datagram dropped", result);
        }
    }
    GG_Buffer_Release(datagram);

    // return GG_ERROR_WOULD_BLOCK to indicate that we're not producing a response at this point,
    // as the response will come later, out of band
    *response = NULL;
    return GG_ERROR_WOULD_BLOCK;
}

//----------------------------------------------------------------------
GG_CoapRequestHandler*
GG_CoapSplitter_AsCoapRequestHandler(GG_CoapSplitter* self)
{
    return GG_CAST(self, GG_CoapRequestHandler);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_CoapSplitter_GetTopPortAsDataSource(GG_CoapSplitter* self)
{
    return GG_CAST(&self->top_port, GG_DataSource);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_CoapSplitter_GetTopPortAsDataSink(GG_CoapSplitter* self)
{
    return GG_CAST(&self->top_port, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_CoapSplitter_GetBottomPortAsDataSource(GG_CoapSplitter* self)
{
    return GG_CAST(&self->bottom_port, GG_DataSource);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_CoapSplitter_GetBottomPortAsDataSink(GG_CoapSplitter* self)
{
    return GG_CAST(&self->bottom_port, GG_DataSink);
}

//----------------------------------------------------------------------
GG_Result
GG_CoapSplitter_Create(GG_CoapEndpoint*  endpoint,
                       GG_CoapSplitter** splitter)
{
    // allocate the object
    GG_CoapSplitter* self = GG_AllocateZeroMemory(sizeof(GG_CoapSplitter));
    *splitter = self;
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup fields
    self->endpoint = endpoint;

    // setup the request handler function table
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter, GG_CoapRequestHandler) {
        .OnRequest = GG_CoapSplitter_OnRequest
    };
    GG_SET_INTERFACE(self, GG_CoapSplitter, GG_CoapRequestHandler);

    // setup the sink/source function tables for the top
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Top, GG_DataSink) {
        GG_CoapSplitter_Top_PutData,
        GG_CoapSplitterPort_SetListener
    };
    GG_SET_INTERFACE(&self->top_port, GG_CoapSplitter_Top, GG_DataSink);
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Top, GG_DataSource) {
        GG_CoapSplitterPort_SetDataSink
    };
    GG_SET_INTERFACE(&self->top_port, GG_CoapSplitter_Top, GG_DataSource);
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Top, GG_DataSinkListener) {
        GG_CoapSplitter_Top_OnCanPut
    };
    GG_SET_INTERFACE(&self->top_port, GG_CoapSplitter_Top, GG_DataSinkListener);

    // setup the sink/source function tables for the bottom
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Bottom, GG_DataSink) {
        GG_CoapSplitter_Bottom_PutData,
        GG_CoapSplitterPort_SetListener
    };
    GG_SET_INTERFACE(&self->bottom_port, GG_CoapSplitter_Bottom, GG_DataSink);
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Bottom, GG_DataSource) {
        GG_CoapSplitterPort_SetDataSink
    };
    GG_SET_INTERFACE(&self->bottom_port, GG_CoapSplitter_Bottom, GG_DataSource);
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Bottom, GG_DataSinkListener) {
        GG_CoapSplitter_Bottom_OnCanPut
    };
    GG_SET_INTERFACE(&self->bottom_port, GG_CoapSplitter_Bottom, GG_DataSinkListener);

    // setup the sink/source function tables for the side
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Side, GG_DataSink) {
        GG_CoapSplitter_Side_PutData,
        GG_CoapSplitterPort_SetListener
    };
    GG_SET_INTERFACE(&self->side_port, GG_CoapSplitter_Side, GG_DataSink);
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Side, GG_DataSource) {
        GG_CoapSplitterPort_SetDataSink
    };
    GG_SET_INTERFACE(&self->side_port, GG_CoapSplitter_Side, GG_DataSource);
    GG_IMPLEMENT_INTERFACE(GG_CoapSplitter_Side, GG_DataSinkListener) {
        GG_CoapSplitter_Side_OnCanPut
    };
    GG_SET_INTERFACE(&self->side_port, GG_CoapSplitter_Side, GG_DataSinkListener);

    // connect the endpoint
    GG_DataSource_SetDataSink(GG_CAST(&self->side_port, GG_DataSource), GG_CoapEndpoint_AsDataSink(endpoint));
    GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(endpoint), GG_CAST(&self->side_port, GG_DataSink));

    // setup the top and bottom socket metadata
    self->top_response_metadata.base    = GG_BUFFER_METADATA_INITIALIZER(DESTINATION_SOCKET_ADDRESS,
                                                                         GG_SocketAddressMetadata);
    self->bottom_response_metadata.base = GG_BUFFER_METADATA_INITIALIZER(DESTINATION_SOCKET_ADDRESS,
                                                                         GG_SocketAddressMetadata);

    // bind to the current thead
    GG_THREAD_GUARD_BIND(self);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_CoapSplitter_Destroy(GG_CoapSplitter* self)
{
    if (self == NULL) {
        return;
    }

    // disconnect the endpoint
    if (self->endpoint) {
        GG_DataSource_SetDataSink(GG_CoapEndpoint_AsDataSource(self->endpoint), NULL);
    }

    // de-register as a listener from all the sinks
    if (self->top_port.sink) {
        GG_DataSink_SetListener(self->top_port.sink, NULL);
    }
    if (self->bottom_port.sink) {
        GG_DataSink_SetListener(self->bottom_port.sink, NULL);
    }
    if (self->side_port.sink) {
        GG_DataSink_SetListener(self->side_port.sink, NULL);
    }

    GG_ClearAndFreeObject(self, 1);
}
