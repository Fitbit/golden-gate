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
 * @date 2018-03-15
 *
 * @details
 *
 * Stack builder implementation.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "gg_stack_builder.h"
#include "gg_stack_builder_base.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_threads.h"
#include "xp/loop/gg_loop.h"
#include "xp/protocols/gg_ipv4_protocol.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/tls/gg_tls.h"
#include "xp/utils/gg_activity_data_monitor.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.stack-builder")

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/

// Number of stacks that have been created. It is safe to make this
// a global that's not protected by a mutex, because the stack builder
// can only be used in a single thread (because of stack elements like
// LWIP that are configured without locking, and are thus single-threaded)
static uint8_t GG_StackBuilder_StackInstanceCount;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/

// Limit how many stack instances can be created. This should never be
// a problem in practice, because there should be just one stack per
// peer at the most. Keeping this number low allows us to use a simple
// IPv4 address allocation scheme (only vary the last byte), but that's
// not a hard limit, it could easily be increased if needed.
#define GG_STACK_BUILDER_MAX_STACK_INSTANCES 64

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static const void*
GG_StackBuilder_FindParameters(GG_StackElementType              element_type,
                               const GG_StackBuilderParameters* parameters,
                               size_t                           parameter_count)
{
    // look for an entry with a matching element type
    for (size_t i = 0; i < parameter_count; i++) {
        if (parameters[i].element_type == element_type) {
            return parameters[i].element_parameters;
        }
    }

    // not found
    return NULL;
}

//----------------------------------------------------------------------
static GG_StackElement*
GG_Stack_FindElementById(GG_Stack* self, GG_StackElementId element_id)
{
    if (!self->element_count) {
        // no elements
        return NULL;
    }

    // check virtual element IDs
    if (element_id == GG_STACK_ELEMENT_ID_BOTTOM) {
        return self->elements[self->element_count-1];
    } else if (element_id == GG_STACK_ELEMENT_ID_TOP) {
        return self->elements[0];
    } else {
        // look for an entry with a matching element type
        for (size_t i = 0; i < self->element_count; i++) {
            if (self->elements[i]->id == element_id) {
                return self->elements[i];
            }
        }
    }

    // not found
    return NULL;
}

//----------------------------------------------------------------------
static void
GG_StackActivityMonitorElement_Destroy(GG_StackActivityMonitorElement* self)
{
    if (self == NULL) return;

    GG_ActivityDataMonitor_Destroy(self->bottom_to_top_monitor);
    GG_ActivityDataMonitor_Destroy(self->top_to_bottom_monitor);

    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
static GG_Result
GG_StackActivityMonitorElement_Create(const GG_StackElementActivityMonitorParameters* parameters,
                                       GG_Stack*                                      stack,
                                       GG_StackActivityMonitorElement**               element)
{
    GG_Result result;

    // allocate the element
    GG_StackActivityMonitorElement* self =
        GG_AllocateZeroMemory(sizeof(GG_StackActivityMonitorElement));
    *element = self;
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the base
    self->base.stack = stack;
    self->base.type  = GG_STACK_ELEMENT_TYPE_ACTIVITY_MONITOR;

    GG_TimerScheduler* scheduler = GG_Loop_GetTimerScheduler(stack->loop);

    // extract parameters
    uint32_t inactivity_timeout = parameters
        ? parameters->inactivity_timeout
        : GG_STACK_ELEMENT_ACTIVITY_MONITOR_DEFAULT_TIMEOUT;

    // create a bottom to top activity data monitor (
    GG_LOG_FINE("creating bottom to top monitor");
    result = GG_ActivityDataMonitor_Create(scheduler,
                                           GG_ACTIVITY_MONITOR_DIRECTION_BOTTOM_TO_TOP,
                                           inactivity_timeout,
                                           &self->bottom_to_top_monitor);
    if (GG_FAILED(result)) {
        goto end;
    }

    // create a top to bottom activity data monitor
    GG_LOG_FINE("creating top to bottom monitor");
    result = GG_ActivityDataMonitor_Create(scheduler,
                                           GG_ACTIVITY_MONITOR_DIRECTION_TOP_TO_BOTTOM,
                                           inactivity_timeout,
                                           &self->top_to_bottom_monitor);
    if (GG_FAILED(result)) {
        goto end;
    }

    // register the stack as a listener for the transport monitor objects
    GG_EventEmitter_SetListener(GG_ActivityDataMonitor_AsEventEmitter(self->bottom_to_top_monitor),
                                GG_CAST(stack, GG_EventListener));
    GG_EventEmitter_SetListener(GG_ActivityDataMonitor_AsEventEmitter(self->top_to_bottom_monitor),
                                GG_CAST(stack, GG_EventListener));

    // setup the ports
    self->base.bottom_port.source = GG_ActivityDataMonitor_AsDataSource(self->top_to_bottom_monitor);
    self->base.bottom_port.sink   = GG_ActivityDataMonitor_AsDataSink(self->bottom_to_top_monitor);
    self->base.top_port.source    = GG_ActivityDataMonitor_AsDataSource(self->bottom_to_top_monitor);
    self->base.top_port.sink      = GG_ActivityDataMonitor_AsDataSink(self->top_to_bottom_monitor);

end:
    if (GG_FAILED(result)) {
        GG_StackActivityMonitorElement_Destroy(self);
        *element = NULL;
    }

    return result;
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
static void
GG_StackActivityMonitorElement_Inspect(GG_StackActivityMonitorElement* self,
                                       GG_Inspector*                   inspector)
{
    GG_Inspector_OnInspectable(inspector,
                               "bottom_to_top_monitor",
                               GG_ActivityDataMonitor_AsInspectable(self->bottom_to_top_monitor));
    GG_Inspector_OnInspectable(inspector,
                               "top_to_bottom_monitor",
                               GG_ActivityDataMonitor_AsInspectable(self->top_to_bottom_monitor));
}
#endif

//----------------------------------------------------------------------
static void
GG_StackGattlinkElement_Destroy(GG_StackGattlinkElement* self)
{
    if (self == NULL) return;

    GG_GattlinkGenericClient_Destroy(self->client);
    GG_Ipv4FrameAssembler_Destroy(self->frame_assembler);
    GG_Ipv4FrameSerializer_Destroy(self->frame_serializer);

    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
static GG_Result
GG_StackGattlinkElement_Start(GG_StackGattlinkElement* self)
{
    GG_LOG_FINER("starting gattlink session");
    return GG_GattlinkGenericClient_Start(self->client);
}

//----------------------------------------------------------------------
static GG_Result
GG_StackGattlinkElement_Reset(GG_StackGattlinkElement* self)
{
    GG_LOG_FINER("resetting gattlink session");
    return GG_GattlinkGenericClient_Reset(self->client);
}

//----------------------------------------------------------------------
static void
GG_StackGattlinkElement_OnLinkMtuChange(GG_StackGattlinkElement*          self,
                                        const GG_StackLinkMtuChangeEvent* event)
{
    GG_GattlinkGenericClient_SetMaxTransportFragmentSize(self->client, (size_t)event->link_mtu);
}

//----------------------------------------------------------------------
static GG_Result
GG_StackGattlinkElement_Create(const GG_StackElementGattlinkParameters* parameters,
                               GG_Stack*                                stack,
                               GG_StackGattlinkElement**                element)
{
    GG_Result result;

    // allocate the element
    GG_StackGattlinkElement* self = GG_AllocateZeroMemory(sizeof(GG_StackGattlinkElement));
    *element = self;
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the base
    self->base.stack = stack;
    self->base.type  = GG_STACK_ELEMENT_TYPE_GATTLINK;

    // setup the common parts of the serializer and assembler configs
    GG_Ipv4FrameSerializationIpConfig serialization_ip_config = { 0 };
    serialization_ip_config.udp_src_ports[0] = stack->ip_configuration.header_compression.default_udp_port;
    serialization_ip_config.udp_dst_ports[0] = stack->ip_configuration.header_compression.default_udp_port;

    // create a frame serializer
    GG_LOG_FINE("creating ipv4 frame serializer");
    if (stack->ip_configuration.header_compression.enabled) {
        // in the outgoing direction, use our local and remote IP addresses as src and dst for compression
        serialization_ip_config.default_src_address = GG_IpAddress_AsInteger(&stack->ip_configuration.local_address);
        serialization_ip_config.default_dst_address = GG_IpAddress_AsInteger(&stack->ip_configuration.remote_address);
    }
    result = GG_Ipv4FrameSerializer_Create(stack->ip_configuration.header_compression.enabled ?
                                           &serialization_ip_config : NULL,
                                           &self->frame_serializer);
    if (GG_FAILED(result)) {
        goto end;
    }

    // create a frame assembler (in the incoming direction, use the reverse from what we use in the outgoing direction)
    // NOTE we always pass a compression config to the assembler, so that even if compression isn't enabled
    // in the outgoing direction, it is able to decompress packets in the incoming direction.
    GG_LOG_FINE("creating ipv4 frame assembler - ip_mtu=%d", (int)stack->ip_configuration.ip_mtu);
    GG_Ipv4FrameAssemblerIpMap assembler_ip_map = { 0 };
    if (stack->ip_configuration.inbound_address_remapping.enabled) {
        assembler_ip_map.src_address =
            GG_IpAddress_AsInteger(&stack->ip_configuration.inbound_address_remapping.source_address);
        assembler_ip_map.remapped_src_address = serialization_ip_config.default_dst_address;
        assembler_ip_map.dst_address =
            GG_IpAddress_AsInteger(&stack->ip_configuration.inbound_address_remapping.destination_address);
        assembler_ip_map.remapped_dst_address = serialization_ip_config.default_src_address;
    }
    serialization_ip_config.default_src_address = GG_IpAddress_AsInteger(&stack->ip_configuration.remote_address);
    serialization_ip_config.default_dst_address = GG_IpAddress_AsInteger(&stack->ip_configuration.local_address);
    result = GG_Ipv4FrameAssembler_Create(stack->ip_configuration.ip_mtu,
                                          &serialization_ip_config,
                                          stack->ip_configuration.inbound_address_remapping.enabled ?
                                          &assembler_ip_map : NULL,
                                          &self->frame_assembler);
    if (GG_FAILED(result)) {
        goto end;
    }

    // create a gattlink client
    size_t gattlink_buffer_size;
    if (parameters && parameters->buffer_size) {
        gattlink_buffer_size = parameters->buffer_size;
    } else {
        //gattlink_buffer_size = 2 * stack->ip_configuration.ip_mtu; // default to 2 max IP packets
        gattlink_buffer_size = 16384; // FIXME: testing
    }
    GG_LOG_FINE("creating gattlink client - buffer_size=%d, tx_window=%d, rx_window=%d, initial_max_fragment_size=%d",
                (int)gattlink_buffer_size,
                parameters ? parameters->tx_window : 0,
                parameters ? parameters->rx_window : 0,
                parameters ? parameters->initial_max_fragment_size :
                             GG_STACK_BUILDER_DEFAULT_GATTLINK_FRAGMENT_SIZE);

    if (parameters && parameters->probe_config) {
        GG_LOG_FINE("Gattlink Data Probe enabled with window span: %d, buffer_sample_count: %d,"
                    " buffer_threshold: %d",
                    (int)parameters->probe_config->window_size_ms,
                    (int)parameters->probe_config->buffer_sample_count,
                    (int)parameters->probe_config->buffer_threshold);
    } else {
        GG_LOG_FINE("Gattlink Data Probe disabled.");
    }
    result = GG_GattlinkGenericClient_Create(GG_Loop_GetTimerScheduler(stack->loop),
                                             gattlink_buffer_size,
                                             parameters ? parameters->tx_window : 0,
                                             parameters ? parameters->rx_window : 0,
                                             parameters ? parameters->initial_max_fragment_size :
                                                          GG_STACK_BUILDER_DEFAULT_GATTLINK_FRAGMENT_SIZE,
                                             parameters ? parameters->probe_config : NULL,
                                             GG_Ipv4FrameSerializer_AsFrameSerializer(self->frame_serializer),
                                             GG_Ipv4FrameAssembler_AsFrameAssembler(self->frame_assembler),
                                             &self->client);
    if (GG_FAILED(result)) {
        goto end;
    }

    // register the stack as a listener for the gattlink object
    GG_EventEmitter_SetListener(GG_GattlinkGenericClient_AsEventEmitter(self->client),
                                GG_CAST(stack, GG_EventListener));

    // setup the ports
    self->base.bottom_port.source = GG_GattlinkGenericClient_GetTransportSideAsDataSource(self->client);
    self->base.bottom_port.sink   = GG_GattlinkGenericClient_GetTransportSideAsDataSink(self->client);
    self->base.top_port.source    = GG_GattlinkGenericClient_GetUserSideAsDataSource(self->client);
    self->base.top_port.sink      = GG_GattlinkGenericClient_GetUserSideAsDataSink(self->client);

end:
    if (GG_FAILED(result)) {
        GG_StackGattlinkElement_Destroy(self);
        *element = NULL;
    }

    return result;
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
static void
GG_StackGattlinkElement_Inspect(GG_StackGattlinkElement* self, GG_Inspector* inspector)
{
    GG_Inspector_OnInspectable(inspector,
                               "frame_assembler",
                               GG_Ipv4FrameAssembler_AsInspectable(self->frame_assembler));
    GG_Inspector_OnInspectable(inspector,
                               "client",
                               GG_GattlinkGenericClient_AsInspectable(self->client));
}
#endif

//----------------------------------------------------------------------
static void
GG_StackDatagramSocketElement_Destroy(GG_StackDatagramSocketElement* self)
{
    if (self == NULL) return;

    GG_DatagramSocket_Destroy(self->socket);

    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
static GG_Result
GG_StackDatagramSocketElement_Create(const GG_StackElementDatagramSocketParameters* parameters,
                                     GG_Stack*                                      stack,
                                     GG_StackDatagramSocketElement**                element)
{
    // allocate the element
    GG_StackDatagramSocketElement* self = GG_AllocateZeroMemory(sizeof(GG_StackDatagramSocketElement));
    *element = self;
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the base
    self->base.stack = stack;
    self->base.type  = GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET;

    // setup the addresses
    GG_SocketAddress local_address  = GG_SOCKET_ADDRESS_NULL_INITIALIZER;
    GG_SocketAddress remote_address = GG_SOCKET_ADDRESS_NULL_INITIALIZER;
    local_address.address  = stack->ip_configuration.local_address;
    remote_address.address = stack->ip_configuration.remote_address;
    if (parameters && parameters->local_port) {
        local_address.port = parameters->local_port;
    } else {
        if (stack->secure) {
            local_address.port = GG_STACK_BUILDER_DEFAULT_DTLS_SOCKET_PORT;
        } else {
            local_address.port = GG_STACK_BUILDER_DEFAULT_UDP_SOCKET_PORT;
        }
    }
    if (parameters && parameters->remote_port) {
        remote_address.port = parameters->remote_port;
    } else {
        if (stack->secure) {
            remote_address.port = GG_STACK_BUILDER_DEFAULT_DTLS_SOCKET_PORT;
        } else {
            remote_address.port = GG_STACK_BUILDER_DEFAULT_UDP_SOCKET_PORT;
        }
    }
    GG_LOG_FINE("datagram socket element: local_port=%d, remote_port=%d",
                (int)local_address.port,
                (int)remote_address.port);

    // instantiate the socket
    GG_Result result = GG_DatagramSocket_Create(&local_address,
                                                &remote_address,
                                                false,
                                                (uint16_t)stack->max_datagram_size,
                                                &self->socket);
    if (GG_FAILED(result)) {
        goto end;
    }

    // attach the socket
    GG_DatagramSocket_Attach(self->socket, stack->loop);

    // setup the ports
    self->base.top_port.source = GG_DatagramSocket_AsDataSource(self->socket);
    self->base.top_port.sink   = GG_DatagramSocket_AsDataSink(self->socket);

end:
    if (GG_FAILED(result)) {
        GG_StackDatagramSocketElement_Destroy(self);
        *element = NULL;
    }

    return result;
}

//----------------------------------------------------------------------
static void
GG_StackDtlsElement_Destroy(GG_StackDtlsElement* self)
{
    if (self == NULL) return;

    GG_DtlsProtocol_Destroy(self->dtls);

    GG_ClearAndFreeObject(self, 0);
}

//----------------------------------------------------------------------
static GG_Result
GG_StackDtlsElement_Create(const void*           client_parameters,
                           const void*           server_parameters,
                           GG_Stack*             stack,
                           GG_StackDtlsElement** element)
{
    // allocate the element
    GG_StackDtlsElement* self = GG_AllocateZeroMemory(sizeof(GG_StackDtlsElement));
    *element = self;
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the base
    self->base.stack = stack;
    self->base.type  = client_parameters ? GG_STACK_ELEMENT_TYPE_DTLS_CLIENT : GG_STACK_ELEMENT_TYPE_DTLS_SERVER;

    // instantiate the DTLS protocol object
    GG_Result result;
    if (client_parameters) {
        const GG_TlsClientOptions* tls_options = (const GG_TlsClientOptions*)client_parameters;
        result = GG_DtlsProtocol_Create(GG_TLS_ROLE_CLIENT,
                                        &tls_options->base,
                                        stack->max_datagram_size,
                                        GG_Loop_GetTimerScheduler(stack->loop),
                                        &self->dtls);
    } else if (server_parameters) {
        const GG_TlsServerOptions* tls_options = (const GG_TlsServerOptions*)server_parameters;
        result = GG_DtlsProtocol_Create(GG_TLS_ROLE_SERVER,
                                        &tls_options->base,
                                        stack->max_datagram_size,
                                        GG_Loop_GetTimerScheduler(stack->loop),
                                        &self->dtls);
    } else {
        result = GG_ERROR_INVALID_PARAMETERS;
        goto end;
    }
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_DtlsProtocol_Create failed (%d)", result);
        goto end;
    }

    // register the stack as a listener for the gattlink object
    GG_EventEmitter_SetListener(GG_DtlsProtocol_AsEventEmitter(self->dtls),
                                GG_CAST(stack, GG_EventListener));

    // setup the ports
    self->base.top_port.source    = GG_DtlsProtocol_GetUserSideAsDataSource(self->dtls);
    self->base.top_port.sink      = GG_DtlsProtocol_GetUserSideAsDataSink(self->dtls);
    self->base.bottom_port.source = GG_DtlsProtocol_GetTransportSideAsDataSource(self->dtls);
    self->base.bottom_port.sink   = GG_DtlsProtocol_GetTransportSideAsDataSink(self->dtls);

end:
    if (GG_FAILED(result)) {
        GG_StackDtlsElement_Destroy(self);
        *element = NULL;
    }

    return result;
}

//----------------------------------------------------------------------
static void
GG_StackDtlsElement_Start(GG_StackDtlsElement* self)
{
    GG_DtlsProtocol_StartHandshake(self->dtls);
}

//----------------------------------------------------------------------
static void
GG_StackDtlsElement_GetStatus(GG_StackDtlsElement* self, GG_DtlsProtocolStatus* status)
{
    GG_DtlsProtocol_GetStatus(self->dtls, status);
}

//----------------------------------------------------------------------
static GG_Result
GG_StackDtlsElement_Reset(GG_StackDtlsElement* self)
{
    return GG_DtlsProtocol_Reset(self->dtls);
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
static void
GG_StackDtlsElement_Inspect(GG_StackDtlsElement* self, GG_Inspector* inspector)
{
    GG_Inspector_OnString(inspector,      "role", self->role == GG_TLS_ROLE_CLIENT ? "CLIENT" : "SERVER");
    GG_Inspector_OnInspectable(inspector, "dtls", GG_DtlsProtocol_AsInspectable(self->dtls));
}
#endif

//----------------------------------------------------------------------
void
GG_Stack_Destroy(GG_Stack* self)
{
    if (self == NULL) return;

    // disconnect all elements from each other
    for (unsigned int i = 1; i < self->element_count; i++) {
        if (self->elements[i - 1] && self->elements[i - 1]->bottom_port.source) {
            GG_DataSource_SetDataSink(self->elements[i - 1]->bottom_port.source, NULL);
        }
        if (self->elements[i] && self->elements[i]->top_port.source) {
            GG_DataSource_SetDataSink(self->elements[i]->top_port.source, NULL);
        }
    }

    // disconnect the top and bottom elements from the outside
    if (self->element_count && self->elements) {
        if (self->elements[0] && self->elements[0]->top_port.source) {
            GG_DataSource_SetDataSink(self->elements[0]->top_port.source, NULL);
        }
        if (self->elements[self->element_count - 1] && self->elements[self->element_count - 1]->bottom_port.source) {
            GG_DataSource_SetDataSink(self->elements[self->element_count - 1]->bottom_port.source, NULL);
        }
    }

    // destroy all the elements
    GG_StackActivityMonitorElement_Destroy(self->activity_monitor_element);
    GG_StackGattlinkElement_Destroy(self->gattlink_element);
    GG_StackNetworkInterfaceElement_Destroy(self->netif_element);
    GG_StackDatagramSocketElement_Destroy(self->datagram_socket_element);
    GG_StackDtlsElement_Destroy(self->dtls_element);

    // free memory resources
    GG_FreeMemory(self->elements);
    GG_ClearAndFreeObject(self, 1);

    // this stack isn't counted anymore
    GG_ASSERT(GG_StackBuilder_StackInstanceCount);
    --GG_StackBuilder_StackInstanceCount;
}

//----------------------------------------------------------------------
GG_Result
GG_Stack_Start(GG_Stack* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_LOG_FINE("starting stack");

    // start Gattlink
    if (self->gattlink_element) {
        GG_StackGattlinkElement_Start(self->gattlink_element);
    }

    // start DTLS
    if (self->dtls_element) {
        GG_StackDtlsElement_Start(self->dtls_element);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Stack_Reset(GG_Stack* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_LOG_FINE("resetting stack");

    // reset DTLS
    if (self->dtls_element) {
        GG_StackDtlsElement_Reset(self->dtls_element);
    }

    // reset Gattlink
    if (self->gattlink_element) {
        GG_StackGattlinkElement_Reset(self->gattlink_element);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_EventEmitter*
GG_Stack_AsEventEmitter(GG_Stack* self)
{
    return GG_CAST(&self->event_emitter, GG_EventEmitter);
}

//----------------------------------------------------------------------
GG_EventListener*
GG_Stack_AsEventListener(GG_Stack* self)
{
    return GG_CAST(self, GG_EventListener);
}

//----------------------------------------------------------------------
GG_Result
GG_Stack_GetIpConfiguration(GG_Stack* self, GG_StackIpConfiguration* ip_configuration)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    *ip_configuration = self->ip_configuration;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Stack_GetDtlsProtocolStatus(GG_Stack* self, GG_DtlsProtocolStatus* status)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check if we do have a DTLS element
    if (!self->dtls_element) {
        return GG_ERROR_NO_SUCH_ITEM;
    }

    // delegate to the DTLS element
    GG_StackDtlsElement_GetStatus(self->dtls_element, status);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
size_t
GG_Stack_GetElementCount(GG_Stack* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    return self->element_count;
}

//----------------------------------------------------------------------
GG_Result
GG_Stack_GetPortById(GG_Stack* self, uint32_t element_id, uint32_t port_id, GG_StackElementPortInfo* port_info)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // find the element by ID
    GG_StackElement* element = GG_Stack_FindElementById(self, element_id);
    if (element == NULL) {
        return GG_ERROR_NO_SUCH_ITEM;
    }

    // get the requested port
    port_info->id = port_id;
    switch (port_id) {
        case GG_STACK_PORT_ID_TOP:
            port_info->sink   = element->top_port.sink;
            port_info->source = element->top_port.source;
            return GG_SUCCESS;

        case GG_STACK_PORT_ID_BOTTOM:
            port_info->sink   = element->bottom_port.sink;
            port_info->source = element->bottom_port.source;
            return GG_SUCCESS;

        default:
            break;
    }

    // not found
    return GG_ERROR_NO_SUCH_ITEM;
}

//----------------------------------------------------------------------
GG_Result
GG_Stack_GetElementByIndex(GG_Stack* self, unsigned int element_index, GG_StackElementInfo* element_info)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // check the range
    if (element_index >= self->element_count) {
        return GG_ERROR_OUT_OF_RANGE;
    }

    // setup the element info
    element_info->id   = self->elements[element_index]->id;
    element_info->type = self->elements[element_index]->type;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_Stack_OnLinkMtuChangeEvent(GG_Stack* self, const GG_StackLinkMtuChangeEvent* event)
{
    // forward the event to the Gattlink element if we have one
    if (self->gattlink_element) {
        GG_StackGattlinkElement_OnLinkMtuChange(self->gattlink_element, event);
    }
}

//----------------------------------------------------------------------
static void
GG_Stack_OnEvent(GG_EventListener* _self, const GG_Event* event)
{
    GG_Stack* self = GG_SELF(GG_Stack, GG_EventListener);
    GG_THREAD_GUARD_CHECK_BINDING(self);

#if defined(GG_CONFIG_ENABLE_LOGGING)
    uint8_t event_type_str[5] = {0};
    GG_BytesFromInt32Be(event_type_str, event->type);
    GG_LOG_FINE("received event %s", (const char*)event_type_str);
#endif

    bool forward_event = false;
    switch (event->type) {
        case GG_EVENT_TYPE_GATTLINK_SESSION_READY:
            forward_event = true;
            if (self->dtls_element) {
                GG_DtlsProtocolStatus dtls_status;
                GG_StackDtlsElement_GetStatus(self->dtls_element, &dtls_status);
                if (dtls_status.state == GG_TLS_STATE_INIT) {
                    GG_LOG_INFO("restarting DTLS");
                    GG_StackDtlsElement_Start(self->dtls_element);
                }
            }
            break;

        case GG_EVENT_TYPE_GATTLINK_SESSION_RESET:
            forward_event = true;
            if (self->dtls_element) {
                GG_DtlsProtocolStatus dtls_status;
                GG_StackDtlsElement_GetStatus(self->dtls_element, &dtls_status);
                if (dtls_status.state != GG_TLS_STATE_INIT) {
                    GG_LOG_INFO("resetting DTLS");
                    GG_StackDtlsElement_Reset(self->dtls_element);
                }
            }
            break;

        case GG_EVENT_TYPE_GATTLINK_SESSION_STALLED:
        case GG_EVENT_TYPE_TLS_STATE_CHANGE:
            forward_event = true;
            break;

        case GG_EVENT_TYPE_LINK_MTU_CHANGE:
            GG_Stack_OnLinkMtuChangeEvent(self, (const GG_StackLinkMtuChangeEvent*)event);
            break;

        case GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_UNDER_THRESHOLD:
        case GG_GENERIC_GATTLINK_CLIENT_OUTPUT_BUFFER_OVER_THRESHOLD:
        case GG_EVENT_TYPE_ACTIVITY_MONITOR_CHANGE:
            forward_event = true;

        default:
            break;
    }

    // forward the event if needed
    if (forward_event && self->event_emitter.listener) {
        GG_StackForwardEvent stack_forward_event = {
            .base = {
                .type   = GG_EVENT_TYPE_STACK_EVENT_FORWARD,
                .source = self
            },
            .forwarded = event
        };
        GG_EventListener_OnEvent(self->event_emitter.listener, &stack_forward_event.base);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_Stack, GG_EventListener) {
    .OnEvent = GG_Stack_OnEvent
};

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
GG_Inspectable*
GG_Stack_AsInspectable(GG_Stack* self)
{
    return GG_CAST(self, GG_Inspectable);
}

//----------------------------------------------------------------------
static GG_Result
GG_Stack_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);
    GG_Stack* self = GG_SELF(GG_Stack, GG_Inspectable);

    GG_Inspector_OnObjectStart(inspector, "ip_configuration");
    char ip_address_string[16];
    GG_IpAddress_AsString(&self->ip_configuration.local_address, ip_address_string, sizeof(ip_address_string));
    GG_Inspector_OnString(inspector, "local_address", ip_address_string);
    GG_IpAddress_AsString(&self->ip_configuration.remote_address, ip_address_string, sizeof(ip_address_string));
    GG_Inspector_OnString(inspector, "remote_address", ip_address_string);
    GG_Inspector_OnInteger(inspector, "ip_mtu", self->ip_configuration.ip_mtu, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnObjectEnd(inspector);
    GG_Inspector_OnInteger(inspector, "max_datagram_size", self->max_datagram_size, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnString(inspector, "role", self->role == GG_STACK_ROLE_NODE ? "NODE" : "HUB");
    GG_Inspector_OnBoolean(inspector, "secure", self->secure);
    if (self->activity_monitor_element) {
        GG_Inspector_OnObjectStart(inspector, "monitor_element");
        GG_StackActivityMonitorElement_Inspect(self->activity_monitor_element, inspector);
        GG_Inspector_OnObjectEnd(inspector);
    }
    if (self->gattlink_element) {
        GG_Inspector_OnObjectStart(inspector, "gattlink_element");
        GG_StackGattlinkElement_Inspect(self->gattlink_element, inspector);
        GG_Inspector_OnObjectEnd(inspector);
    }
    if (self->netif_element) {
        GG_Inspector_OnObjectStart(inspector, "netif_element");
        GG_StackNetworkInterfaceElement_Inspect(self->netif_element, inspector);
        GG_Inspector_OnObjectEnd(inspector);
    }
    if (self->dtls_element) {
        GG_Inspector_OnObjectStart(inspector, "dtls_element");
        GG_StackDtlsElement_Inspect(self->dtls_element, inspector);
        GG_Inspector_OnObjectEnd(inspector);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_Stack, GG_Inspectable) {
    .Inspect = GG_Stack_Inspect
};

#endif

//----------------------------------------------------------------------
GG_Result
GG_StackBuilder_BuildStack(const char*                      descriptor,
                           const GG_StackBuilderParameters* parameters,
                           size_t                           parameter_count,
                           GG_StackRole                     role,
                           const GG_StackIpConfiguration*   ip_configuration,
                           GG_Loop*                         loop,
                           GG_DataSource*                   transport_source,
                           GG_DataSink*                     transport_sink,
                           GG_Stack**                       stack)
{
    GG_ASSERT(descriptor);
    GG_Result result = GG_SUCCESS;
    GG_DataSource* source = NULL;
    GG_DataSink* sink = NULL;

    GG_THREAD_GUARD_CHECK_MAIN_LOOP();

    // check bounds
    if (GG_StackBuilder_StackInstanceCount > GG_STACK_BUILDER_MAX_STACK_INSTANCES) {
        return GG_ERROR_OUT_OF_RESOURCES;
    }

    // count how many elements we need to create
    size_t element_count = strlen(descriptor);
    if (element_count == 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // validate we don't have duplicate element types
    for (size_t i = 0; i < element_count; i++) {
        if (strchr(descriptor + i + 1, descriptor[i]) != NULL) {
            return GG_ERROR_INVALID_PARAMETERS;
        }
    }

    // allocate a new object
    GG_Stack* self = GG_AllocateZeroMemory(sizeof(GG_Stack));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }
    *stack = self;

    // setup the vtables
    GG_SET_INTERFACE(self, GG_Stack, GG_EventListener);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(self, GG_Stack, GG_Inspectable));

    // check if the stack uses DTLS or not
    self->secure = strchr(descriptor, 'D') != NULL;
    GG_LOG_FINE("stack is secure: %s", self->secure ? "true" : "false");

    // setup basic fields
    self->role = role;
    self->loop = loop;
    GG_EventEmitterBase_Init(&self->event_emitter);
    self->index = GG_StackBuilder_StackInstanceCount;
    if (ip_configuration) {
        // copy the configuration
        self->ip_configuration = *ip_configuration;
    } else {
        if (GG_StackBuilder_StackInstanceCount > 0) {
            // for stacks that are not the first stack, enable header compression so
            // that we can get IP address translation, and setup address remapping
            // so that if the peer doesn't use header compression, we can still rewrite
            // the source and destination addresses of incoming packets.
            self->ip_configuration.header_compression.enabled = true;
            self->ip_configuration.inbound_address_remapping.enabled = true;
            GG_IpAddress_SetFromInteger(&self->ip_configuration.inbound_address_remapping.source_address,
                                        role == GG_STACK_ROLE_NODE ?
                                        GG_STACK_BUILDER_DEFAULT_NODE_REMOTE_IP_ADDRESS_BASE + 2:
                                        GG_STACK_BUILDER_DEFAULT_HUB_REMOTE_IP_ADDRESS_BASE + 3);
            GG_IpAddress_SetFromInteger(&self->ip_configuration.inbound_address_remapping.destination_address,
                                        role == GG_STACK_ROLE_NODE ?
                                        GG_STACK_BUILDER_DEFAULT_NODE_LOCAL_IP_ADDRESS_BASE + 3 :
                                        GG_STACK_BUILDER_DEFAULT_HUB_LOCAL_IP_ADDRESS_BASE + 2);
        }
    }

    // update default values
    if (self->ip_configuration.ip_mtu == 0) {
        self->ip_configuration.ip_mtu = GG_STACK_BUILDER_DEFAULT_IP_MTU;
    }

    // setup ip configuration
    if (ip_configuration == NULL || GG_IpAddress_AsInteger(&ip_configuration->local_address) == 0) {
        if (role == GG_STACK_ROLE_NODE) {
            GG_IpAddress_SetFromInteger(&self->ip_configuration.local_address,
                                        GG_STACK_BUILDER_DEFAULT_NODE_LOCAL_IP_ADDRESS_BASE +
                                        3 + (GG_StackBuilder_StackInstanceCount * 2));
        } else {
            GG_IpAddress_SetFromInteger(&self->ip_configuration.local_address,
                                        GG_STACK_BUILDER_DEFAULT_HUB_LOCAL_IP_ADDRESS_BASE +
                                        2 + (GG_StackBuilder_StackInstanceCount * 2));
        }
    }
    if (ip_configuration == NULL || GG_IpAddress_AsInteger(&ip_configuration->remote_address) == 0) {
        if (role == GG_STACK_ROLE_NODE) {
            GG_IpAddress_SetFromInteger(&self->ip_configuration.remote_address,
                                        GG_STACK_BUILDER_DEFAULT_NODE_REMOTE_IP_ADDRESS_BASE +
                                        2 + (GG_StackBuilder_StackInstanceCount * 2));
        } else {
            GG_IpAddress_SetFromInteger(&self->ip_configuration.remote_address,
                                        GG_STACK_BUILDER_DEFAULT_HUB_REMOTE_IP_ADDRESS_BASE +
                                        3 + (GG_StackBuilder_StackInstanceCount * 2));
        }
    }
    if (ip_configuration == NULL || GG_IpAddress_AsInteger(&ip_configuration->if_netmask) == 0) {
        GG_IpAddress_SetFromInteger(&self->ip_configuration.if_netmask, GG_STACK_BUILDER_DEFAULT_NETIF_NETMASK);
    }
    if (ip_configuration == NULL || ip_configuration->header_compression.default_udp_port == 0) {
        self->ip_configuration.header_compression.default_udp_port = self->secure ?
                                                                     GG_STACK_BUILDER_DEFAULT_DTLS_SOCKET_PORT :
                                                                     GG_STACK_BUILDER_DEFAULT_UDP_SOCKET_PORT;
    }
    GG_LOG_FINE("stack IP MTU = %d", (int)self->ip_configuration.ip_mtu);
    GG_LOG_FINE("stack local address: %d.%d.%d.%d",
                (int)self->ip_configuration.local_address.ipv4[0],
                (int)self->ip_configuration.local_address.ipv4[1],
                (int)self->ip_configuration.local_address.ipv4[2],
                (int)self->ip_configuration.local_address.ipv4[3]);
    GG_LOG_FINE("stack remote address: %d.%d.%d.%d",
                (int)self->ip_configuration.remote_address.ipv4[0],
                (int)self->ip_configuration.remote_address.ipv4[1],
                (int)self->ip_configuration.remote_address.ipv4[2],
                (int)self->ip_configuration.remote_address.ipv4[3]);
    GG_LOG_FINE("stack netif netmask: %d.%d.%d.%d",
                (int)self->ip_configuration.if_netmask.ipv4[0],
                (int)self->ip_configuration.if_netmask.ipv4[1],
                (int)self->ip_configuration.if_netmask.ipv4[2],
                (int)self->ip_configuration.if_netmask.ipv4[3]);
    GG_LOG_FINE("compression enabled: %s", self->ip_configuration.header_compression.enabled ? "yes" : "no");
    GG_LOG_FINE("compression default UDP port: %u", self->ip_configuration.header_compression.default_udp_port);

    // compute the max datagram size we can receive
    if (self->ip_configuration.ip_mtu > (GG_IPV4_MIN_IP_HEADER_SIZE + GG_UDP_HEADER_SIZE)) {
        self->max_datagram_size = self->ip_configuration.ip_mtu - (GG_IPV4_MIN_IP_HEADER_SIZE + GG_UDP_HEADER_SIZE);
    } else {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // allocate an array for the elements
    self->elements = GG_AllocateZeroMemory(element_count * sizeof(GG_StackElement*));
    if (self->elements == NULL) {
        result = GG_ERROR_OUT_OF_MEMORY;
        goto end;
    }
    self->element_count = element_count;

    // build one element at a time, starting from the top
    for (size_t i = 0; i < element_count; i++) {
        GG_StackElement* element = NULL;
        char element_code = descriptor[i];
        switch (element_code) {
            case 'A': // Activity Monitor
                if (self->activity_monitor_element) {
                    GG_LOG_SEVERE("Multiple activity monitor elements not supported");
                    goto end;
                }

                GG_LOG_FINE("creating Activity Monitor element");

                // create the element
                result = GG_StackActivityMonitorElement_Create(
                    (const GG_StackElementActivityMonitorParameters*)
                    GG_StackBuilder_FindParameters(GG_STACK_ELEMENT_TYPE_ACTIVITY_MONITOR,
                                                   parameters,
                                                   parameter_count),
                    self,
                    &self->activity_monitor_element);
                if (GG_FAILED(result)) {
                    goto end;
                }

                element = &self->activity_monitor_element->base;
                break;

            case 'G': // Gattlink
                GG_LOG_FINE("creating Gattlink element");

                // create the element
                result = GG_StackGattlinkElement_Create(
                    (const GG_StackElementGattlinkParameters*)
                    GG_StackBuilder_FindParameters(GG_STACK_ELEMENT_TYPE_GATTLINK,
                                                   parameters,
                                                   parameter_count),
                    self,
                    &self->gattlink_element);
                if (GG_FAILED(result)) {
                    goto end;
                }

                element = &self->gattlink_element->base;
                break;

            case 'N': // Network Interface
                GG_LOG_FINE("creating Network Interface element");

                // create the element
                result = GG_StackNetworkInterfaceElement_Create(
                    self,
                    self->ip_configuration.ip_mtu,
                    &self->netif_element);
                if (GG_FAILED(result)) {
                    goto end;
                }

                // NOTE: since this element is only declared but not defined here, we perform
                // a different type of case, knowing that the element struct must start with
                // a GG_StackElement
                element = (GG_StackElement*)self->netif_element;
                break;

            case 'S': // Datagram Socket
                GG_LOG_FINE("creating Datagram Socket element");

                // create the element
                result = GG_StackDatagramSocketElement_Create(
                    (const GG_StackElementDatagramSocketParameters*)
                    GG_StackBuilder_FindParameters(GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET,
                                                   parameters,
                                                   parameter_count),
                    self,
                    &self->datagram_socket_element);
                if (GG_FAILED(result)) {
                    goto end;
                }

                element = &self->datagram_socket_element->base;
                break;

            case 'D': // DTLS Client or Server
                GG_LOG_FINE("creating DTLS element");

                // figure out if this is a client or server
                const void* client_params = GG_StackBuilder_FindParameters(
                    GG_STACK_ELEMENT_TYPE_DTLS_CLIENT,
                    parameters,
                    parameter_count);
                const void* server_params = GG_StackBuilder_FindParameters(
                    GG_STACK_ELEMENT_TYPE_DTLS_SERVER,
                    parameters,
                    parameter_count);
                if ((client_params == NULL && server_params == NULL) ||
                    (client_params != NULL && server_params != NULL)) {
                    // we must have either client or server params, but not both
                    result = GG_ERROR_INVALID_PARAMETERS;
                    goto end;
                }

                // create the element
                result = GG_StackDtlsElement_Create(client_params, server_params, self, &self->dtls_element);
                if (GG_FAILED(result)) {
                    goto end;
                }

                element = &self->dtls_element->base;
                break;

            default:
                GG_LOG_WARNING("unsupported stack element in descriptor (%c)", element_code);
                result = GG_ERROR_NOT_SUPPORTED;
                goto end;
        }

        // store the element in the stack
        self->elements[i] = element;

        // assign an ID to the element
        element->id = (GG_StackElementId)(GG_STACK_BUILDER_ID_BASE + i);

        // connect this element to the previous one if any
        if (i > 0) {
            if (element->top_port.source && sink) {
                GG_DataSource_SetDataSink(element->top_port.source, sink);
            }
            if (element->top_port.sink && source) {
                GG_DataSource_SetDataSink(source, element->top_port.sink);
            }
        }

        // update the current source and sink to point to the new element
        sink = element->bottom_port.sink;
        source = element->bottom_port.source;
    }

    // Connect the transport sink and source to the bottom port of the bottom stack element
    if (transport_source && sink) {
        GG_DataSource_SetDataSink(transport_source, sink);
    }
    if (transport_sink && source) {
        GG_DataSource_SetDataSink(source, transport_sink);
    }

    // bind the stack to the current thread
    GG_THREAD_GUARD_BIND(self);

end:
    // count this stack
    ++GG_StackBuilder_StackInstanceCount;

    // destroy the object if something failed
    if (GG_FAILED(result)) {
        GG_Stack_Destroy(self);
        *stack = NULL;
    }

    return result;
}
