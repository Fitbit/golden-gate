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
 * @date 2018-04-15
 *
 * @details
 *
 * Stack builder implementation base.
 */
#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "gg_stack_builder.h"
#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/protocols/gg_ipv4_protocol.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/tls/gg_tls.h"
#include "xp/utils/gg_activity_data_monitor.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup StackBuilder Stack Builder
//! Stack Builder Base
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Element port
 */
typedef struct {
    GG_DataSource* source; ///< Source for the port (may be NULL)
    GG_DataSink*   sink;   ///< Sink for the port (may be NULL)
} GG_StackElementPort;

/**
 * Base class for stack elements
 */
typedef struct {
    GG_Stack*           stack;       ///< Stack to which this element belongs
    GG_StackElementId   id;          ///< Element ID
    GG_StackElementType type;        ///< Element type
    GG_StackElementPort top_port;    ///< Top port
    GG_StackElementPort bottom_port; ///< Bottom port
} GG_StackElement;

/**
 * Element types that are declared here but defined in specific ports implementations
 */
typedef struct GG_StackNetworkInterfaceElement GG_StackNetworkInterfaceElement;

/**
 * Transport Monitor stack element
 */
typedef struct {
    GG_StackElement         base;
    GG_ActivityDataMonitor* bottom_to_top_monitor;
    GG_ActivityDataMonitor* top_to_bottom_monitor;
} GG_StackActivityMonitorElement;

/**
 * Gattlink stack element
 */
typedef struct {
    GG_StackElement           base;
    GG_Ipv4FrameSerializer*   frame_serializer;
    GG_Ipv4FrameAssembler*    frame_assembler;
    GG_GattlinkGenericClient* client;
} GG_StackGattlinkElement;

/**
 * Datagram Socket stack element
 */
typedef struct {
    GG_StackElement    base;
    GG_DatagramSocket* socket;
} GG_StackDatagramSocketElement;

/**
 * DTLS Client or Server
 */
typedef struct {
    GG_StackElement    base;
    GG_DtlsProtocol*   dtls;
    GG_TlsProtocolRole role;
} GG_StackDtlsElement;

/**
 * Stack implementation
 */
struct GG_Stack {
    GG_IMPLEMENTS(GG_EventListener);
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    GG_StackElement**       elements;
    size_t                  element_count;
    GG_StackIpConfiguration ip_configuration;
    unsigned int            max_datagram_size;
    GG_StackRole            role;
    GG_Loop*                loop;
    GG_EventEmitterBase     event_emitter;
    bool                    secure; ///< True if the stack contains a DTLS protocol element
    size_t                  index;  ///< The value of the global stack instance counter when this stack was created

    // shortcuts to known element types
    GG_StackActivityMonitorElement*  activity_monitor_element;
    GG_StackGattlinkElement*         gattlink_element;
    GG_StackNetworkInterfaceElement* netif_element;
    GG_StackDatagramSocketElement*   datagram_socket_element;
    GG_StackDtlsElement*             dtls_element;

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_STACK_BUILDER_ID_BASE                             (1024)

#define GG_STACK_BUILDER_DEFAULT_GATTLINK_FRAGMENT_SIZE      (20)
#define GG_STACK_BUILDER_DEFAULT_IP_MTU                      (1280)

#define GG_STACK_BUILDER_DEFAULT_UDP_SOCKET_PORT             (5683)
#define GG_STACK_BUILDER_DEFAULT_DTLS_SOCKET_PORT            (5684)

#define GG_STACK_BUILDER_DEFAULT_NETIF_NETMASK               (0xFFFFFFFE) // 255.255.255.254

// By default, the node will use a local IP address = 169.254.X.Y where
// Y is an odd number >= 3 and a remote IP address = 169.254.X.(Y-1)
#define GG_STACK_BUILDER_DEFAULT_NODE_LOCAL_IP_ADDRESS_BASE  (0xA9FE0000) // 169.254.0.0
#define GG_STACK_BUILDER_DEFAULT_NODE_REMOTE_IP_ADDRESS_BASE (0xA9FE0000) // 169.254.0.0

// By default, the hub will use a local IP address = 169.254.X.Y where
// Y is an even number >= 2 and a remote IP address = 169.254.X.(Y+1)
#define GG_STACK_BUILDER_DEFAULT_HUB_LOCAL_IP_ADDRESS_BASE   (0xA9FE0000) // 169.254.0.0
#define GG_STACK_BUILDER_DEFAULT_HUB_REMOTE_IP_ADDRESS_BASE  (0xA9FE0000) // 169.254.0.0

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

// declarations for functions that are implemented in specific port elements
GG_Result GG_StackNetworkInterfaceElement_Create(GG_Stack*                         stack,
                                                 size_t                            netif_mtu,
                                                 GG_StackNetworkInterfaceElement** element);
void GG_StackNetworkInterfaceElement_Inspect(GG_StackNetworkInterfaceElement* self, GG_Inspector* inspector);

void GG_StackNetworkInterfaceElement_Destroy(GG_StackNetworkInterfaceElement* self);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
