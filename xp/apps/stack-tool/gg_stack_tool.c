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
 * @date 2018-06-12
 *
 * @details
 *
 * Stack Tool
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <ctype.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_ring_buffer.h"
#include "xp/common/gg_memory.h"
#include "xp/module/gg_module.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/tls/gg_tls.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/stack_builder/gg_stack_builder.h"
#include "xp/coap/gg_coap.h"
#include "xp/utils/gg_blaster_data_source.h"
#include "xp/utils/gg_perf_data_sink.h"
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
#include "gg_stack_tool_core_bluetooth_transport.h"
#endif
#if defined(GG_CONFIG_ENABLE_MACOS_TUNNEL)
#include "gg_stack_tool_macos_tunnel.h"
#endif

//----------------------------------------------------------------------
// constants
//----------------------------------------------------------------------
#define GG_STACK_TOOL_MAX_DATAGRAM_SIZE                 1280
#define GG_STACK_TOOL_DEFAULT_HUB_BOTTOM_SEND_PORT      9000
#define GG_STACK_TOOL_DEFAULT_HUB_BOTTOM_RECEIVE_PORT   9001
#define GG_STACK_TOOL_DEFAULT_HUB_TOP_SEND_PORT         9100
#define GG_STACK_TOOL_DEFAULT_HUB_TOP_RECEIVE_PORT      9101
#define GG_STACK_TOOL_DEFAULT_NODE_BOTTOM_SEND_PORT     GG_STACK_TOOL_DEFAULT_HUB_BOTTOM_RECEIVE_PORT
#define GG_STACK_TOOL_DEFAULT_NODE_BOTTOM_RECEIVE_PORT  GG_STACK_TOOL_DEFAULT_HUB_BOTTOM_SEND_PORT
#define GG_STACK_TOOL_DEFAULT_NODE_TOP_SEND_PORT        9200
#define GG_STACK_TOOL_DEFAULT_NODE_TOP_RECEIVE_PORT     9201
#define GG_STACK_TOOL_DEFAULT_HUB_COMMAND_PORT          7000
#define GG_STACK_TOOL_DEFAULT_HUB_EVENT_PORT            7100
#define GG_STACK_TOOL_DEFAULT_NODE_COMMAND_PORT         7001
#define GG_STACK_TOOL_DEFAULT_NODE_EVENT_PORT           7101
#define GG_STACK_TOOL_KEY_SIZE                          16
#define GG_STACK_TOOL_MAX_DTLS_IDENTITY_SIZE            256

#define GG_STACK_TOOL_SEND_ANSI_COLOR                   "\033[35;1m"
#define GG_STACK_TOOL_RECEIVE_ANSI_COLOR                "\033[34;1m"
#define GG_STACK_TOOL_ANSI_COLOR_RESET                  "\033[0m"

//----------------------------------------------------------------------
// globals
//----------------------------------------------------------------------
static GG_BlasterDataSource* g_blaster;
static GG_PerfDataSink*      g_perf_sink;

#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
static GG_StackToolBluetoothTransport* g_bluetooth_transport;
static char* g_bluetooth_id = NULL;
#endif

//----------------------------------------------------------------------
// types and constants
//----------------------------------------------------------------------
typedef enum {
    GG_STACK_TOOL_TOP_UDP,
    GG_STACK_TOOL_TOP_BLAST
#if defined(GG_CONFIG_ENABLE_MACOS_TUNNEL)
    , GG_STACK_TOOL_TOP_TUNNEL
#endif
} StackToolTopType;

//----------------------------------------------------------------------
// forward declarations
//----------------------------------------------------------------------
static void PskResolver_AddDtlsKey(const char* identity_and_key);

//----------------------------------------------------------------------
// convert a 4CC to a string
//----------------------------------------------------------------------
static void
Convert4CCToString(uint32_t code, char* string)
{
    snprintf(string, 5, "%c%c%c%c",
             (char)(code >> 24),
             (char)(code >> 16),
             (char)(code >>  8),
             (char)(code      ));
}

//----------------------------------------------------------------------
// Print connection arrows
//----------------------------------------------------------------------
static void
ShowPortArrows(bool source, bool sink)
{
    if (source && sink) {
        printf("       ^          |\n"
               "       |          v\n");
    } else if (source) {
        printf("                  |\n"
               "                  v\n");
    } else if (sink) {
        printf("       ^           \n"
               "       |           \n");
    } else {
        printf("       |          |\n");
    }
}

//----------------------------------------------------------------------
// Print connection names
//----------------------------------------------------------------------
static void
ShowPortNames(bool source, bool sink, bool top)
{
    const char* sink_str   = sink   ? "[ sink ]" : "[......]";
    const char* source_str = source ? "[source]" : "[......]";
    if (top) {
        printf("   %s   %s\n", source_str, sink_str);
    } else {
        printf("   %s   %s\n", sink_str, source_str);
    }
}

//----------------------------------------------------------------------
// Print out info about a stack element
//----------------------------------------------------------------------
static void
ShowStackElement(GG_Stack* stack, const GG_StackElementInfo* element_info)
{
    GG_Result result;

    // print the top connections
    GG_StackElementPortInfo port_info;
    result = GG_Stack_GetPortById(stack, element_info->id, GG_STACK_PORT_ID_TOP, &port_info);
    if (GG_SUCCEEDED(result)) {
        ShowPortNames(port_info.source != NULL, port_info.sink != NULL, true);
    }

    // print the element type
    char type_str[5];
    Convert4CCToString(element_info->type, type_str);
    printf("+-----------------------+\n");
    printf("|   (%s)    id=%4d   |\n", type_str, (int)element_info->id);
    printf("+-----------------------+\n");

    // print the bottom connections
    result = GG_Stack_GetPortById(stack, element_info->id, GG_STACK_PORT_ID_BOTTOM, &port_info);
    if (GG_SUCCEEDED(result)) {
        ShowPortNames(port_info.source != NULL, port_info.sink != NULL, false);
        ShowPortArrows(port_info.source != NULL, port_info.sink != NULL);
    }
}

//----------------------------------------------------------------------
// Print out info about all the visible elements of a stack
//----------------------------------------------------------------------
static void
ShowStack(GG_Stack* stack)
{
    for (unsigned int index = 0;; index++) {
        GG_StackElementInfo element_info;
        GG_Result result = GG_Stack_GetElementByIndex(stack, index, &element_info);
        if (GG_FAILED(result)) {
            break;
        }
        if (index == 0) {
            GG_StackElementPortInfo port_info;
            result = GG_Stack_GetPortById(stack, element_info.id, GG_STACK_PORT_ID_TOP, &port_info);
            if (GG_SUCCEEDED(result)) {
                ShowPortArrows(port_info.source != NULL, port_info.sink != NULL);
            }
        }
        ShowStackElement(stack, &element_info);
    }
}

//----------------------------------------------------------------------
// Print out the IP configuration
//----------------------------------------------------------------------
static void
ShowIpConfig(GG_Stack* stack)
{
    printf("\nIP Config:\n");
    GG_StackIpConfiguration ip_config;
    GG_Stack_GetIpConfiguration(stack, &ip_config);
    printf("Local Address:  %d.%d.%d.%d\n"
           "Remote Address: %d.%d.%d.%d\n"
           "IP MTU:         %d\n",
           (int)ip_config.local_address.ipv4[0],
           (int)ip_config.local_address.ipv4[1],
           (int)ip_config.local_address.ipv4[2],
           (int)ip_config.local_address.ipv4[3],
           (int)ip_config.remote_address.ipv4[0],
           (int)ip_config.remote_address.ipv4[1],
           (int)ip_config.remote_address.ipv4[2],
           (int)ip_config.remote_address.ipv4[3],
           (int)ip_config.ip_mtu);
}

//----------------------------------------------------------------------
// Display a CoAP message
//----------------------------------------------------------------------
static void
ShowCoapMessage(GG_CoapMessage* message, const char* color_escape)
{
    unsigned int code = GG_CoapMessage_GetCode(message);
    printf("%s", color_escape);
    printf("MSG code         = %d.%02d\n", GG_COAP_MESSAGE_CODE_CLASS(code), GG_COAP_MESSAGE_CODE_DETAIL(code));
    const char* type_str = "";
    switch (GG_CoapMessage_GetType(message)) {
        case GG_COAP_MESSAGE_TYPE_CON:
            type_str = "CON";
            break;

        case GG_COAP_MESSAGE_TYPE_NON:
            type_str = "NON";
            break;

        case GG_COAP_MESSAGE_TYPE_ACK:
            type_str = "ACK";
            break;

        case GG_COAP_MESSAGE_TYPE_RST:
            type_str = "RST";
            break;
    }
    printf("MSG type         = %s\n", type_str);
    printf("MSG id           = %d\n", GG_CoapMessage_GetMessageId(message));
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_length = GG_CoapMessage_GetToken(message, token);
    char token_hex[2*GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH+1];
    GG_BytesToHex(token, token_length, token_hex, true);
    token_hex[2*token_length] = 0;
    printf("MSG token        = %s\n", token_hex);

    GG_CoapMessageOptionIterator option_iterator;
    GG_CoapMessage_InitOptionIterator(message, GG_COAP_MESSAGE_OPTION_ITERATOR_FILTER_ANY, &option_iterator);
    while (option_iterator.option.number) {
        switch (option_iterator.option.type) {
            case GG_COAP_MESSAGE_OPTION_TYPE_UINT:
                printf("MSG option %u (uint): %u\n",
                       (int)option_iterator.option.number,
                       (int)option_iterator.option.value.uint);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_STRING:
                printf("MSG option %u (string): %.*s\n",
                       (int)option_iterator.option.number,
                       (int)option_iterator.option.value.string.length,
                       option_iterator.option.value.string.chars);
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE:
                printf("MSG option %u (opaque): size=%u : bytes=",
                       (int)option_iterator.option.number,
                       (int)option_iterator.option.value.opaque.size);
                for (int i=0; i < (int)option_iterator.option.value.opaque.size; i++) {
                    printf("%.2x ", option_iterator.option.value.opaque.bytes[i]);
                }
                printf("\n");
                break;

            case GG_COAP_MESSAGE_OPTION_TYPE_EMPTY:
                printf("MSG option %u (empty)\n",
                       (int)option_iterator.option.number);
                break;
        }

        GG_CoapMessage_StepOptionIterator(message, &option_iterator);
    }

    printf("MSG payload size = %d\n", (int)GG_CoapMessage_GetPayloadSize(message));

    if (GG_CoapMessage_GetPayloadSize(message)) {
        if (GG_COAP_MESSAGE_CODE_CLASS(code) >= GG_COAP_MESSAGE_CODE_CLASS_CLIENT_ERROR_RESPONSE) {
            // if code class is an error class, lets check for extended error payload
            GG_CoapExtendedError extended_error;
            if (GG_CoapExtendedError_Decode(&extended_error, GG_CoapMessage_GetPayload(message),
                                            (int)GG_CoapMessage_GetPayloadSize(message)) == GG_SUCCESS) {
                printf("MSG extended error namespace:      = %.*s\n",
                       (int)extended_error.name_space_size, extended_error.name_space);
                printf("MSG extended error code            = %d\n",
                       extended_error.code);
                printf("MSG extended error message         = %.*s\n",
                       (int)extended_error.message_size, extended_error.message);
            }
        }
    }
    printf(GG_STACK_TOOL_ANSI_COLOR_RESET);
}

//----------------------------------------------------------------------
// Setup the blaster
//----------------------------------------------------------------------
static void
SetupBlaster(GG_DataSource* top_source,
             GG_DataSink*   top_sink,
             size_t         packet_count,
             size_t         packet_size)
{
    // create a performance-measuring sink
    GG_Result result = GG_PerfDataSink_Create(GG_PERF_DATA_SINK_MODE_BASIC_OR_IP_COUNTER,
                                              GG_PERF_DATA_SINK_OPTION_PRINT_STATS_TO_CONSOLE |
                                              GG_PERF_DATA_SINK_OPTION_AUTO_RESET_STATS,
                                              1000, // print stats every second
                                              &g_perf_sink);
    GG_ASSERT(result == GG_SUCCESS);

    // connect the perf sink to the stack
    GG_DataSource_SetDataSink(top_source, GG_PerfDataSink_AsDataSink(g_perf_sink));

    // start a blaster if required
    if (packet_count && packet_size) {
        // init a blaster instance
        result = GG_BlasterDataSource_Create(packet_size,
                                             GG_BLASTER_IP_COUNTER_PACKET_FORMAT,
                                             packet_count,
                                             NULL, // no timer
                                             0,    // no timer
                                             &g_blaster);
        GG_ASSERT(result == GG_SUCCESS);

        // connect the blaster to the stack
        GG_DataSource_SetDataSink(GG_BlasterDataSource_AsDataSource(g_blaster), top_sink);

        // start the blaster
        GG_BlasterDataSource_Start(g_blaster);
    }
}

//----------------------------------------------------------------------
// Cleanup the blaster
//----------------------------------------------------------------------
static void
CleanupBlaster(void)
{
    GG_BlasterDataSource_Destroy(g_blaster);
    GG_PerfDataSink_Destroy(g_perf_sink);
}

//----------------------------------------------------------------------
// Socket wrapper that can strip out the metadata of buffers and
// keep track of the last received datagram in order to be able to
// respond to the sender without having to know its port number ahead
// of time.
// The socket wrapper can also display a trace of data that flows through
// it, either raw/unknown or as CoAP datagrams.
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DatagramSocket);
    GG_IMPLEMENTS(GG_DataSource);
    struct {
        GG_IMPLEMENTS(GG_DataSink);
    } in_sink;
    struct {
        GG_IMPLEMENTS(GG_DataSink);
        GG_DataSink* sink;
    } out_sink;

    GG_DatagramSocket*       socket;
    GG_SocketAddressMetadata metadata;
    bool                     autobind;
    bool                     coap_mode;
    uint16_t                 coap_request_send_port;
    bool                     trace;
    char*                    name;
} SocketWrapper;

static GG_DataSink*
SocketWrapper_AsDataSink(GG_DatagramSocket* _self)
{
    SocketWrapper* self = GG_SELF(SocketWrapper, GG_DatagramSocket);

    return GG_CAST(&self->in_sink, GG_DataSink);
}

static GG_DataSource*
SocketWrapper_AsDataSource(GG_DatagramSocket* _self)
{
    SocketWrapper* self = GG_SELF(SocketWrapper, GG_DatagramSocket);

    return GG_CAST(self, GG_DataSource);
}

static void
SocketWrapper_Destroy(GG_DatagramSocket* _self)
{
    SocketWrapper* self = GG_SELF(SocketWrapper, GG_DatagramSocket);

    GG_DatagramSocket_Destroy(self->socket);
    free(self->name);
    GG_FreeMemory(self);
}

static GG_Result
SocketWrapper_Attach(GG_DatagramSocket* _self, GG_Loop* loop)
{
    SocketWrapper* self = GG_SELF(SocketWrapper, GG_DatagramSocket);

    return GG_DatagramSocket_Attach(self->socket, loop);
}

static GG_Result
SocketWrapper_In_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    SocketWrapper* self = GG_SELF_M(in_sink, SocketWrapper, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    GG_CoapMessage* coap_message = NULL;
    if (self->coap_mode) {
        (void)GG_CoapMessage_CreateFromDatagram(data, &coap_message);
    }
    if (self->trace) {
        printf(GG_STACK_TOOL_SEND_ANSI_COLOR "<<< [%s] %d bytes\n" GG_STACK_TOOL_ANSI_COLOR_RESET,
               self->name,
               (int)GG_Buffer_GetDataSize(data));
        if (coap_message) {
            ShowCoapMessage(coap_message, GG_STACK_TOOL_SEND_ANSI_COLOR);
        }
    }

    // decide where to send this
    GG_BufferMetadata* send_metadata = NULL;
    if (self->metadata.socket_address.port) {
        send_metadata = &self->metadata.base;
    }

    // check if we need to look for a CoAP request
    GG_SocketAddressMetadata coap_request_send_metadata =
        GG_DESTINATION_SOCKET_ADDRESS_METADATA_INITIALIZER(GG_IpAddress_Any, self->coap_request_send_port);
    if (self->coap_request_send_port && coap_message) {
        uint8_t coap_message_code = GG_CoapMessage_GetCode(coap_message);
        if (coap_message_code >= 1 && coap_message_code <= 4) {
            // this is a CoAP request
            send_metadata = &coap_request_send_metadata.base;
        }
    }
    GG_CoapMessage_Destroy(coap_message);

    return GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(self->socket), data, send_metadata);
}

static GG_Result
SocketWrapper_In_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    SocketWrapper* self = GG_SELF_M(in_sink, SocketWrapper, GG_DataSink);

    return GG_DataSink_SetListener(GG_DatagramSocket_AsDataSink(self->socket), listener);
}

static GG_Result
SocketWrapper_Out_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    SocketWrapper* self = GG_SELF_M(out_sink, SocketWrapper, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    // analyze the CoAP message if in CoAP mode
    GG_CoapMessage* coap_message = NULL;
    if (self->coap_mode) {
        (void)GG_CoapMessage_CreateFromDatagram(data, &coap_message);
    }

    // trace if needed
    if (self->trace) {
        printf(GG_STACK_TOOL_RECEIVE_ANSI_COLOR ">>> [%s] %u bytes\n" GG_STACK_TOOL_ANSI_COLOR_RESET,
               self->name,
               (int)GG_Buffer_GetDataSize(data));
        if (coap_message) {
            ShowCoapMessage(coap_message, GG_STACK_TOOL_RECEIVE_ANSI_COLOR);
        }
    }

    // remember the metadata
    if (self->autobind && metadata && metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS) {
        const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)metadata;
        if (socket_metadata->socket_address.port != self->metadata.socket_address.port ||
            !GG_IpAddress_Equal(&socket_metadata->socket_address.address, &self->metadata.socket_address.address)) {
            bool should_bind = true;

            // check if this is a CoAP request
            if (coap_message) {
                uint8_t coap_message_code = GG_CoapMessage_GetCode(coap_message);
                if (coap_message_code == 0 || coap_message_code > 4) {
                    // this is not a CoAP request, don't bind
                    should_bind = false;
                }
            }

            if (should_bind) {
                if (self->trace) {
                    printf("### binding to socket address %u.%u.%u.%u:%u\n",
                           socket_metadata->socket_address.address.ipv4[0],
                           socket_metadata->socket_address.address.ipv4[1],
                           socket_metadata->socket_address.address.ipv4[2],
                           socket_metadata->socket_address.address.ipv4[3],
                           socket_metadata->socket_address.port);
                }
                self->metadata = *socket_metadata;
            }
        }
    }

    // cleanup
    GG_CoapMessage_Destroy(coap_message);

    if (self->out_sink.sink) {
        return GG_DataSink_PutData(self->out_sink.sink, data, NULL);
    } else {
        return GG_ERROR_INTERNAL;
    }
}

static GG_Result
SocketWrapper_Out_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    SocketWrapper* self = GG_SELF_M(out_sink, SocketWrapper, GG_DataSink);

    return GG_DataSink_SetListener(GG_DatagramSocket_AsDataSink(self->socket), listener);
}

static GG_Result
SocketWrapper_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    SocketWrapper* self = GG_SELF(SocketWrapper, GG_DataSource);

    self->out_sink.sink = sink;
    return GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(self->socket),
                                     GG_CAST(&self->out_sink, GG_DataSink));
}

GG_IMPLEMENT_INTERFACE(SocketWrapper, GG_DatagramSocket) {
    .AsDataSink   = SocketWrapper_AsDataSink,
    .AsDataSource = SocketWrapper_AsDataSource,
    .Destroy      = SocketWrapper_Destroy,
    .Attach       = SocketWrapper_Attach
};

GG_IMPLEMENT_INTERFACE(SocketWrapper, GG_DataSource) {
    .SetDataSink = SocketWrapper_SetDataSink
};

GG_IMPLEMENT_INTERFACE(SocketWrapper_In, GG_DataSink) {
    .PutData     = SocketWrapper_In_PutData,
    .SetListener = SocketWrapper_In_SetListener
};

GG_IMPLEMENT_INTERFACE(SocketWrapper_Out, GG_DataSink) {
    .PutData     = SocketWrapper_Out_PutData,
    .SetListener = SocketWrapper_Out_SetListener
};

static GG_Result
SocketWrapper_Create(GG_DatagramSocket*  socket,
                     const char*         name,
                     bool                autobind,
                     bool                coap_mode,
                     uint16_t            coap_request_send_port,
                     bool                trace,
                     GG_DatagramSocket** wrapper)
{
    // allocate a new object
    SocketWrapper* self = (SocketWrapper*)GG_AllocateZeroMemory(sizeof(SocketWrapper));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // set members
    self->socket                 = socket;
    self->name                   = strdup(name);
    self->autobind               = autobind;
    self->trace                  = trace;
    self->coap_mode              = coap_mode;
    self->coap_request_send_port = coap_request_send_port;

    // set interfaces
    GG_SET_INTERFACE(self, SocketWrapper, GG_DatagramSocket);
    GG_SET_INTERFACE(self, SocketWrapper, GG_DataSource);
    GG_SET_INTERFACE(&self->in_sink, SocketWrapper_In, GG_DataSink);
    GG_SET_INTERFACE(&self->out_sink, SocketWrapper_Out, GG_DataSink);

    *wrapper = GG_CAST(self, GG_DatagramSocket);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Create a BSD socket
//----------------------------------------------------------------------
static GG_Result
CreateSocket(GG_Loop*            loop,
             const char*         name,
             bool                coap_mode,
             const char*         send_host_ip,
             uint16_t            send_port,
             uint16_t            coap_request_send_port,
             uint16_t            receive_port,
             bool                trace,
             GG_DatagramSocket** socket)
{
    GG_DatagramSocket* inner_socket = NULL;
    GG_SocketAddress local_address  = { GG_IP_ADDRESS_NULL_INITIALIZER, receive_port };
    GG_SocketAddress remote_address = { GG_IpAddress_Any,               send_port    };
    GG_IpAddress_SetFromString(&remote_address.address, send_host_ip);
    GG_Result result = GG_BsdDatagramSocket_Create(receive_port ? &local_address  : NULL,
                                                   send_port    ? &remote_address : NULL,
                                                   false,
                                                   GG_STACK_TOOL_MAX_DATAGRAM_SIZE,
                                                   &inner_socket);
    if (GG_FAILED(result)) {
        return result;
    }

    // attach the socket to the loop
    result = GG_DatagramSocket_Attach(inner_socket, loop);
    if (GG_FAILED(result)) {
        GG_DatagramSocket_Destroy(inner_socket);
        return result;
    }

    // wrap the socket
    result = SocketWrapper_Create(inner_socket,
                                  name,
                                  send_port == 0,
                                  coap_mode,
                                  coap_request_send_port,
                                  trace,
                                  socket);
    if (GG_FAILED(result)) {
        GG_DatagramSocket_Destroy(inner_socket);
        *socket = NULL;

        return result;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// Simple event listener that prints out events
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_EventListener);

    GG_Stack*    stack;
    bool         start_on_link_up;
    bool         stack_started;
    GG_DataSink* remote_event_sink;
} StackListener;

static void
StackListener_OnEvent(GG_EventListener* _self, const GG_Event* event)
{
    StackListener* self = GG_SELF(StackListener, GG_EventListener);
    char type_str[5];
    char remote_event_str[1024] = {0};

    GG_COMPILER_UNUSED(self);

    Convert4CCToString(event->type, type_str);
    printf(">>> Event: type=%s\n", type_str);
    switch (event->type) {
        case GG_EVENT_TYPE_STACK_EVENT_FORWARD: {
            const GG_StackForwardEvent* forward_event = (const GG_StackForwardEvent*)event;
            Convert4CCToString(forward_event->forwarded->type, type_str);
            printf("   Forwarded Event: type=%s\n", type_str);

            switch (forward_event->forwarded->type) {
                case GG_EVENT_TYPE_TLS_STATE_CHANGE: {
                    GG_DtlsProtocolStatus dtls_status;
                    GG_DtlsProtocol* dtls_protocol = (GG_DtlsProtocol*)forward_event->forwarded->source;
                    GG_DtlsProtocol_GetStatus(dtls_protocol, &dtls_status);
                    switch (dtls_status.state) {
                        case GG_TLS_STATE_INIT:
                            printf("        DTLS State: INIT\n");
                            break;

                        case GG_TLS_STATE_HANDSHAKE:
                            printf("        DTLS State: HANDSHAKE\n");
                            break;

                        case GG_TLS_STATE_SESSION:
                            printf("        DTLS State: SESSION\n");
                            if (dtls_status.psk_identity_size) {
                                char* hex = GG_AllocateMemory(dtls_status.psk_identity_size * 2 + 1);
                                hex[dtls_status.psk_identity_size * 2] = '\0';
                                GG_BytesToHex(dtls_status.psk_identity, dtls_status.psk_identity_size, hex, true);
                                printf("            PSK Identity: %s (", hex);
                                for (size_t i = 0; i < dtls_status.psk_identity_size; i++) {
                                    uint8_t c = (char)dtls_status.psk_identity[i];
                                    if (!isprint(c)) {
                                        c = ' '; // just print a space for unprintable chars
                                    }
                                    printf("%c", c);
                                }
                                printf(")\n");
                                GG_FreeMemory(hex);
                            }
                            break;

                        case GG_TLS_STATE_ERROR:
                            printf("        DTLS State: ERROR (%d)\n", dtls_status.last_error);
                            break;
                    }
                    break;
                }

                case GG_EVENT_TYPE_GATTLINK_SESSION_STALLED: {
                    const GG_GattlinkStalledEvent* stalled_event =
                        (const GG_GattlinkStalledEvent*)forward_event->forwarded;
                    printf("        Gattlink Stall (%u ms)\n", (int)stalled_event->stalled_time);
                    break;
                }

                default:
                    break;
            }
            break;
        }

#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
        case GG_EVENT_TYPE_BLUETOOTH_TRANSPORT_SCAN: {
            const GG_StackToolBluetoothTransportScanEvent* scan_event =
                (const GG_StackToolBluetoothTransportScanEvent*)event;
            printf("Bluetooth Scan: %s - ID = %s RSSI = %d\n",
                   scan_event->peripheral_name,
                   scan_event->peripheral_id,
                   scan_event->rssi);
            snprintf(remote_event_str,
                     sizeof(remote_event_str),
                     "{\"type\":\"bt-scan\", \"details\":{\"name\":\"%s\", \"id\": \"%s\", \"rssi\": %d}}",
                     scan_event->peripheral_name,
                     scan_event->peripheral_id,
                     scan_event->rssi);
            break;
        }

        case GG_EVENT_TYPE_BLUETOOTH_LINK_CONNECTED_EVENT: {
            printf("Bluetooth Link Up\n");
            if (self->start_on_link_up) {
                if (!self->stack_started) {
                    GG_Stack_Start(self->stack);
                    self->stack_started = true;
                }
            }
            snprintf(remote_event_str,
                     sizeof(remote_event_str),
                     "{\"type\":\"bt-link-up\", \"details\":{\"id\": \"%s\"}}",
                     g_bluetooth_id ? g_bluetooth_id : "");
            break;
        }

        case GG_EVENT_TYPE_BLUETOOTH_LINK_STATUS_CONENCTION_CONFIG_EVENT: {
            const GG_StackToolBluetoothTransportLinkStatusConnectionConfigEvent* lc_event =
                (const GG_StackToolBluetoothTransportLinkStatusConnectionConfigEvent*)event;
            snprintf(remote_event_str,
                     sizeof(remote_event_str),
                     "{"
                     "\"type\":\"bt-link-status-connection-config\","
                     "\"details\":{"
                     "\"connection_interval\": %u,"
                     "\"slave_latency\": %u,"
                     "\"supervision_timeout\": %u,"
                     "\"mtu\": %u,"
                     "\"mode\": %u"
                     "}}",
                     lc_event->connection_interval,
                     lc_event->slave_latency,
                     lc_event->supervision_timeout,
                     lc_event->mtu,
                     lc_event->mode);
            break;
        }
#endif
    }

    // send the event to a remote over the event socket
    if (remote_event_str[0]) {
        size_t data_size = strlen(remote_event_str);
        GG_DynamicBuffer* data = NULL;
        GG_DynamicBuffer_Create(data_size, &data);
        GG_DynamicBuffer_SetData(data, (const uint8_t*)remote_event_str, data_size);
        GG_DataSink_PutData(self->remote_event_sink, GG_DynamicBuffer_AsBuffer(data), NULL);
        GG_DynamicBuffer_Release(data);
    }
}

GG_IMPLEMENT_INTERFACE(StackListener, GG_EventListener) {
    .OnEvent = StackListener_OnEvent
};

//----------------------------------------------------------------------
// Command Listener
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    GG_Stack* stack;
} CommandListener;

static GG_Result
CommandListener_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    CommandListener* self = GG_SELF(CommandListener, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);
    GG_String command_string;
    GG_String_Init(command_string);
    GG_String_AssignN(&command_string, (const char*)GG_Buffer_GetData(data), GG_Buffer_GetDataSize(data));

    printf("*** Received command: %s\n", GG_CSTR(command_string));

    if (GG_String_Equals(&command_string, "@reset", false)) {
        printf("*** Resetting stack\n");
        GG_Stack_Reset(self->stack);
    } else if (GG_String_StartsWith(&command_string, "@dtls-add-key:")) {
        printf("*** Adding DTLS key\n");
        PskResolver_AddDtlsKey(GG_String_GetChars(&command_string) + 14);
    }
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
    else if (GG_String_StartsWith(&command_string, "@bt-connect:")) {
        const char* uuid = GG_String_GetChars(&command_string) + 12;
        if (g_bluetooth_id) {
            free(g_bluetooth_id);
            g_bluetooth_id = NULL;
        }
        g_bluetooth_id = strdup(uuid);
        GG_StackToolBluetoothTransport_Connect(g_bluetooth_transport, uuid);
    } else if (GG_String_StartsWith(&command_string, "@bt-lc-set-mode:")) {
        const char* mode_string = GG_String_GetChars(&command_string) + 16;
        uint8_t mode;
        if (!strcmp(mode_string, "fast")) {
            mode = 0;
        } else if (!strcmp(mode_string, "slow")) {
            mode = 1;
        } else {
            fprintf(stderr, "!!! ERROR: unknown mode %s\n", mode_string);
            return GG_SUCCESS;
        }
        GG_StackToolBluetoothTransport_SetPreferredConnectionMode(g_bluetooth_transport, mode);
    }
#endif

    // cleanup
    GG_String_Destruct(&command_string);

    return GG_SUCCESS;
}

static GG_Result
CommandListener_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(CommandListener, GG_DataSink) {
    .PutData     = CommandListener_PutData,
    .SetListener = CommandListener_SetListener
};

//----------------------------------------------------------------------
// Object that can resolve keys
//----------------------------------------------------------------------
typedef struct Psk Psk;
struct Psk {
    uint8_t  identity[GG_STACK_TOOL_MAX_DTLS_IDENTITY_SIZE];
    size_t   identity_size;
    uint8_t  key[GG_STACK_TOOL_KEY_SIZE];
    Psk*     next;
};

typedef struct {
    GG_IMPLEMENTS(GG_TlsKeyResolver);

    Psk* psks;
} StaticPskResolver;

static GG_Result
StaticPskResolver_ResolvePsk(GG_TlsKeyResolver* _self,
                             const uint8_t*     key_identity,
                             size_t             key_identity_size,
                             uint8_t*           key,
                             size_t*            key_size)
{
    StaticPskResolver* self = (StaticPskResolver*)GG_SELF(StaticPskResolver, GG_TlsKeyResolver);

    // we only support 16-byte keys
    if (*key_size < GG_STACK_TOOL_KEY_SIZE) {
        *key_size = GG_STACK_TOOL_KEY_SIZE;
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // look for a match in the list
    Psk* psk = self->psks;
    while (psk) {
        if (key_identity_size == psk->identity_size &&
            !memcmp(key_identity, psk->identity, key_identity_size)) {
            // match! copy the key
            memcpy(key, psk->key, GG_STACK_TOOL_KEY_SIZE);
            *key_size = GG_STACK_TOOL_KEY_SIZE;

            return GG_SUCCESS;
        }

        psk = psk->next;
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

GG_IMPLEMENT_INTERFACE(StaticPskResolver, GG_TlsKeyResolver) {
    StaticPskResolver_ResolvePsk
};

static StaticPskResolver PskResolver;

static void PskResolver_AddDtlsKey(const char* identity_and_key)
{
    const char* separator = strchr(identity_and_key, ':');
    if (!separator) {
        return;
    }
    const char* key_hex = separator + 1;
    if (strlen(key_hex) != 32) {
        return;
    }
    if (separator - identity_and_key > GG_STACK_TOOL_MAX_DTLS_IDENTITY_SIZE) {
        return;
    }
    Psk* psk = (Psk*)GG_AllocateZeroMemory(sizeof(Psk));
    psk->identity_size = separator - identity_and_key;
    memcpy(psk->identity, (const uint8_t*)identity_and_key, psk->identity_size);
    GG_HexToBytes(key_hex, 32, psk->key);
    psk->next = PskResolver.psks;
    PskResolver.psks = psk;

    printf("*** DTLS key added\n");
}

//----------------------------------------------------------------------
// Send a command
//----------------------------------------------------------------------
static void
SendCommand(GG_DatagramSocket* socket, const char* command)
{
    GG_StaticBuffer buffer;
    GG_StaticBuffer_Init(&buffer, (const uint8_t*)command, strlen(command));
    GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(socket),
                        GG_StaticBuffer_AsBuffer(&buffer),
                        NULL);
}

//----------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------
static Psk DefaultPsk = {
    .identity = { 'B', 'O', 'O', 'T', 'S', 'T', 'R', 'A', 'P' },
    .identity_size = 9,
    .key = {
        0x81, 0x06, 0x54, 0xe3, 0x36, 0xad, 0xca, 0xb0,
        0xa0, 0x3c, 0x60, 0xf7, 0x4a, 0xa0, 0xb6, 0xfb
    }
};

//----------------------------------------------------------------------
// Main entry point
//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    if (argc < 3) {
        printf("usage:\n"
               "  gg-stack-tool [options] hub|node <stack-descriptor-or-command>\n"
               "\n"
               "where <stack-descriptor-or-command> is either a stack descriptor or a command\n"
               "string starting with a @ character\n"
               "\n"
               "options:\n"
               "  --top [coap] <send_host_ip> <send_port> <receive_port>\n"
               "    Specify the IP address and port number to connect to the top of the stack (user).\n"
               "    If the 'coap' option is used, packets sent and received through the top of the stack\n"
               "    are assumed to be CoAP datagrams.\n"
               "    (default: 127.0.0.1 9002 9003 as a hub and 127.0.0.1 9003 9002 as a node)\n"
               "\n"
               "  --top blast <packet-count> <packet-size>\n"
               "    If <packet-count> is not 0, attach a packet blaster to the top of the stack, with the\n"
               "    given paket count and packet size, and start blasting, as well as printing stats for\n"
               "    packets received from a remote blastera. If <packet-count> or <packet-size> is 0,\n"
               "    don't start blasting, only print stats.\n"
               "\n"
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
               "  --top tunnel\n"
               "    Connect the top of the stack on an IP tunnel (typically only useful for stacks where the\n"
               "    top of the stack produces/consumes IP packets).\n"
               "\n"
#endif
               "  --bottom <send_host_ip> <send_port> <receive_port>\n"
               "    Specify the IP address and port number to connect to the bottom of the stack (transport).\n"
               "    (default: 127.0.0.1 9000 9001 as a hub and 127.0.0.1 9001 9000 as a node)\n"
               "\n"
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
               "  --bottom bluetooth <bluetooth-device-id>|scan|node:<advertised-name>\n"
               "    In the 'hub' role, connect the bottom of the stack directly to a Bluetooth peripheral,\n"
               "    connecting to the device with ID <bluetooth-device-id> (obtained by scanning).\n"
               "    Use 'scan' to only scan and display device IDs.\n"
               "    In the 'node' role, accept connections from a Bluetooth central\n"
               "    (this is mutually exclusive with the --bottom option above).\n"
               "\n"
               "  --force-start\n"
               "    Don't wait for a link up event from the transport before starting the stack (only\n"
               "    valid when the bottom of the stack is bluetooth).\n"
               "\n"
#endif
               "  --gattlink <max-fragment-size> <rx-window> <tx-window>\n"
               "    Specify the Gattlink parameters.\n"
               "\n"
               "  --dtls-key <key-identity>:<key>\n"
               "    Where <key-identity> is an ASCII string, and <key> is 16 bytes in hex (32 characters)\n"
               "    For the `hub` role, multiple --dtls-key options can be used to specify a list of\n"
               "    keys. For the `node` role, only one key can be specified.\n"
               "\n"
               "  --enable-header-compression\n"
               "    Enable header compression\n"
               "\n"
               "  --trace\n"
               "    Show packets as they are received or sent from the top and bottom sockets.\n"
               "\n"
               "  --command-port <command_port>\n"
               "    Receive commands on port <command_port> (default: 7000 for hub, 7001 for node).\n"
               "\n"
               "  --event-port <event_port>\n"
               "    Send events on port <event_port> (default: 7100 for hub, 7101 for node).\n"
               "\n"
               "commands:\n"
               "    @reset                             : reset the stack\n"
               "    @dtls-add-key:<key-identity>:<key> : add a DTLS key\n"
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
               "    @bt-connect:<uuid>|scan   : connect to a bluetooth device or scan\n"
               "    @bt-lc-set-mode:slow|fast : set the preferred link controller connection mode\n"
#endif
               "\n"
               "NOTES:\n"
               "  * Specify a port number of 0 for the send port of the top or bottom to indicate\n"
               "    that the socket should send to the IP address and port number of the latest received\n"
               "    packet instead of a fixed address and port.\n"
               "  * Specify a port number as 0/X, with X non-zero, for the send port of the top, to\n"
               "    indicate that CoAP requests going through the top should be sent to port X, but CoAP\n"
               "    responses should be sent to the port number from which they were received.\n"
               "  * Specify a port number of 0 for the receive port of the top or bottom to indicate\n"
               "    that the network stack should pick any available port number.\n");
        return 1;
    }

    // parse parameters
    GG_StackRole     role                        = GG_STACK_ROLE_HUB;
    bool             role_parsed                 = false;
    const char*      stack_descriptor_or_command = NULL;
    StackToolTopType top_type                    = GG_STACK_TOOL_TOP_UDP;
    bool             top_coap_mode               = false;
    const char*      top_send_host_ip            = "127.0.0.1";
    uint16_t         top_send_port               = 0;
    uint16_t         top_coap_request_send_port  = 0;
    uint16_t         top_receive_port            = 0;
    bool             top_ports_set               = false;
    const char*      bottom_send_host_ip         = "127.0.0.1";
    uint16_t         bottom_send_port            = 0;
    uint16_t         bottom_receive_port         = 0;
    bool             bottom_ports_set            = false;
    size_t           blast_packet_count          = 0;
    size_t           blast_packet_size           = 0;
    uint16_t         gattlink_max_fragment_size  = 0;
    uint8_t          gattlink_rx_window          = 0;
    uint8_t          gattlink_tx_window          = 0;
    bool             enable_header_compression   = false;
    bool             trace                       = false;
    bool             command_mode                = false;
    uint16_t         command_port                = 0;
    uint16_t         event_port                  = 0;
    Psk*             psks                        = NULL;
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
    bool             force_start                 = false;
#endif
    bool             wait_for_link_up            = false;
    GG_Result        result;

    // don't buffer stdout (makes it easier when used in a pipeline)
    setvbuf(stdout, NULL, _IONBF, 0);

    char* arg;
    while ((arg = *++argv)) {
        if (!strcmp("--top", arg)) {
            if (!argv[1] || !argv[2] || !argv[3]) {
                fprintf(stderr, "ERROR: --top option requires 3 or 4 arguments\n");
                return 1;
            }
            if (!strcmp(argv[1], "blast")) {
                top_type = GG_STACK_TOOL_TOP_BLAST;
                blast_packet_count = (size_t)strtoul(argv[2], NULL, 10);
                blast_packet_size  = (size_t)strtoul(argv[3], NULL, 10);
                argv += 3;
#if defined(GG_CONFIG_ENABLE_MACOS_TUNNEL)
            } else if (!strcmp(argv[1], "tunnel")) {
                top_type = GG_STACK_TOOL_TOP_TUNNEL;
                argv += 1;
#endif
            } else {
                top_send_host_ip = *++argv;
                if (!strcmp(top_send_host_ip, "coap")) {
                    if (!argv[3]) {
                        fprintf(stderr, "ERROR: --top option with coap requires 4 arguments\n");
                    }
                    top_coap_mode = true;
                    top_send_host_ip = *++argv;
                }

                const char* top_send_port_spec = *++argv;
                char* slash = strchr(top_send_port_spec, '/');
                if (slash) {
                    *slash = '\0';
                    top_coap_request_send_port = (uint16_t)strtoul(slash + 1, NULL, 10);
                }
                top_send_port    = (uint16_t)strtoul(top_send_port_spec, NULL, 10);
                top_receive_port = (uint16_t)strtoul(*++argv, NULL, 10);
                top_ports_set    = true;
            }
        } else if (!strcmp("--bottom", arg)) {
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
            if (!argv[1] || !argv[2]) {
                fprintf(stderr, "ERROR: --bottom option requires at least 2 arguments\n");
                return 1;
            }
            if (!strcmp(argv[1], "bluetooth")) {
                g_bluetooth_id = strdup(argv[2]);
                argv += 2;
            } else
#endif
            {
                if (!argv[1] || !argv[2] || !argv[3]) {
                    fprintf(stderr, "ERROR: --bottom option for UDP requires 3 arguments\n");
                    return 1;
                }
                bottom_send_host_ip  = *++argv;
                bottom_send_port     = (uint16_t)strtoul(*++argv, NULL, 10);
                bottom_receive_port  = (uint16_t)strtoul(*++argv, NULL, 10);
                bottom_ports_set     = true;
            }
        } else if (!strcmp("--command-port", arg)) {
            const char* command_port_str = *++argv;
            if (!command_port_str) {
                fprintf(stderr, "ERROR: --command-port option requires an argument\n");
                return 1;
            }
            command_port = (uint16_t)strtoul(command_port_str, NULL, 10);
        } else if (!strcmp("--event-port", arg)) {
            const char* event_port_str = *++argv;
            if (!event_port_str) {
                fprintf(stderr, "ERROR: --event-port option requires an argument\n");
                return 1;
            }
            event_port = (uint16_t)strtoul(event_port_str, NULL, 10);
        } else if (!strcmp("--dtls-key", arg)) {
            const char* dtls_key = *++argv;
            if (!dtls_key) {
                fprintf(stderr, "ERROR: --dtls-key option requires an argument\n");
                return 1;
            }
            const char* separator = strchr(dtls_key, ':');
            if (!separator) {
                fprintf(stderr, "ERROR: invalid --dtls-key argument\n");
                return 1;
            }
            const char* key_hex = separator + 1;
            if (strlen(key_hex) != 32) {
                fprintf(stderr, "ERROR: --dtls-key key argument isn't 32 characters\n");
                return 1;
            }
            if (separator - dtls_key > GG_STACK_TOOL_MAX_DTLS_IDENTITY_SIZE) {
                fprintf(stderr, "ERROR: --dtls-key identity too large\n");
                return 1;
            }

            // allocate a new Psk
            Psk* psk = (Psk*)GG_AllocateZeroMemory(sizeof(Psk));
            psk->identity_size = separator - dtls_key;
            memcpy(psk->identity, (const uint8_t*)dtls_key, psk->identity_size);
            GG_HexToBytes(key_hex, 32, psk->key);

            // put the new psk in the list
            psk->next = psks;
            psks = psk;
        } else if (!strcmp("--gattlink", arg)) {
            if (!argv[1] || !argv[2] || !argv[3]) {
                fprintf(stderr, "ERROR: --gattlink option requires 3 arguments\n");
                return 1;
            }
            gattlink_max_fragment_size = (uint16_t)strtoul(*++argv, NULL, 10);
            gattlink_rx_window         = (uint8_t)strtoul(*++argv, NULL, 10);
            gattlink_tx_window         = (uint8_t)strtoul(*++argv, NULL, 10);
        } else if (!strcmp("--trace", arg)) {
            trace = true;
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
        } else if (!strcmp("--force-start", arg)) {
            force_start = true;
#endif
        } else if (!strcmp("--enable-header-compression", arg)) {
            enable_header_compression = true;
        } else if (!role_parsed) {
            if (!strcmp(arg, "hub")) {
                role = GG_STACK_ROLE_HUB;
            } else if (!strcmp(arg, "node")) {
                role = GG_STACK_ROLE_NODE;
            } else {
                fprintf(stderr, "ERROR: invalid role\n");
                return 1;
            }
            role_parsed = true;
        } else if (!stack_descriptor_or_command) {
            stack_descriptor_or_command = arg;
            if (stack_descriptor_or_command[0] == '@') {
                command_mode = true;
            }
        } else {
            fprintf(stderr, "ERROR: unexpected argument\n");
            return 1;
        }
    }

    // check that all arguments were supplied
    if (!role_parsed || !stack_descriptor_or_command) {
        fprintf(stderr, "ERROR: missing arguments\n");
        return 1;
    }

    // update defaults where needed
    if (!top_ports_set) {
        top_send_port = role == GG_STACK_ROLE_HUB ?
                        GG_STACK_TOOL_DEFAULT_HUB_TOP_SEND_PORT :
                        GG_STACK_TOOL_DEFAULT_NODE_TOP_SEND_PORT;
        top_receive_port = role == GG_STACK_ROLE_HUB ?
                           GG_STACK_TOOL_DEFAULT_HUB_TOP_RECEIVE_PORT :
                           GG_STACK_TOOL_DEFAULT_NODE_TOP_RECEIVE_PORT;
    }
    if (!bottom_ports_set) {
        bottom_send_port = role == GG_STACK_ROLE_HUB ?
                           GG_STACK_TOOL_DEFAULT_HUB_BOTTOM_SEND_PORT :
                           GG_STACK_TOOL_DEFAULT_NODE_BOTTOM_SEND_PORT;
        bottom_receive_port = role == GG_STACK_ROLE_HUB ?
                              GG_STACK_TOOL_DEFAULT_HUB_BOTTOM_RECEIVE_PORT :
                              GG_STACK_TOOL_DEFAULT_NODE_BOTTOM_RECEIVE_PORT;
    }
    if (command_port == 0) {
        command_port = role == GG_STACK_ROLE_HUB ?
                       GG_STACK_TOOL_DEFAULT_HUB_COMMAND_PORT :
                       GG_STACK_TOOL_DEFAULT_NODE_COMMAND_PORT;
    }
    if (event_port == 0) {
        event_port = role == GG_STACK_ROLE_HUB ?
                       GG_STACK_TOOL_DEFAULT_HUB_EVENT_PORT :
                       GG_STACK_TOOL_DEFAULT_NODE_EVENT_PORT;
    }
    if (psks == NULL) {
        psks = &DefaultPsk;
    }

    // init Golden Gate
    GG_Module_Initialize();

    // let's announce ourselves
    printf("=== Golden Gate Stack Tool ===\n");

    // setup the loop
    GG_Loop* loop = NULL;
    result = GG_Loop_Create(&loop);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_Loop_Create failed (%d)\n", result);
        return 1;
    }
    GG_Loop_BindToCurrentThread(loop);

    // create a BSD socket to send/receive commands
    GG_DatagramSocket* command_socket;
    result = CreateSocket(loop,
                          "command",
                          false,
                          "127.0.0.1",
                          command_mode ? command_port : 0,
                          0,
                          command_mode ? 0 : command_port,
                          trace,
                          &command_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: failed to create command socket on port %u (%d)\n", command_port, result);
        return 1;
    }

    // create a BSD socket to send events
    GG_DatagramSocket* event_socket;
    result = CreateSocket(loop,
                          "event",
                          false,
                          "127.0.0.1",
                          event_port,
                          0,
                          0,
                          trace,
                          &event_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: failed to create event socket on port %u (%d)\n", event_port, result);
        return 1;
    }

    // setup the command listener
    CommandListener command_listener;
    command_listener.stack = NULL;
    GG_SET_INTERFACE(&command_listener, CommandListener, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(command_socket), GG_CAST(&command_listener, GG_DataSink));

    if (command_mode) {
        // send a command or run the stack
        SendCommand(command_socket, stack_descriptor_or_command);
    } else {
        // create the bottom source and sink
        GG_DataSource*     bottom_source = NULL;
        GG_DataSink*       bottom_sink   = NULL;
        GG_DatagramSocket* bottom_socket = NULL;
    #if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
        // create a Bluetooth source/sink if specified
        if (g_bluetooth_id) {
            result = GG_StackToolBluetoothTransport_Create(loop,
                                                           g_bluetooth_id,
                                                           &g_bluetooth_transport);
            if (GG_FAILED(result)) {
                fprintf(stderr, "ERROR: GG_StackToolBluetoothTransport_Create failed (%d)\n", result);
                return 1;
            }
            bottom_source = GG_StackToolBluetoothTransport_AsDataSource(g_bluetooth_transport);
            bottom_sink   = GG_StackToolBluetoothTransport_AsDataSink(g_bluetooth_transport);

            // check if we need to wait for a link up event
            wait_for_link_up = !force_start;
        } else
    #endif
        {
            // create a BSD socket to connect to the bottom
            result = CreateSocket(loop,
                                  "bottom",
                                  false,
                                  bottom_send_host_ip,
                                  bottom_send_port,
                                  0,
                                  bottom_receive_port,
                                  trace,
                                  &bottom_socket);
            if (GG_FAILED(result)) {
                fprintf(stderr, "ERROR: failed to create bottom socket (%d)\n", result);
                return 1;
            }
            bottom_source = GG_DatagramSocket_AsDataSource(bottom_socket);
            bottom_sink   = GG_DatagramSocket_AsDataSink(bottom_socket);
        }

        // prepare construction parameters
        GG_StackBuilderParameters parameters[4];
        size_t parameter_count = 0;

        // initialize a key resolver if needed
        if (strchr(stack_descriptor_or_command, 'D')) {
            static uint16_t cipher_suites[3] = {
                GG_TLS_PSK_WITH_AES_128_CCM,
                GG_TLS_PSK_WITH_AES_128_GCM_SHA256,
                GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
            };

            if (role == GG_STACK_ROLE_HUB) {
                // setup a DTLS key resolver
                GG_SET_INTERFACE(&PskResolver, StaticPskResolver, GG_TlsKeyResolver);
                PskResolver.psks = psks;

                static GG_TlsServerOptions dtls_server_parameters = {
                    .base = {
                        .cipher_suites       = cipher_suites,
                        .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
                    },
                    .key_resolver = GG_CAST(&PskResolver, GG_TlsKeyResolver)
                };
                parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_SERVER;
                parameters[parameter_count].element_parameters = &dtls_server_parameters;
                ++parameter_count;
            } else {
                static GG_TlsClientOptions tls_options = {
                    .base = {
                        .cipher_suites       = cipher_suites,
                        .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
                    }
                };
                tls_options.psk_identity      = psks->identity;
                tls_options.psk_identity_size = psks->identity_size;
                tls_options.psk               = psks->key;
                tls_options.psk_size          = GG_STACK_TOOL_KEY_SIZE;
                parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_CLIENT;
                parameters[parameter_count].element_parameters = &tls_options;
                ++parameter_count;
            }
        }

        // setup gattlink if needed
        if (gattlink_max_fragment_size) {
            static GG_StackElementGattlinkParameters gattlink_options;
            gattlink_options = (GG_StackElementGattlinkParameters) {
                .rx_window                 = gattlink_rx_window,
                .tx_window                 = gattlink_tx_window,
                .buffer_size               = 0,
                .initial_max_fragment_size = gattlink_max_fragment_size,
                .probe_config              = NULL,
            };
            parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_GATTLINK;
            parameters[parameter_count].element_parameters = &gattlink_options;
            ++parameter_count;
        }

        // setup an IP config if we need to enable IP header compression
        GG_StackIpConfiguration ip_config = { 0 };
        if (enable_header_compression) {
            ip_config.header_compression.enabled = true;
        }

        // build the stack with all defaults
        GG_Stack* stack = NULL;
        result = GG_StackBuilder_BuildStack(stack_descriptor_or_command,
                                            parameters,
                                            parameter_count,
                                            role,
                                            enable_header_compression ? &ip_config : NULL,
                                            loop,
                                            bottom_source,
                                            bottom_sink,
                                            &stack);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_StackBuilder_BuildStack failed (%d)\n", result);
            return 1;
        }

        // register the stack with the command listener
        command_listener.stack = stack;

        // show the top of the stack
        printf("    <%5d>    <%5d>\n", top_send_port, top_receive_port);
        printf("       ^          |\n");
        printf("       |          v\n");
        printf("========= top ============\n");

        // show the stack info
        ShowStack(stack);

        // show the bottom of the stack
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
        if (g_bluetooth_id) {
            printf("========= bottom ============\n");
            printf("       ^          |\n");
            printf("       |          v\n");
            printf("    <GATT>     <GATT>\n");
        } else
#endif
        {
            printf("========= bottom ============\n");
            printf("       ^          |\n");
            printf("       |          v\n");
            printf("    <%5d>    <%5d>\n", bottom_receive_port, bottom_send_port);
        }

        // show the IP config
        ShowIpConfig(stack);

        // listen for events from the stack
        StackListener listener;
        listener.stack = stack;
        listener.start_on_link_up = wait_for_link_up;
        listener.stack_started = false;
        listener.remote_event_sink = GG_DatagramSocket_AsDataSink(event_socket);
        GG_SET_INTERFACE(&listener, StackListener, GG_EventListener);
        GG_EventEmitter_SetListener(GG_Stack_AsEventEmitter(stack), GG_CAST(&listener, GG_EventListener));

#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
        if (g_bluetooth_id) {
            // the stack is interested in MTU change events from the transport
            GG_StackToolBluetoothTransport_SetMtuListener(g_bluetooth_transport,
                                                          GG_Stack_AsEventListener(stack));

            // the tool is interested in scan events from the transport
            GG_StackToolBluetoothTransport_SetScanListener(g_bluetooth_transport,
                                                           GG_CAST(&listener, GG_EventListener));

            // the tool is interested in link up events from the transport
            GG_StackToolBluetoothTransport_SetConnectionListener(g_bluetooth_transport,
                                                                 GG_CAST(&listener, GG_EventListener));

            // start the bluetooth transport
            GG_StackToolBluetoothTransport_Start(g_bluetooth_transport);
        }
#endif

        // setup and connect the top of the stack
#if defined(GG_CONFIG_ENABLE_MACOS_TUNNEL)
        GG_StackToolMacosTunnel* tunnel = NULL;
#endif
        GG_DatagramSocket* top_socket = NULL;
        GG_StackElementPortInfo top_port;
        result = GG_Stack_GetPortById(stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &top_port);
        if (GG_SUCCEEDED(result) && top_port.source && top_port.sink) {
            switch (top_type) {
                case GG_STACK_TOOL_TOP_UDP: {
                    result = CreateSocket(loop,
                                          "top",
                                          top_coap_mode || top_coap_request_send_port,
                                          top_send_host_ip,
                                          top_send_port,
                                          top_coap_request_send_port,
                                          top_receive_port,
                                          trace,
                                          &top_socket);
                    if (GG_FAILED(result)) {
                        fprintf(stderr, "ERROR: failed to create top socket (%d)\n", result);
                        return 1;
                    }

                    GG_DataSource_SetDataSink(top_port.source, GG_DatagramSocket_AsDataSink(top_socket));
                    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(top_socket), top_port.sink);
                    break;
                }

                case GG_STACK_TOOL_TOP_BLAST:
                    SetupBlaster(top_port.source, top_port.sink, blast_packet_count, blast_packet_size);
                    break;

#if defined(GG_CONFIG_ENABLE_MACOS_TUNNEL)
                case GG_STACK_TOOL_TOP_TUNNEL:
                    result = GG_StackToolMacosTunnel_Create(loop, trace, &tunnel);
                    if (GG_FAILED(result)) {
                        fprintf(stderr, "ERROR: failed to create tunnel (%d)\n", result);
                        return 1;
                    }
                    GG_DataSource_SetDataSink(top_port.source, GG_StackToolMacosTunnel_AsDataSink(tunnel));
                    GG_DataSource_SetDataSink(GG_StackToolMacosTunnel_AsDataSource(tunnel), top_port.sink);
                    break;
#endif
            }
        } else {
            printf(">>> stack has no connectable top port\n");
        }

        // start the stack
        if (!wait_for_link_up) {
            GG_Stack_Start(stack);
        }

        // run the loop
        GG_Loop_Run(loop);

        // cleanup
        GG_Stack_Destroy(stack);
        GG_DatagramSocket_Destroy(top_socket);
        GG_DatagramSocket_Destroy(bottom_socket);
#if defined(GG_CONFIG_ENABLE_CORE_BLUETOOTH_TRANSPORT)
        GG_StackToolBluetoothTransport_Destroy(g_bluetooth_transport);
#endif
#if defined(GG_CONFIG_ENABLE_MACOS_TUNNEL)
        GG_StackToolMacosTunnel_Destroy(tunnel);
#endif
    }

    // cleanup
    GG_DatagramSocket_Destroy(command_socket);
    GG_DatagramSocket_Destroy(event_socket);
    if (psks != &DefaultPsk) {
        while (psks) {
            Psk* next = psks->next;
            GG_FreeMemory(psks);
            psks = next;
        }
    }
    CleanupBlaster();

    // done
    GG_Loop_Destroy(loop);
    GG_Module_Terminate();
    return 0;
}
