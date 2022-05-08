/**
 * @file
 * @brief Stack Builder
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-03-13
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/tls/gg_tls.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup StackBuilder Stack Builder
//! Stack Builder
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Stack object.
 *
 * A Stack represents a collection of one or more communication layers/elements
 * connected to each other and managed as a single unit.
 *
 * The elements are logically layered from 'bottom' to 'top', where the 'bottom'
 * layer is typically logically closest to the 'transport' side of the stack and
 * the 'top' layer is logically closest to the 'application' side of the stack.
 *
 * Each element in a stack has one or two I/O ports that can be internally
 * connected to other elements in the same stack and/or exposed to users
 * of the stack so that they may be used directly or connected to other
 * data sources/sinks.
 *
 * Stacks may be connected on their top and bottom sides as applicable, to
 * communication elements like transport sources/sinks, application source/sinks,
 * as well as sources/sinks from other stacks (for example, one may create two
 * stack objets and connect them together to form a larger stack).
 *
 * Stack objects listen for events from the elements they create and hold, and
 * forward some of those events to their listener, if any.
 */
typedef struct GG_Stack GG_Stack;

/**
 * Stack element type identifier.
 * See GG_STACK_ELEMENT_TYPE_XXX constants
 */
typedef uint32_t GG_StackElementType;

/**
 * Stack element identifier.
 */
typedef uint32_t GG_StackElementId;

/**
 * Stack port identifier.
 */
typedef uint32_t GG_StackPortId;

/**
 * Information about a stack element
 */
typedef struct {
    GG_StackElementId   id;   ///< Identifier for the element
    GG_StackElementType type; ///< Type of the element
} GG_StackElementInfo;

/**
 * I/O interfaces that are exposed by stack elements.
 * A stack element typically has 1 or 2 ports that it communicates through.
 */
typedef struct {
    GG_StackPortId id;     ///< Identifier for the port
    GG_DataSource* source; ///< Data source interface for an element I/O port
    GG_DataSink*   sink;   ///< Data sink interface for an element I/O port
} GG_StackElementPortInfo;

/**
 * IP configuration for a stack
 */
typedef struct {
    GG_IpAddress local_address;             ///< Local IP address          (use 0.0.0.0 for default)
    GG_IpAddress remote_address;            ///< Remote IP address         (use 0.0.0.0 for default)
    GG_IpAddress if_netmask;                ///< Network interface netmask (use 0.0.0.0 for default)
    uint16_t     ip_mtu;                    ///< IP MTU                    (use 0 for default)
    struct {
        bool     enabled;                   ///< True when header compression is enabled
        uint16_t default_udp_port;          ///< Default UDP port used with header compression
    }            header_compression;        ///< Header compression configuration
    struct {
        bool         enabled;               ///< True when address remapping is enabled
        GG_IpAddress source_address;        ///< Remap this source address to the remote address
        GG_IpAddress destination_address;   ///< Remap this destination address to the local address
    }            inbound_address_remapping; ///< Inbound address remapping
} GG_StackIpConfiguration;

/**
 * Role of a stack
 */
typedef enum {
    GG_STACK_ROLE_HUB,
    GG_STACK_ROLE_NODE
} GG_StackRole;

/**
 * Construction parameters for an Activity Monitor element
 */
typedef struct {
    uint32_t inactivity_timeout; ///< Delay before detecting inactivity in ms (use 0 for default)
} GG_StackElementActivityMonitorParameters;

/**
 * Construction parameters for a Gattlink element
 */
typedef struct {
    uint8_t  rx_window;                         ///< Size of the RX window         (use 0 for default)
    uint8_t  tx_window;                         ///< Size of the TX window         (use 0 for default)
    size_t   buffer_size;                       ///< Size of the buffer            (use 0 for default)
    uint16_t initial_max_fragment_size;         ///< Initial maximum fragment size (use 0 for default)
    const GG_GattlinkProbeConfig* probe_config; ///< Configuration for data probe  (use NULL to disable)
} GG_StackElementGattlinkParameters;

/**
 * Construction parameters for a UDP Datagram Socket element
 */
typedef struct {
    uint16_t local_port;  ///< Local UDP port number
    uint16_t remote_port; ///< Remote UDP port number
} GG_StackElementDatagramSocketParameters;

/**
 * Entry in a list of construction parameters pass when building a stack
 *
 * Depending on the element_type field, the element_parameters field points
 * to a type-specific struct, as follows:
 *   - GG_STACK_ELEMENT_TYPE_ACTIVITY_MONITOR: GG_StackElementActivityMonitorParameters
 *   - GG_STACK_ELEMENT_TYPE_GATTLINK: GG_StackElementGattlinkParameters
 *   - GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET: GG_StackElementDatagramSocketParameters
 *   - GG_STACK_ELEMENT_TYPE_DTLS_CLIENT: GG_TlsClientOptions
 *   - GG_STACK_ELEMENT_TYPE_DTLS_SERVER: GG_TlsServerOptions
 */
typedef struct {
    GG_StackElementType element_type;       ///< Element type to which the parameters apply
    const void*         element_parameters; ///< Pointer to a type-specific parameter struct,
                                            ///< or NULL for defaults
} GG_StackBuilderParameters;

/**
 * Event emitted by a stack when it is forwarding an event from one of its elements.
 *
 * (cast a GG_Event to that type when the event's type ID is GG_EVENT_TYPE_STACK_EVENT_FORWARD)
 */
typedef struct {
    GG_Event        base;
    const GG_Event* forwarded; ///< The event that is forwarded
} GG_StackForwardEvent;

/**
 * Event emitted when a link MTU has changed.
 *
 * (cast a GG_Event to that type when the event's type ID is GG_EVENT_TYPE_LINK_MTU_CHANGE)
 */
typedef struct {
    GG_Event     base;
    unsigned int link_mtu; ///< Value of the new MTU
} GG_StackLinkMtuChangeEvent;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_STACK_ELEMENT_TYPE_ACTIVITY_MONITOR     GG_4CC('a', 'm', 'o', 'n') ///< Activity Monitor element
#define GG_STACK_ELEMENT_TYPE_GATTLINK             GG_4CC('g', 'l', 'n', 'k') ///< Gattlink element
#define GG_STACK_ELEMENT_TYPE_IP_NETWORK_INTERFACE GG_4CC('n', 'e', 't', 'i') ///< Network Interface element
#define GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET      GG_4CC('u', 'd', 'p', 's') ///< UDP Socket element
#define GG_STACK_ELEMENT_TYPE_DTLS_CLIENT          GG_4CC('t', 'l', 's', 'c') ///< DTLS Client element
#define GG_STACK_ELEMENT_TYPE_DTLS_SERVER          GG_4CC('t', 'l', 's', 's') ///< DTLS Server element

#define GG_STACK_ELEMENT_ID_TOP                    0  ///< Virtual element ID for the top-most element of a stack
#define GG_STACK_ELEMENT_ID_BOTTOM                 1  ///< Virtual element ID for the bottom-most element of a stack

#define GG_STACK_PORT_ID_TOP                       0  ///< Port at the top side of an element
#define GG_STACK_PORT_ID_BOTTOM                    1  ///< Port at the bottom side of an element

#define GG_EVENT_TYPE_STACK_EVENT_FORWARD          GG_4CC('s', 't', 'k', 'f') ///< @see GG_StackForwardEvent
#define GG_EVENT_TYPE_LINK_MTU_CHANGE              GG_4CC('m', 't', 'u', 'c') ///< @see GG_StackLinkMtuChangeEvent

#define GG_STACK_ELEMENT_ACTIVITY_MONITOR_DEFAULT_TIMEOUT 30000

/*----------------------------------------------------------------------
|   standard stack configurations
+---------------------------------------------------------------------*/

/**
 * Stack with only a Gattlink element.
 *
 * <pre>
 *              <top>
 *
 *      [sink]        [source]
 * +----------------------------+
 * |    Gattlink  ('glnk')      |
 * +----------------------------+
 *      [source]      [sink]
 *
 *            <bottom>
 * </pre>
 *
 * Element Configuration parameters:
 *   - GG_StackElementGattlinkParameters (optional, omit for defaults)
 *
 * Construction parameters:
 *   - source, sink (required)
 */
#define GG_STACK_DESCRIPTOR_GATTLINK_ONLY "G"

/**
 * Stack with Gattlink and a Network Interface
 *
 * <pre>
 *             <top>
 *
 *       +~~~~~~~~~~~~~~~~+
 *       |       IP       |  (not exposed as a stack element)
 *       +~~~~~~~~~~~~~~~~+
 *    {internal communication}
 * +----------------------------+
 * | Network Interface ('neti') |
 * +----------------------------+
 *      [source]      [sink]
 *         |             |
 *      [sink]        [source]
 * +----------------------------+
 * |        Gattlink            |
 * +----------------------------+
 *      [source]      [sink]
 *
 *           <bottom>
 * </pre>
 *
 * Element Configuration parameters:
 *   - GG_StackElementGattlinkParameters (optional, omit for defaults)
 *
 * Construction parameters:
 *   - source, sink (required)
 *   - ip_configuration (optional, pass NULL for defaults)
 */
#define GG_STACK_DESCRIPTOR_NETIF_GATTLINK "NG"

/**
 * Stack with Gattlink, a Network Interface, and a UDP socket
 *
 * <pre>
 *              <top>
 *
 *      [sink]        [source]
 * +------------------------------+
 * | UDP Datagram Socket ('udps') |
 * +------------------------------+
 *    {internal communication}
 *       +~~~~~~~~~~~~~~~~+
 *       |       IP       |  (not exposed as a stack element)
 *       +~~~~~~~~~~~~~~~~+
 *    {internal communication}
 * +----------------------------+
 * | Network Interface ('neti') |
 * +----------------------------+
 *      [source]      [sink]
 *         |             |
 *      [sink]        [source]
 * +----------------------------+
 * |        Gattlink            |
 * +----------------------------+
 *      [source]      [sink]
 *
 *            <bottom>
 * </pre>
 *
 * Element Configuration parameters:
 *   - GG_StackElementGattlinkParameters (optional, omit for defaults)
 *   - GG_StackElementDatagramSocketParameters (optional, omit for defaults)
 *
 * Construction parameters:
 *   - source, sink (required)
 *   - ip_configuration (optional, pass NULL for defaults)
 */
#define GG_STACK_DESCRIPTOR_SOCKET_NETIF_GATTLINK "SNG"

/**
 * Stack with Gattlink, a Network Interface, a UDP socket and DTLS
 *
 * <pre>
 *             <top>
 *
 *      [sink]        [source]
 * +----------------------------+
 * |   DTLS ('tlss' or 'tlsc')  |
 * +----------------------------+
 *      [source]      [sink]
 *         |             |
 *      [sink]        [source]
 * +------------------------------+
 * | UDP Datagram Socket ('udps') |
 * +------------------------------+
 *    {internal communication}
 *       +~~~~~~~~~~~~~~~~+
 *       |       IP       |  (not exposed as a stack element)
 *       +~~~~~~~~~~~~~~~~+
 *    {internal communication}
 * +----------------------------+
 * | Network Interface ('neti') |
 * +----------------------------+
 *      [source]      [sink]
 *         |             |
 *      [sink]        [source]
 * +----------------------------+
 * |        Gattlink            |
 * +----------------------------+
 *      [source]      [sink]
 *
 *           <bottom>
 * </pre>
 *
 * Element Configuration parameters:
 *   - GG_StackElementGattlinkParameters (optional, omit for defaults)
 *   - GG_StackElementDatagramSocketParameters (optional, omit for defaults)
 *   - GG_TlsServerOptions (required in Server mode, omit in Client mode)
 *   - GG_TlsClientOptions (required in Client mode, omit in Server mode)
 *
 * Construction parameters:
 *   - source, sink (required)
 *   - ip_configuration (optional, pass NULL for defaults)
*/
#define GG_STACK_DESCRIPTOR_DTLS_SOCKET_NETIF_GATTLINK  "DSNG"

/**
 * Stack with a UDP socket and DTLS
 *
 * <pre>
 *             <top>
 *
 *      [sink]        [source]
 * +----------------------------+
 * |   DTLS ('tlss' or 'tlsc')  |
 * +----------------------------+
 *      [source]      [sink]
 *         |             |
 *      [sink]        [source]
 * +------------------------------+
 * | UDP Datagram Socket ('udps') |
 * +------------------------------+
 *    {internal communication}
 *       +~~~~~~~~~~~~~~~~+
 *       |       IP       |  (not exposed as a stack element)
 *       +~~~~~~~~~~~~~~~~+
 *
 *           <bottom>
 * </pre>
 *
 * Element Configuration parameters:
 *   - GG_StackElementDatagramSocketParameters (optional, omit for defaults)
 *   - GG_TlsServerOptions (required in Server mode, omit in Client mode)
 *   - GG_TlsClientOptions (required in Client mode, omit in Server mode)
 *
 * Construction parameters:
 *   - ip_configuration (optional, pass NULL for defaults)
 */
#define GG_STACK_DESCRIPTOR_DTLS_SOCKET "DS"

/**
 * Stack with just a Network Interface
 *
 * <pre>
 *             <top>
 *
 *       +~~~~~~~~~~~~~~~~+
 *       |       IP       |  (not exposed as a stack element)
 *       +~~~~~~~~~~~~~~~~+
 *    {internal communication}
 * +----------------------------+
 * | Network Interface ('neti') |
 * +----------------------------+
 *      [source]      [sink]
 *
 *           <bottom>
 * </pre>
 *
 * Element Configuration parameters:
 *   - none
 *
 * Construction parameters:
 *   - source, sink (required)
 *   - ip_configuration (optional, pass NULL for defaults)
*/
#define GG_STACK_DESCRIPTOR_NETIF "N"

/**
 * Stack with Transport Activity Monitor, Gattlink, a Network Interface, a UDP socket and DTLS
 *
 * <pre>
 *             <top>
 *
 *      [sink]        [source]
 * +----------------------------+
 * |   DTLS ('tlss' or 'tlsc')  |
 * +----------------------------+
 *      [source]      [sink]
 *         |             |
 *      [sink]        [source]
 * +------------------------------+
 * | UDP Datagram Socket ('udps') |
 * +------------------------------+
 *    {internal communication}
 *       +~~~~~~~~~~~~~~~~+
 *       |       IP       |  (not exposed as a stack element)
 *       +~~~~~~~~~~~~~~~~+
 *    {internal communication}
 * +----------------------------+
 * | Network Interface ('neti') |
 * +----------------------------+
 *      [source]      [sink]
 *         |             |
 *      [sink]        [source]
 * +----------------------------+
 * |        Gattlink            |
 * +----------------------------+
 *      [source]      [sink]
  *         |             |
 *      [sink]        [source]
 * +----------------------------+
 * |        Activity Monitor    |
 * +----------------------------+
 *      [source]      [sink]
 *
 *           <bottom>
 * </pre>
 *
 * Element Configuration parameters:
 *   - GG_StackElementActivityMonitorParameters (optional, omit for defaults)
 *   - GG_StackElementGattlinkParameters (optional, omit for defaults)
 *   - GG_StackElementDatagramSocketParameters (optional, omit for defaults)
 *   - GG_TlsServerOptions (required in Server mode, omit in Client mode)
 *   - GG_TlsClientOptions (required in Client mode, omit in Server mode)
 *
 * Construction parameters:
 *   - source, sink (required)
 *   - ip_configuration (optional, pass NULL for defaults)
*/
#define GG_STACK_DESCRIPTOR_DTLS_SOCKET_NETIF_GATTLINK_ACTIVITY "DSNGA"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Build a stack.
 *
 * Depending on the stack that is requested, the caller may pass construction
 * parameters and/or an IP configuration when non-default values are needed.
 *
 * @param descriptor A descriptor that uniquely identifies the stack that is requested.
 * @param parameters Pointer to an array of element construction parameters.
 * @param parameter_count Number of entries in the parameters array.
 * @param role Role that this stack should assume.
 * @param ip_configuration IP Configuration for the stack, or NULL to let the stack builder decide.
 * @param loop Loop in which the stack will run.
 * @param source Data source to connect the bottom of the stack to, or NULL if not applicable.
 * NOTE: when this parameter is not NULL, the stack builder will connect that source to the
 * bottom element of the stack, but not keep a reference to that object. It is the responsibility
 * of the caller to disconnect that source from the stack before destroying the stack.
 * @param sink Data sink to connect the bottom of the stack to, or NULL if not applicable.
 * @param stack [out] Pointer to the variable where the built stack will be returned.
 *
 * @return GG_SUCCESS if the stack could be built, or a negative error code.
 */
GG_Result GG_StackBuilder_BuildStack(const char*                      descriptor,
                                     const GG_StackBuilderParameters* parameters,
                                     size_t                           parameter_count,
                                     GG_StackRole                     role,
                                     const GG_StackIpConfiguration*   ip_configuration,
                                     GG_Loop*                         loop,
                                     GG_DataSource*                   source,
                                     GG_DataSink*                     sink,
                                     GG_Stack**                       stack);

/**
 * Destroy/teardown a stack
 */
void GG_Stack_Destroy(GG_Stack* self);

/**
 * Start a stack.
 *
 * Starting a stack should be done after the stack has been built and
 * the user of the stack is ready for data to start flowing through the
 * stack.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the stack could be started, or a negative error code.
 */
GG_Result GG_Stack_Start(GG_Stack* self);

/**
 * Reset a stack.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the stack could be reset, or a negative error code.
 */
GG_Result GG_Stack_Reset(GG_Stack* self);

/**
 * Get the event emitter interface of a stack.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_EventEmitter interface for the object.
 */
GG_EventEmitter* GG_Stack_AsEventEmitter(GG_Stack* self);

/**
 * Get the event listener interface of a stack.
 *
 * That interface allows sending events directly to the stack.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_EventEmitter interface for the object.
 */
GG_EventListener* GG_Stack_AsEventListener(GG_Stack* self);

/**
 * Get the inspectable interface of a stack.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_Inspectable interface for the object.
 */
GG_Inspectable* GG_Stack_AsInspectable(GG_Stack* self);

/**
 * Get the IP information associated with a stack, if any.
 *
 * @param self The object on which this method is invoked.
 * @param ip_configuration Pointer to the struct where the configuration will be written.
 *
 * @return GG_SUCCESS if an IP configuration can be returned for the stack,
 * or GG_ERROR_NOT_SUPPORTED if the stack does not have any IP configuration.
 */
GG_Result GG_Stack_GetIpConfiguration(GG_Stack* self, GG_StackIpConfiguration* ip_configuration);

/**
 * Get the status for the DTLS element of the stack, if there is one.
 *
 * @param self The object on which this method is invoked.
 * @param status Pointer to the struct where the status will be returned.
 *
 * @return GG_SUCCESS if the stack has a DTLS element and its status could be obtained,
 * GG_ERROR_NO_SUCH_ITEM if the stack does not have a DTLS element, or a negative error code.
 */
GG_Result GG_Stack_GetDtlsProtocolStatus(GG_Stack* self, GG_DtlsProtocolStatus* status);

/**
 * Get I/O interfaces for a stack element.
 *
 * @param self The object on which this method is invoked.
 * @param element_id Id of the element that owns the port. A virtual element ID constants may be used.
 * @param port_id Id of the port to get within the element.
 * @param port [out] Address of the struct in which the port info will be returned.
 *
 * @return GG_SUCCESS if the port exists, or GG_ERROR_NO_SUCH_ITEM if it doesn't.
 */
GG_Result GG_Stack_GetPortById(GG_Stack* self, uint32_t element_id, uint32_t port_id, GG_StackElementPortInfo* port);

/**
 * Get the number of elements in a stack.
 *
 * @return The number of elements in the stack.
 */
size_t GG_Stack_GetElementCount(GG_Stack* self);

/**
 * Get an element by index.
 * This may be used, for example, to iterate over all the elements in a stack.
 * The elements in a stack are ordered from top to bottom, so the top-most element is at index 0.
 *
 * @param self The object on which this method is invoked.
 * @param element_index Index of the element to get (0-based).
 * @param element_info [out] Address of the struct in which the element info will be returned.
 *
 * @return GG_SUCCESS if the requested element exits in the stack, or a GG_ERROR_NO_SUCH_ITEM.
 */
GG_Result GG_Stack_GetElementByIndex(GG_Stack* self, unsigned int element_index, GG_StackElementInfo* element_info);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
