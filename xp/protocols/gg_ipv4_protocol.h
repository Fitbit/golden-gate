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
 * @date 2017-11-21
 *
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_common.h"
#include "xp/protocols/gg_protocols.h"
#include "xp/sockets/gg_sockets.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_IPV4_MIN_IP_HEADER_SIZE 20
#define GG_IPV4_MAX_IP_HEADER_SIZE 60
#define GG_UDP_HEADER_SIZE         8

#define GG_IPV4_PROTOCOL_ICMP 1
#define GG_IPV4_PROTOCOL_TCP  6
#define GG_IPV4_PROTOCOL_UDP  17

// Offset of the Source IP Address field
#define GG_IPV4_HEADER_SOURCE_ADDRESS_OFFSET 12
// Offset of the Destination IP Address field
#define GG_IPV4_HEADER_DESTINATION_ADDRESS_OFFSET 16

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Frame Assember that can re-assemble IPv4 packets.
 */
typedef struct GG_Ipv4FrameAssembler GG_Ipv4FrameAssembler;

/**
 * Frame Serializer that can serialize IPv4 packets.
 */
typedef struct GG_Ipv4FrameSerializer GG_Ipv4FrameSerializer;

/**
 * IPv4 IP Packet Header
 */
typedef struct {
    uint8_t  version;
    uint8_t  ihl;
    uint8_t  dscp;
    uint8_t  ecn;
    uint16_t total_length;
    uint16_t identification;
    uint8_t  flags;
    uint16_t fragment_offset;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_address;
    uint32_t dst_address;
    uint8_t  options[40];
} GG_Ipv4PacketHeader;

/**
 * UDP Packet Header
 */
typedef struct {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} GG_UdpPacketHeader;

/**
 * IP Configuration used when creating GG_Ipv4FrameSerializer and GG_Ipv4FrameAssembler instances.
 * This configuration is used when compressing/decompression IPv4 and UDP headers: header fields with
 * matching values may be elided when compressing, and elided fields will be set to those values when
 * decompressing.
 */
typedef struct {
    uint32_t default_src_address; ///< Source address to elide/restore when compressing/decrompressing
    uint32_t default_dst_address; ///< Destination address to elide/restore when compressing/decrompressing
    uint16_t udp_src_ports[3];    ///< Source port numbers to elide/restore when compressing/decrompressing
    uint16_t udp_dst_ports[3];    ///< Destination port numbers to elide/restore when compressing/decrompressing
} GG_Ipv4FrameSerializationIpConfig;

/**
 * IP Address Remapping Configuration used when a frame assembler should remap a certain source
 * and destination address.
 */
typedef struct {
    uint32_t src_address;          ///< Source address to remap
    uint32_t remapped_src_address; ///< Source address to remap to
    uint32_t dst_address;          ///< Destination address to remap
    uint32_t remapped_dst_address; ///< Destination address to remap to
} GG_Ipv4FrameAssemnblerIpMap;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
/**
 * Create an GG_Ipv4FrameAssembler instance.
 *
 * @param max_packet_size Maximum packet size that can be re-assembled by the frame assembler.
 * @param ip_config Configuration options used for header compression. If NULL, no compression will be done.
 * @param ip_map Source/Destination IP address remapping info. If NULL, no remapping will be done.
 * @param assembler Pointer to where the new instance will be returned.
 *
 * @return GG_SUCCESS if an instance was created, or a negative error code.
 */
GG_Result GG_Ipv4FrameAssembler_Create(uint16_t                                 max_packet_size,
                                       const GG_Ipv4FrameSerializationIpConfig* ip_config,
                                       const GG_Ipv4FrameAssemnblerIpMap*       ip_map,
                                       GG_Ipv4FrameAssembler**                  assembler);

/**
 * Destroy an instance.
 *
 * @param self Object on which this method is called.
 */
void GG_Ipv4FrameAssembler_Destroy(GG_Ipv4FrameAssembler* self);

/**
 * Get the #GG_FrameAssembler interface for this object.
 *
 * @param self Object on this this method is called.
 * @return The #GG_FrameAssembler interface for this object.
 */
GG_FrameAssembler* GG_Ipv4FrameAssembler_AsFrameAssembler(GG_Ipv4FrameAssembler* self);

/**
 * Get the #GG_Inspectable interface for this object.
 *
 * @param self Object on this this method is called.
 * @return GG_Inspectable interface for this object.
 */
GG_Inspectable* GG_Ipv4FrameAssembler_AsInspectable(GG_Ipv4FrameAssembler* self);

/**
 * Create an GG_Ipv4FrameSerializer instance.
 *
 * @param ip_config Configuration options used for header decompression. If NULL, decompression won't be supported.
 * @param serializer Pointer to where the new instance will be returned.
 *
 * @return GG_SUCCESS if an instance was created, or a negative error code.
 */
GG_Result GG_Ipv4FrameSerializer_Create(const GG_Ipv4FrameSerializationIpConfig* ip_config,
                                        GG_Ipv4FrameSerializer**                 serializer);

/**
 * Get the #GG_FrameSerializer interface for this object.
 *
 * @param self Object on this this method is called.
 * @return The #GG_FrameSerializer interface for this object.
 */
GG_FrameSerializer* GG_Ipv4FrameSerializer_AsFrameSerializer(GG_Ipv4FrameSerializer* self);

/**
 * Destroy an instance.
 *
 * @param self Object on which this method is called.
 */
void GG_Ipv4FrameSerializer_Destroy(GG_Ipv4FrameSerializer* self);

/**
 * Compute the IPv4 checksum for a buffer.
 *
 * @param data The data to compute the checksum for.
 * @param data_size Number of bytes of data.
 *
 * @return The checksum value.
 */
uint16_t GG_Ipv4Checksum(const uint8_t* data, size_t data_size);

/**
 * Serialize an IPv4 header
 *
 * @param self The header to serialize.
 * @param buffer The buffer to serialize into. If NULL is passed, no serialization is performed, but the
 * size needed to serialize this header is returned via the buffer_size parameter.
 * @param buffer_size [in,out] Pointer to the size of the buffer (or to a 0 value if buffer is NULL).
 * When the method returns, this value is updated with the number of bytes serialized into the buffer,
 * or the number of bytes needed if the buffer size passed was too small.
 * @param compute_checksum Set to true if the checksum should be computed locally.
 *
 * NOTE: this method does not fully validate that the fields of the packet header are valid, so the serialized
 * data may not be a valid IPv4 packet header if the input structure was not itself valid.
 *
 * @return GG_SUCCESS if the header could be serialized or the buffer was NULL, GG_ERROR_NOT_ENOUGH_SPACE
 * if the buffer is too small, or another negative error code.
 */
GG_Result GG_Ipv4PacketHeader_Serialize(const GG_Ipv4PacketHeader* self,
                                        uint8_t*                   buffer,
                                        size_t*                    buffer_size,
                                        bool                       compute_checksum);

/**
 * Parse an IPv4 header from its serialized form.
 *
 * @param self The header whose fields will be set.
 * @param packet A data buffer containing an IPv4 header.
 * @param packet_size Size of the data buffer.
 *
 * @return GG_SUCCESS if a valid header was found and parsed, or a negative error code.
 */
GG_Result GG_Ipv4PacketHeader_Parse(GG_Ipv4PacketHeader* self, const uint8_t* packet, size_t packet_size);

/**
 * Serialize a UDP header.
 *
 * @param self The UDP header to serialize.
 * @param buffer The buffer to serialize into. This buffer must be able to hold at least
 * GG_UDP_HEADER_SIZE bytes.
 *
 * @return GG_SUCCESS if the header could be serialized, or a negative error code.
 */
GG_Result GG_UdpPacketHeader_Serialize(const GG_UdpPacketHeader* self, uint8_t* buffer);

/**
 * Parse a UDP header from its serialized form.
 *
 * @param self The header whose fields will be set.
 * @param packet A data buffer containing a UDP header.
 * @param packet_size Size of the data buffer.
 *
 * @return GG_SUCCESS if a valid header was found and parsed, or a negative error code.
 */
GG_Result GG_UdpPacketHeader_Parse(GG_UdpPacketHeader* self, const uint8_t* packet, size_t packet_size);

#if defined(__cplusplus)
}
#endif
