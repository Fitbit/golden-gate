/**
 *
 * @file
 *
 * @copyright
 * Copyright 2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-02-03
 *
 * @details
 *
 * NIP library implementation.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_io.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_utils.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/protocols/gg_ipv4_protocol.h"
#include "gg_nip.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.nip")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/

// dynamic UDP port numbers, according to http://www.iana.org/assignments/port-numbers:
#define GG_NIP_UDP_DYNAMIC_PORT_RANGE_START (0xC000)
#define GG_NIP_UDP_DYNAMIC_PORT_RANGE_END   (0xFFFF)
#define GG_NIP_UDP_DYNAMIC_PORT_RANGE_SPAN  (GG_NIP_UDP_DYNAMIC_PORT_RANGE_END - GG_NIP_UDP_DYNAMIC_PORT_RANGE_START)

#define GG_NIP_MAX_PACKET_SIZE                  0xFFFF
#define GG_NIP_IP_HEADER_SIZE                   20
#define GG_NIP_UDP_HEADER_SIZE                   8

#define GG_NIP_IP_VERSION_4                      4
#define GG_NIP_IP_PROTOCOL_UDP                   17

#define GG_NIP_IP_HEADER_VERSION_AND_IHL_OFFSET  0
#define GG_NIP_IP_HEADER_TOTAL_LENGTH_OFFSET     2
#define GG_NIP_IP_HEADER_IDENTIFICATION_OFFSET   4
#define GG_NIP_IP_HEADER_PROTOCOL_OFFSET         9
#define GG_NIP_IP_HEADER_CHECKSUM_OFFSET        10
#define GG_NIP_IP_HEADER_SRC_ADDR_OFFSET        12
#define GG_NIP_IP_HEADER_DST_ADDR_OFFSET        16

#define GG_NIP_UDP_HEADER_SRC_PORT_OFFSET       0
#define GG_NIP_UDP_HEADER_DST_PORT_OFFSET       2
#define GG_NIP_UDP_HEADER_LENGTH_OFFSET         4

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/

// those macros are simple and assume that data is a uint8_t*
#define GG_NIP_GET_16(data, offset) ((uint16_t)(((data)[(offset)] << 8) | (data)[(offset) + 1]))
#define GG_NIP_SET_16(data, offset, value) do {     \
    (data)[(offset)]     = (uint8_t)((value) >> 8); \
    (data)[(offset) + 1] = (uint8_t)(value);        \
} while (0)

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
static const uint8_t GG_NipIpUdpHeaderPrototype[10] = {
    0x45, // Version | IHL
    0x00, // DSCP | ECN
    0x00, // Total len MSB
    0x00, // Total len LSB
    0x00, // Identification MSB
    0x00, // Identification LSB
    0x00, // Flags | Fragment Offset MSB
    0x00, // Fragment Offset LSB
    0xFF, // TTL
    0x11  // Protocol (UDP)
};

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
typedef struct {
    bool          initialized;     ///< set to `true` when the stack has been initialized
    GG_LinkedList udp_endpoints;   ///< list of sockets attached to the stack
    uint8_t       header_template[GG_NIP_IP_HEADER_SIZE + GG_NIP_UDP_HEADER_SIZE];
    uint16_t      dynamic_port_scan_start; ///< starting point when looking for an unassigned dynamic port
    uint16_t      next_ip_identification;  ///< counter for the IP identification field

    // the following fields represent the single network interface
    struct {
        GG_IMPLEMENTS(GG_DataSource);       ///< To send data to the transport
        GG_IMPLEMENTS(GG_DataSink);         ///< To receive data from the transport
        GG_IMPLEMENTS(GG_DataSinkListener); ///< To receive notifications from the transport sink

        uint32_t     address;        ///< IP address assigned to the network interface
        GG_DataSink* transport_sink; ///< Transport data sink
    } netif;
} GG_NipStack;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_NipStack GG_IpStack; ///< IP stack singleton

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_Nip_Configure(const GG_IpAddress* netif_address)
{
    // lazy-initialize
    GG_Result result = GG_Nip_Initialize();
    if (GG_FAILED(result)) {
        return result;
    }

    // assign the netif IP address
    GG_IpStack.netif.address = GG_IpAddress_AsInteger(netif_address);

    // fill in the IP+UDP header template
    memcpy(GG_IpStack.header_template, GG_NipIpUdpHeaderPrototype, sizeof(GG_NipIpUdpHeaderPrototype));
    memset(&GG_IpStack.header_template[sizeof(GG_NipIpUdpHeaderPrototype)],
           0,
           sizeof(GG_IpStack.header_template) - sizeof(GG_NipIpUdpHeaderPrototype));
    GG_BytesFromInt32Be(&GG_IpStack.header_template[GG_NIP_IP_HEADER_SRC_ADDR_OFFSET], GG_IpStack.netif.address);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// This function is called when a socket user sends a UDP datagram.
// It creates a packet with an IP and UDP header, followed by the payload,
// and sends it to the transport.
// The optional UDP checksum is left un-computed.
//----------------------------------------------------------------------
static GG_Result
GG_NipUdpEndpoint_PutData(GG_DataSink*             _self,
                          GG_Buffer*               data,
                          const GG_BufferMetadata* metadata)
{
    GG_ASSERT(data);
    GG_NipUdpEndpoint* self = GG_SELF(GG_NipUdpEndpoint, GG_DataSink);

    // check that the datagram can fit
    size_t payload_size = GG_Buffer_GetDataSize(data);
    size_t packet_size  =  payload_size + GG_NIP_IP_HEADER_SIZE + GG_NIP_UDP_HEADER_SIZE;
    if (packet_size > GG_NIP_MAX_PACKET_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // check that we have a network interface transport to send to
    if (!GG_IpStack.netif.transport_sink) {
        return GG_ERROR_NETWORK_UNREACHABLE;
    }

    // decide what destination address to use
    uint32_t dst_address = 0;
    uint32_t dst_port    = self->remote_address.port;
    if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS && !self->connected) {
        // use the destination specified by the metadata
        const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)metadata;
        dst_address = GG_IpAddress_AsInteger(&socket_metadata->socket_address.address);
        dst_port    = socket_metadata->socket_address.port;
    } else {
        // use the destination specified at creation time
        dst_address = GG_IpAddress_AsInteger(&self->remote_address.address);
        dst_port    = self->remote_address.port;
    }

    // check that the destination is valid
    if (dst_address == 0 || dst_port == 0) {
        GG_LOG_WARNING("invalid destination address or port (%08x:%u)", (int)dst_address, (int)dst_port);
        return GG_ERROR_INVALID_STATE;
    }

    // allocate a buffer large enough for the IP+UDP header and payload
    GG_DynamicBuffer* packet = NULL;
    GG_Result result = GG_DynamicBuffer_Create(packet_size, &packet);
    if (GG_FAILED(result)) {
        return result;
    }

    // start with the header template
    GG_DynamicBuffer_SetDataSize(packet, packet_size);
    uint8_t* packet_data = GG_DynamicBuffer_UseData(packet);
    memcpy(packet_data, GG_IpStack.header_template, sizeof(GG_IpStack.header_template));

    // fill in the template blanks for the IP header
    uint8_t* ip_header = packet_data;
    GG_BytesFromInt32Be(&ip_header[GG_NIP_IP_HEADER_DST_ADDR_OFFSET], dst_address);
    GG_NIP_SET_16(ip_header, GG_NIP_IP_HEADER_TOTAL_LENGTH_OFFSET, packet_size);
    uint16_t identification = GG_IpStack.next_ip_identification++; // it is normal for the counter to cycle
    GG_NIP_SET_16(ip_header, GG_NIP_IP_HEADER_IDENTIFICATION_OFFSET, identification);
    uint16_t checksum = ~GG_Ipv4Checksum(ip_header, GG_NIP_IP_HEADER_SIZE);
    GG_NIP_SET_16(ip_header, GG_NIP_IP_HEADER_CHECKSUM_OFFSET, checksum);

    // fill in the template blanks for the UDP header
    uint16_t udp_length = (uint16_t)(packet_size - GG_NIP_IP_HEADER_SIZE);
    uint8_t* udp_header = &packet_data[GG_NIP_IP_HEADER_SIZE];
    GG_NIP_SET_16(udp_header, GG_NIP_UDP_HEADER_SRC_PORT_OFFSET, self->local_address.port);
    GG_NIP_SET_16(udp_header, GG_NIP_UDP_HEADER_DST_PORT_OFFSET, self->remote_address.port);
    GG_NIP_SET_16(udp_header, GG_NIP_UDP_HEADER_LENGTH_OFFSET,   udp_length);

    // copy the payload
    uint8_t* udp_payload = &packet_data[GG_NIP_IP_HEADER_SIZE + GG_NIP_UDP_HEADER_SIZE];
    memcpy(udp_payload, GG_Buffer_GetData(data), payload_size);

    // send the packet to the transport
    result = GG_DataSink_PutData(GG_IpStack.netif.transport_sink, GG_DynamicBuffer_AsBuffer(packet), NULL);

    // done with the packet
    GG_DynamicBuffer_Release(packet);

    return result;
}

//----------------------------------------------------------------------
static GG_Result
GG_NipUdpEndpoint_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    GG_NipUdpEndpoint* self = GG_SELF(GG_NipUdpEndpoint, GG_DataSink);

    self->data_sink_listener = listener;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_NipUdpEndpoint_SetDataSink(GG_DataSource* _self, GG_DataSink* data_sink)
{
    GG_NipUdpEndpoint* self = GG_SELF(GG_NipUdpEndpoint, GG_DataSource);

    // keep a reference to the sink
    self->data_sink = data_sink;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_NipUdpEndpoint_OnCanPut(GG_DataSinkListener* _self)
{
    // nothing to do, in this implementation we don't keep a pending buffer queue
    GG_COMPILER_UNUSED(_self);
}

/*----------------------------------------------------------------------
|   GG_NipUdpEndpoint function tables
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_NipUdpEndpoint, GG_DataSink) {
    .PutData     = GG_NipUdpEndpoint_PutData,
    .SetListener = GG_NipUdpEndpoint_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_NipUdpEndpoint, GG_DataSource) {
    .SetDataSink = GG_NipUdpEndpoint_SetDataSink
};

GG_IMPLEMENT_INTERFACE(GG_NipUdpEndpoint, GG_DataSinkListener) {
    .OnCanPut = GG_NipUdpEndpoint_OnCanPut
};

//----------------------------------------------------------------------
void
GG_NipUdpEndpoint_Init(GG_NipUdpEndpoint*      self,
                       const GG_SocketAddress* local_address,
                       const GG_SocketAddress* remote_address,
                       bool                    connect_to_remote)
{
    // clear the memory first
    memset(self, 0, sizeof(*self));

    // copy the fields
    if (local_address) {
        self->local_address = *local_address;
    }
    if (remote_address) {
        self->remote_address = *remote_address;
    }
    self->connected = connect_to_remote;

    // setup the vtable
    GG_SET_INTERFACE(self, GG_NipUdpEndpoint, GG_DataSink);
    GG_SET_INTERFACE(self, GG_NipUdpEndpoint, GG_DataSource);
    GG_SET_INTERFACE(self, GG_NipUdpEndpoint, GG_DataSinkListener);
}

//----------------------------------------------------------------------
// Returns `true` if the specified port number is already assigned to
// a socket.
//----------------------------------------------------------------------
static bool
GG_Nip_UdpSrcPortInUse(uint16_t port)
{
    GG_LINKED_LIST_FOREACH(list_node, &GG_IpStack.udp_endpoints) {
        GG_NipUdpEndpoint* udp_endpoint = GG_LINKED_LIST_ITEM(list_node, GG_NipUdpEndpoint, list_node);
        if (udp_endpoint->local_address.port == port) {
            return true;
        }
    }

    return false;
}

//----------------------------------------------------------------------
GG_Result
GG_Nip_AddUdpEndpoint(GG_NipUdpEndpoint* udp_endpoint)
{
    // lazy-initialize
    GG_Result result = GG_Nip_Initialize();
    if (GG_FAILED(result)) {
        return result;
    }

    // check that this endpoint isn't already linked
    if (!GG_LINKED_LIST_NODE_IS_UNLINKED(&udp_endpoint->list_node)) {
        return GG_ERROR_INVALID_STATE;
    }

    // if the local address isn't set, use the interface address
    if (GG_IpAddress_IsAny(&udp_endpoint->local_address.address)) {
        GG_IpAddress_SetFromInteger(&udp_endpoint->local_address.address, GG_IpStack.netif.address);
    }

    // if the port is 0, find a free port
    if (udp_endpoint->local_address.port == 0) {
        udp_endpoint->local_port_bound = false;
        for (unsigned int i = 0; i < GG_NIP_UDP_DYNAMIC_PORT_RANGE_SPAN; i++) {
            uint16_t port = (uint16_t)(GG_NIP_UDP_DYNAMIC_PORT_RANGE_START +
                                       ((GG_IpStack.dynamic_port_scan_start + i) %
                                        GG_NIP_UDP_DYNAMIC_PORT_RANGE_SPAN));
            if (!GG_Nip_UdpSrcPortInUse(port)) {
                udp_endpoint->local_address.port = port;
                GG_IpStack.dynamic_port_scan_start = (i + 1) % GG_NIP_UDP_DYNAMIC_PORT_RANGE_SPAN;
                break;
            }
        }

        // return now if we couldn't find a free dynamic port
        if (udp_endpoint->local_address.port == 0) {
            return GG_ERROR_OUT_OF_RESOURCES;
        }
    } else {
        // check that this port isn't already used
        if (GG_Nip_UdpSrcPortInUse(udp_endpoint->local_address.port)) {
            GG_LOG_WARNING("UDP port already in use");
            return GG_ERROR_ADDRESS_IN_USE;
        }
        udp_endpoint->local_port_bound = true;
    }

    // add the endpoint to the list
    GG_LINKED_LIST_APPEND(&GG_IpStack.udp_endpoints, &udp_endpoint->list_node);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Nip_RemoveEndpoint(GG_NipUdpEndpoint* udp_endpoint)
{
    if (GG_LINKED_LIST_NODE_IS_UNLINKED(&udp_endpoint->list_node)) {
        return GG_ERROR_INVALID_STATE;
    }

    GG_LINKED_LIST_NODE_REMOVE(&udp_endpoint->list_node);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
// This function is called when a UDP packet has been received from
// the transport
//----------------------------------------------------------------------
static void
GG_NipStack_OnUdpPacketReceived(GG_Buffer* packet,
                                size_t     packet_offset,
                                size_t     packet_size,
                                uint32_t   src_address)
{
    // check the size
    if (packet_size < GG_NIP_UDP_HEADER_SIZE) {
        GG_LOG_WARNING("UDP packet too short");
        return;
    }

    // check the length
    const uint8_t* udp_header = GG_Buffer_GetData(packet) + packet_offset;
    uint16_t udp_length = GG_NIP_GET_16(udp_header, GG_NIP_UDP_HEADER_LENGTH_OFFSET);
    if (udp_length != packet_size) {
        // uh oh... mismatch
        GG_LOG_WARNING("UDP length mismatch (expected %u, got %u)", (int)packet_size, (int)udp_length);
        return;
    }

    // get the source and destination ports
    uint16_t src_port = GG_NIP_GET_16(udp_header, GG_NIP_UDP_HEADER_SRC_PORT_OFFSET);
    uint16_t dst_port = GG_NIP_GET_16(udp_header, GG_NIP_UDP_HEADER_DST_PORT_OFFSET);
    GG_LOG_FINEST("UDP src_port = %d, dst_port = %d", src_port, dst_port);

    // find a matching socket to deliver to (either not locally bound, or a direct port match)
    GG_LINKED_LIST_FOREACH(list_node, &GG_IpStack.udp_endpoints) {
        GG_NipUdpEndpoint* udp_endpoint = GG_LINKED_LIST_ITEM(list_node, GG_NipUdpEndpoint, list_node);
        if (!udp_endpoint->local_port_bound || udp_endpoint->local_address.port == dst_port) {
            GG_LOG_FINER("found matching socket");

            // check that the socket has a sink to deliver to
            if (udp_endpoint->data_sink == NULL) {
                GG_LOG_INFO("socket has no sink, dropping");
                return;
            }

            // create a packet with just the payload of the packet, without the header
            GG_Buffer* payload = NULL;
            GG_Result result = GG_SubBuffer_Create(packet,
                                                   packet_offset + GG_NIP_UDP_HEADER_SIZE,
                                                   packet_size - GG_NIP_UDP_HEADER_SIZE,
                                                   &payload);
            if (GG_FAILED(result)) {
                GG_LOG_WARNING("failed to create payload buffer (%d)", result);
                return;
            }

            // deliver the payload (ignore errors here, as we don't want to maintain a packet queue)
            GG_SocketAddressMetadata metadata;
            metadata.base = GG_BUFFER_METADATA_INITIALIZER(SOURCE_SOCKET_ADDRESS, GG_SocketAddressMetadata);
            GG_IpAddress_SetFromInteger(&metadata.socket_address.address, src_address);
            metadata.socket_address.port = src_port;
            GG_DataSink_PutData(udp_endpoint->data_sink, payload, &metadata.base);

            // done
            GG_Buffer_Release(payload);
            return;
        }
    }

    // no matching socket found
    GG_LOG_INFO("no matching socket found");
}

//----------------------------------------------------------------------
// This function is called when data arrives from the network interface
// transport.
//----------------------------------------------------------------------
static GG_Result
GG_NipStack_PutData(GG_DataSink*             self,
                    GG_Buffer*               data,
                    const GG_BufferMetadata* metadata)
{
    GG_ASSERT(data);
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(metadata);

    // get the packet data and size
    const uint8_t* packet_data = GG_Buffer_GetData(data);
    size_t packet_size = GG_Buffer_GetDataSize(data);
    GG_LOG_FINER("received packet from netif transport: size=%u", (int)packet_size);

    // quick sanity check
    if (packet_size < GG_NIP_IP_HEADER_SIZE) {
        GG_LOG_WARNING("packet too short");
        return GG_SUCCESS;
    }

    // check the IP header version and size
    if ((packet_data[GG_NIP_IP_HEADER_VERSION_AND_IHL_OFFSET] >> 4) != GG_NIP_IP_VERSION_4) {
        GG_LOG_WARNING("unsupported IP version");
    }
    unsigned int header_size = 4 * (packet_data[GG_NIP_IP_HEADER_VERSION_AND_IHL_OFFSET] & 0xF);
    if (header_size < GG_NIP_IP_HEADER_SIZE) {
        GG_LOG_WARNING("ihl too small");
        return GG_SUCCESS;
    }

    // check that we support this protocol
    if (packet_data[GG_NIP_IP_HEADER_PROTOCOL_OFFSET] != GG_NIP_IP_PROTOCOL_UDP) {
        GG_LOG_INFO("dropping non-UDP packet (protocol = %d)", packet_data[GG_NIP_IP_HEADER_PROTOCOL_OFFSET]);
        return GG_SUCCESS;
    }

    // check the total length
    size_t total_length = GG_NIP_GET_16(packet_data, GG_NIP_IP_HEADER_TOTAL_LENGTH_OFFSET);
    if (total_length != packet_size) {
        GG_LOG_WARNING("packet length mismatch (expected %u, got %u)", (int)packet_size, (int)total_length);
        return GG_SUCCESS;
    }

    // check that this packet is for us
    uint32_t dst_address = GG_BytesToInt32Be(&packet_data[GG_NIP_IP_HEADER_DST_ADDR_OFFSET]);
    if (dst_address != GG_IpStack.netif.address) {
        GG_LOG_INFO("packet destination (%08x) isn't for our network interface", (int)dst_address);
        return GG_SUCCESS;
    }
    uint32_t src_address = GG_BytesToInt32Be(&packet_data[GG_NIP_IP_HEADER_SRC_ADDR_OFFSET]);
    GG_LOG_FINER("source address = %08x", (int)src_address);

    // process the packet
    GG_NipStack_OnUdpPacketReceived(data, header_size, packet_size - header_size, src_address);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_NipStack_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    // ignore the listener, as the network interface always accepts or drops
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_NipStack_OnCanPut(GG_DataSinkListener* _self)
{
    GG_NipStack* self = GG_SELF_M(netif, GG_NipStack, GG_DataSinkListener);

    // notify all sockets that it is a good time to (re)send any data that's ready
    // NOTE: a further optimization here may be to keep a flag for each endpoint
    // of whether it has received a GG_ERROR_WOULD_BLOCK condition earlier.
    GG_LINKED_LIST_FOREACH(list_node, &self->udp_endpoints) {
        GG_NipUdpEndpoint* udp_endpoint = GG_LINKED_LIST_ITEM(list_node, GG_NipUdpEndpoint, list_node);
        if (udp_endpoint->data_sink_listener) {
            GG_DataSinkListener_OnCanPut(udp_endpoint->data_sink_listener);
        }
    }
}

//----------------------------------------------------------------------
static GG_Result
GG_NipStack_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_NipStack* self = GG_SELF_M(netif, GG_NipStack, GG_DataSource);

    // de-register as a listener from any previous sink
    if (self->netif.transport_sink) {
        GG_DataSink_SetListener(self->netif.transport_sink, NULL);
    }

    // keep a reference to the sink
    self->netif.transport_sink = sink;

    // register as a listener with the new sink
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(&self->netif, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_NipStack function tables
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_NipStack, GG_DataSinkListener) {
    .OnCanPut = GG_NipStack_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_NipStack, GG_DataSink) {
    .PutData     = GG_NipStack_PutData,
    .SetListener = GG_NipStack_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_NipStack, GG_DataSource) {
    .SetDataSink = GG_NipStack_SetDataSink
};

//----------------------------------------------------------------------
GG_DataSink*
GG_Nip_AsDataSink(void)
{
    return GG_CAST(&GG_IpStack.netif, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_Nip_AsDataSource(void)
{
    return GG_CAST(&GG_IpStack.netif, GG_DataSource);
}

//----------------------------------------------------------------------
GG_Result
GG_Nip_Initialize(void)
{
    if (GG_IpStack.initialized) {
        return GG_SUCCESS;
    }

    // initialize fields
    GG_LINKED_LIST_INIT(&GG_IpStack.udp_endpoints);
    GG_IpStack.dynamic_port_scan_start = GG_NIP_UDP_DYNAMIC_PORT_RANGE_START;

    // setup the vtable for the netif
    GG_SET_INTERFACE(&GG_IpStack.netif, GG_NipStack, GG_DataSource);
    GG_SET_INTERFACE(&GG_IpStack.netif, GG_NipStack, GG_DataSink);
    GG_SET_INTERFACE(&GG_IpStack.netif, GG_NipStack, GG_DataSinkListener);

    // done
    GG_IpStack.initialized = true;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Nip_Terminate(void)
{
    // detach from any previous transport we may have
    GG_IpStack.netif.transport_sink = NULL;

    // done
    GG_IpStack.initialized = false;
}
