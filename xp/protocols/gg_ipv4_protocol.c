/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-11-21
 *
 * @details
 * Misc protocol parsers and helpers
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/annotations/gg_annotations.h"
#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_threads.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_protocols.h"
#include "gg_ipv4_protocol.h"

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.protocol.ipv4")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_IPV4_MIN_PARTIAL_HEADER_SIZE  4  // enough to get the packet size field
#define GG_IPV4_BASE_HEADER_SIZE         20 // with 0 options
#define GG_IPV4_HEADER_MIN_IHL           5  // min value for the header IHL field
#define GG_IPV4_HEADER_MAX_IHL           15 // max value for the header IHL field

#define GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE           6    // flags and two fixed-size fields
#define GG_IPV4_HEADER_COMPRESSION_MAX_OVERHEAD         2    // maximum added size in the worst case
#define GG_IPV4_HEADER_COMPRESSION_PACKET_IS_COMPRESSED 0x80 // and with fist byte of packet

#define GG_IPV4_HEADER_COMPRESSION_HAS_IHL              0x0001
#define GG_IPV4_HEADER_COMPRESSION_HAS_DSCP             0x0002
#define GG_IPV4_HEADER_COMPRESSION_HAS_ECN              0x0004
#define GG_IPV4_HEADER_COMPRESSION_HAS_FLAGS            0x0008
#define GG_IPV4_HEADER_COMPRESSION_HAS_FRAGMENT_OFFSET  0x0010
#define GG_IPV4_HEADER_COMPRESSION_HAS_TTL              0x0020
#define GG_IPV4_HEADER_COMPRESSION_PROTOCOL_MASK        0x00C0
#define GG_IPV4_HEADER_COMPRESSION_PROTOCOL_TCP         0x0000
#define GG_IPV4_HEADER_COMPRESSION_PROTOCOL_UDP         0x0040
#define GG_IPV4_HEADER_COMPRESSION_PROTOCOL_ICMP        0x0080
#define GG_IPV4_HEADER_COMPRESSION_HAS_PROTOCOL         0x00C0
#define GG_IPV4_HEADER_COMPRESSION_HAS_SRC_ADDRESS      0x0100
#define GG_IPV4_HEADER_COMPRESSION_HAS_DST_ADDRESS      0x0200

#define GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_MASK    0x0C00
#define GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_A       0x0000
#define GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_B       0x0400
#define GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_C       0x0800
#define GG_IPV4_HEADER_COMPRESSION_UDP_HAS_SRC_PORT     0x0C00
#define GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_MASK    0x3000
#define GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_A       0x0000
#define GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_B       0x1000
#define GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_C       0x2000
#define GG_IPV4_HEADER_COMPRESSION_UDP_HAS_DST_PORT     0x3000
#define GG_IPV4_HEADER_COMPRESSION_UDP_HAS_LENGTH       0x4000

#define GG_IPV4_HEADER_COMPRESSION_DEFAULT_IHL              5
#define GG_IPV4_HEADER_COMPRESSION_DEFAULT_DSCP             0
#define GG_IPV4_HEADER_COMPRESSION_DEFAULT_ECN              0
#define GG_IPV4_HEADER_COMPRESSION_DEFAULT_FLAGS            0
#define GG_IPV4_HEADER_COMPRESSION_DEFAULT_FRAGMENT_OFFSET  0
#define GG_IPV4_HEADER_COMPRESSION_DEFAULT_TTL              0

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_Ipv4FrameAssembler {
    GG_IMPLEMENTS(GG_FrameAssembler);
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)
    GG_THREAD_GUARD_ENABLE_BINDING

    bool                              enable_decompression;
    GG_Ipv4FrameSerializationIpConfig ip_config;
    bool                              enable_remapping;
    GG_Ipv4FrameAssemnblerIpMap       ip_map;
    size_t                            skip;
    size_t                            payload_size;
    size_t                            packet_size;
    size_t                            buffer_size;
    uint8_t                           buffer[]; // more buffer bytes follow
};

struct GG_Ipv4FrameSerializer {
    GG_IMPLEMENTS(GG_FrameSerializer);

    bool                              enable_compression;
    GG_Ipv4FrameSerializationIpConfig ip_config;
    uint8_t                           workspace[GG_IPV4_MAX_IP_HEADER_SIZE +
                                                GG_UDP_HEADER_SIZE +
                                                GG_IPV4_HEADER_COMPRESSION_MAX_OVERHEAD];
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
uint16_t
GG_Ipv4Checksum(const uint8_t* data, size_t data_size)
{
    uint32_t checksum = 0;

    // process all pairs of bytes
    while (data_size > 1) {
        uint16_t in = (uint16_t)((*data++) << 8);
        in |= (*data++);
        checksum += in;
        data_size -= 2;
    }

    // process any single byte leftover
    if (data_size) {
        checksum += *data << 8;
    }

    // add deferred carry bits
    checksum = (checksum >> 16) + (checksum & 0x0000FFFF);
    if (checksum & 0xFFFF0000) {
        checksum = (checksum >> 16) + (checksum & 0x0000FFFF);
    }

    return (uint16_t)checksum;
}

//----------------------------------------------------------------------
GG_Result
GG_Ipv4PacketHeader_Parse(GG_Ipv4PacketHeader* self, const uint8_t* packet, size_t packet_size)
{
    // basic check that we have at least a base header (without options)
    if (packet_size < GG_IPV4_BASE_HEADER_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    self->version = packet[0] >> 4;
    if (self->version != 4) {
        return GG_ERROR_INVALID_FORMAT;
    }
    self->ihl = packet[0] & 0x0F;
    if (self->ihl < GG_IPV4_HEADER_MIN_IHL || (self->ihl * 4) > packet_size) {
        return GG_ERROR_INVALID_FORMAT;
    }
    self->dscp = packet[1] >> 2;
    self->ecn  = packet[1] & 0x03;
    self->total_length    = (uint16_t)((packet[2] << 8) | packet[3]);
    self->identification  = (uint16_t)((packet[4] << 8) | packet[5]);
    self->flags           = packet[6] >> 5;
    self->fragment_offset = (uint16_t)(((packet[6] & 0x1F) << 8) | packet[7]);
    self->ttl             = packet[8];
    self->protocol        = packet[9];
    self->checksum        = (uint16_t)((packet[10] << 8) | packet[11]);
    self->src_address     = GG_BytesToInt32Be(&packet[12]);
    self->dst_address     = GG_BytesToInt32Be(&packet[16]);
    memcpy(self->options, &packet[20], (self->ihl - 5) * 4);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_Ipv4PacketHeader_Serialize(const GG_Ipv4PacketHeader* self,
                              uint8_t*                   buffer,
                              size_t*                    buffer_size,
                              bool                       compute_checksum)
{
    GG_ASSERT(self);
    GG_ASSERT(buffer_size);

    // basic sanity check
    if (self->ihl < GG_IPV4_HEADER_MIN_IHL || self->ihl > GG_IPV4_HEADER_MAX_IHL) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // check that the buffer is large enough and/or return the required buffer size
    size_t serialized_size = 4 * self->ihl;
    if (!buffer || (*buffer_size < serialized_size)) {
        *buffer_size = serialized_size;
        return buffer ? GG_ERROR_NOT_ENOUGH_SPACE : GG_SUCCESS;
    }
    *buffer_size = serialized_size;

    // serialize all fields
    buffer[ 0] = (uint8_t)((self->version << 4) | self->ihl);
    buffer[ 1] = (uint8_t)((self->dscp << 2) | (self->ecn & 0x03));
    buffer[ 2] = (uint8_t)(self->total_length >> 8);
    buffer[ 3] = (uint8_t)(self->total_length);
    buffer[ 4] = (uint8_t)(self->identification >> 8);
    buffer[ 5] = (uint8_t)(self->identification);
    buffer[ 6] = (uint8_t)((self->flags << 5) | ((self->fragment_offset >> 8) & 0x1F));
    buffer[ 7] = (uint8_t)(self->fragment_offset);
    buffer[ 8] = self->ttl;
    buffer[ 9] = self->protocol;
    GG_BytesFromInt32Be(&buffer[12],  self->src_address);
    GG_BytesFromInt32Be(&buffer[16], self->dst_address);
    if (self->ihl > 5) {
        memcpy(&buffer[20], self->options, 4 * (self->ihl - 5));
    }

    // special processing for the checksum
    if (compute_checksum) {
        buffer[10] = 0;
        buffer[11] = 0;
        uint16_t checksum = ~GG_Ipv4Checksum(buffer, 4 * self->ihl);
        buffer[10] = (uint8_t)(checksum >> 8);
        buffer[11] = (uint8_t)(checksum);
    } else {
        buffer[10] = (uint8_t)(self->checksum >> 8);
        buffer[11] = (uint8_t)(self->checksum);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_UdpPacketHeader_Parse(GG_UdpPacketHeader* self, const uint8_t* packet, size_t packet_size)
{
    // sanity check
    if (packet_size < GG_UDP_HEADER_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    self->src_port = (uint16_t)((packet[0] << 8) | packet[1]);
    self->dst_port = (uint16_t)((packet[2] << 8) | packet[3]);
    self->length   = (uint16_t)((packet[4] << 8) | packet[5]);
    self->checksum = (uint16_t)((packet[6] << 8) | packet[7]);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_UdpPacketHeader_Serialize(const GG_UdpPacketHeader* self, uint8_t* buffer)
{
    GG_ASSERT(self);

    // serialize all fields
    buffer[0] = (uint8_t)(self->src_port >> 8);
    buffer[1] = (uint8_t)(self->src_port);
    buffer[2] = (uint8_t)(self->dst_port >> 8);
    buffer[3] = (uint8_t)(self->dst_port);
    buffer[4] = (uint8_t)(self->length >> 8);
    buffer[5] = (uint8_t)(self->length);
    buffer[6] = (uint8_t)(self->checksum >> 8);
    buffer[7] = (uint8_t)(self->checksum);

    return GG_SUCCESS;
}

/**
 * Compress an IP header and optional UDP header into a buffer.
 *
 * IPv4 Header:
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |Version|  IHL  |Type of Service|          Total Length         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |         Identification        |Flags|      Fragment Offset    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |  Time to Live |    Protocol   |         Header Checksum       |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                       Source Address                          |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Destination Address                        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |                    Options                    |    Padding    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * UDP Header:
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |          Source Port          |       Destination Port        |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |            Length             |           Checksum            |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |   .... data ....                                              |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * The compression algorithm and format works like this:
 *
 * Compressed Header:
 * 0                   1                   2                   3
 * 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |        Elision Flags          |          Total Length         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |     ... variable number of fields, depending on elision  ...
 *
 * For each field in the IPv4 header and UDP header (if the packet is a UDP packet), if
 * the field matches a corresponding field in the supplied GG_Ipv4FrameSerializationIpConfig
 * struct, the field is elided. For other fields not specified in the configuration,
 * the compressor compares against fixed default values in order to decide whether to
 * elide or not: the most common values for each field are elided.
 * The first 16 bits of the compressed header are a bit mask that indicates which fields
 * have been elided (i.e not serialized) and which fields are serialized. The most significant
 * bit of the flags is 1, so that a parser can differentiate a compressed packet from
 * a non-compressed packet (where the most significant 4 bits are 0100 = 4).
 * The following 16 bits are the Total Length field, just like in an uncompressed IPv4 header.
 * This allows a frame serializer to obtain the first 4 bytes of any packet, whether compressed
 * or not, and be able to know the total size of the packet.
 * Finally, all the non-elided fields follow, including up to 7 bits of padding to make
 * the header a multiple of 8 bits.
 */
static void
GG_Ipv4_CompressHeaders(const GG_Ipv4PacketHeader*               ip_header,
                        const GG_UdpPacketHeader*                udp_header,
                        const GG_Ipv4FrameSerializationIpConfig* ip_config,
                        uint8_t*                                 buffer,
                        size_t*                                  buffer_size)
{
    GG_ASSERT(ip_header);
    GG_ASSERT(buffer_size);
    GG_ASSERT(*buffer_size >= GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE);
    GG_ASSERT(ip_header->ihl >= GG_IPV4_HEADER_MIN_IHL);

    // init a bitstream object to write the variable part into the buffer
    GG_BitOutputStream bits;
    GG_BitOutputStream_Init(&bits,
                            buffer + GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE,
                            *buffer_size - GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE);

    // for each field, if it has the default value, leave the corresponding flag unset, else
    // set the flag and serialize the field
    uint16_t flags = GG_IPV4_HEADER_COMPRESSION_PACKET_IS_COMPRESSED << 8;
    if (ip_header->ihl != GG_IPV4_HEADER_COMPRESSION_DEFAULT_IHL) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_IHL;
        GG_BitOutputStream_Write(&bits, ip_header->ihl, 4);
    }
    if (ip_header->dscp != GG_IPV4_HEADER_COMPRESSION_DEFAULT_DSCP) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_DSCP;
        GG_BitOutputStream_Write(&bits, ip_header->dscp, 6);
    }
    if (ip_header->ecn != GG_IPV4_HEADER_COMPRESSION_DEFAULT_ECN) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_ECN;
        GG_BitOutputStream_Write(&bits, ip_header->ecn, 2);
    }
    if (ip_header->flags != GG_IPV4_HEADER_COMPRESSION_DEFAULT_FLAGS) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_FLAGS;
        GG_BitOutputStream_Write(&bits, ip_header->flags, 3);
    }
    if (ip_header->fragment_offset != GG_IPV4_HEADER_COMPRESSION_DEFAULT_FRAGMENT_OFFSET) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_FRAGMENT_OFFSET;
        GG_BitOutputStream_Write(&bits, ip_header->fragment_offset, 13);
    }
    if (ip_header->ttl != GG_IPV4_HEADER_COMPRESSION_DEFAULT_TTL) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_TTL;
        GG_BitOutputStream_Write(&bits, ip_header->ttl, 8);
    }
    if (ip_header->protocol == GG_IPV4_PROTOCOL_TCP) {
        flags |= GG_IPV4_HEADER_COMPRESSION_PROTOCOL_TCP;
    } else if (ip_header->protocol == GG_IPV4_PROTOCOL_UDP) {
        flags |= GG_IPV4_HEADER_COMPRESSION_PROTOCOL_UDP;
    } else if (ip_header->protocol == GG_IPV4_PROTOCOL_ICMP) {
        flags |= GG_IPV4_HEADER_COMPRESSION_PROTOCOL_ICMP;
    } else {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_PROTOCOL;
        GG_BitOutputStream_Write(&bits, ip_header->protocol, 8);
    }
    if (ip_header->src_address != ip_config->default_src_address) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_SRC_ADDRESS;
        GG_BitOutputStream_Write(&bits, ip_header->src_address, 32);
    }
    if (ip_header->dst_address != ip_config->default_dst_address) {
        flags |= GG_IPV4_HEADER_COMPRESSION_HAS_DST_ADDRESS;
        GG_BitOutputStream_Write(&bits, ip_header->dst_address, 32);
    }

    // options
    size_t options_size = (4 * (ip_header->ihl - GG_IPV4_HEADER_MIN_IHL));
    if (options_size) {
        for (unsigned int i = 0; i < options_size; i++) {
            GG_BitOutputStream_Write(&bits, ip_header->options[i], 8);
        }
    }

    // UDP
    if (udp_header) {
        if (udp_header->src_port == ip_config->udp_src_ports[0]) {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_A;
        } else if (udp_header->src_port == ip_config->udp_src_ports[1]) {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_B;
        } else if (udp_header->src_port == ip_config->udp_src_ports[2]) {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_C;
        } else {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_HAS_SRC_PORT;
            GG_BitOutputStream_Write(&bits, udp_header->src_port, 16);
        }
        if (udp_header->dst_port == ip_config->udp_dst_ports[0]) {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_A;
        } else if (udp_header->dst_port == ip_config->udp_dst_ports[1]) {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_B;
        } else if (udp_header->dst_port == ip_config->udp_dst_ports[2]) {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_C;
        } else {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_HAS_DST_PORT;
            GG_BitOutputStream_Write(&bits, udp_header->dst_port, 16);
        }
        if ((4 * ip_header->ihl) + udp_header->length != ip_header->total_length) {
            flags |= GG_IPV4_HEADER_COMPRESSION_UDP_HAS_LENGTH;
            GG_BitOutputStream_Write(&bits, udp_header->length, 16);
        }
    }

    // compute header and payload sizes
    size_t header_size = 4 * ip_header->ihl;
    if (ip_header->protocol == GG_IPV4_PROTOCOL_UDP) {
        header_size += GG_UDP_HEADER_SIZE;
    }
    GG_ASSERT(ip_header->total_length >= header_size);
    size_t payload_size = ip_header->total_length - header_size;
    size_t compressed_headers_size = GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE +
                                     (GG_BitOutputStream_GetPosition(&bits) + 7) / 8;
    size_t total_length = compressed_headers_size + payload_size;

    // output the fixed part
    buffer[0] = flags >> 8;
    buffer[1] = flags & 0xFF;
    buffer[2] = (uint8_t)(total_length >> 8);
    buffer[3] = (uint8_t)(total_length & 0xFF);
    buffer[4] = ip_header->identification >> 8;
    buffer[5] = ip_header->identification & 0xFF;

    // ensure all bits are written to the buffer
    GG_BitOutputStream_Flush(&bits);

    // update the buffer size
    *buffer_size = compressed_headers_size;
}

//----------------------------------------------------------------------
// Decompress an IP header and optional UDP header from a packet buffer.
//----------------------------------------------------------------------
static GG_Result
GG_Ipv4_DecompressHeaders(const uint8_t*                           data,
                          size_t                                   data_size,
                          const GG_Ipv4FrameSerializationIpConfig* ip_config,
                          GG_Ipv4PacketHeader*                     ip_header,
                          GG_UdpPacketHeader*                      udp_header,
                          size_t*                                  compressed_header_size)
{
    GG_ASSERT(ip_header);
    GG_ASSERT(udp_header);
    GG_ASSERT(compressed_header_size);
    GG_ASSERT(data_size >= GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE);

    // parse the fixed part (skip the total length field here since it represents the compressed size)
    uint16_t flags            = (uint16_t)((data[0] << 8) | data[1]);
    ip_header->identification = (uint16_t)((data[4] << 8) | data[5]);

    // set the checksum to 0, the caller will have to compute it when serializing
    ip_header->checksum = 0;

    // the IP version is implicit
    ip_header->version = 4;

    // setup a bit stream to read the variable part
    GG_BitInputStream bits;
    GG_BitInputStream_Init(&bits,
                           data + GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE,
                           data_size - GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE);

    // parse the variable part based on the flags in the fixed part
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_IHL) {
        ip_header->ihl = (uint8_t)GG_BitInputStream_Read(&bits, 4);
    } else {
        ip_header->ihl = GG_IPV4_HEADER_COMPRESSION_DEFAULT_IHL;
    }
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_DSCP) {
        ip_header->dscp = (uint8_t)GG_BitInputStream_Read(&bits, 6);
    } else {
        ip_header->dscp = GG_IPV4_HEADER_COMPRESSION_DEFAULT_DSCP;
    }
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_ECN) {
        ip_header->ecn = (uint8_t)GG_BitInputStream_Read(&bits, 2);
    } else {
        ip_header->ecn = GG_IPV4_HEADER_COMPRESSION_DEFAULT_ECN;
    }
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_FLAGS) {
        ip_header->flags = (uint8_t)GG_BitInputStream_Read(&bits, 3);
    } else {
        ip_header->flags = GG_IPV4_HEADER_COMPRESSION_DEFAULT_FLAGS;
    }
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_FRAGMENT_OFFSET) {
        ip_header->fragment_offset = (uint16_t)GG_BitInputStream_Read(&bits, 13);
    } else {
        ip_header->fragment_offset = GG_IPV4_HEADER_COMPRESSION_DEFAULT_FRAGMENT_OFFSET;
    }
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_TTL) {
        ip_header->ttl = (uint8_t)GG_BitInputStream_Read(&bits, 8);
    } else {
        ip_header->ttl = GG_IPV4_HEADER_COMPRESSION_DEFAULT_TTL;
    }
    if ((flags & GG_IPV4_HEADER_COMPRESSION_PROTOCOL_MASK) == GG_IPV4_HEADER_COMPRESSION_PROTOCOL_TCP) {
        ip_header->protocol = GG_IPV4_PROTOCOL_TCP;
    } else if ((flags & GG_IPV4_HEADER_COMPRESSION_PROTOCOL_MASK) == GG_IPV4_HEADER_COMPRESSION_PROTOCOL_UDP) {
        ip_header->protocol = GG_IPV4_PROTOCOL_UDP;
    } else if ((flags & GG_IPV4_HEADER_COMPRESSION_PROTOCOL_MASK) == GG_IPV4_HEADER_COMPRESSION_PROTOCOL_ICMP) {
        ip_header->protocol = GG_IPV4_PROTOCOL_ICMP;
    } else {
        ip_header->protocol = (uint8_t)GG_BitInputStream_Read(&bits, 8);
    }
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_SRC_ADDRESS) {
        ip_header->src_address = GG_BitInputStream_Read(&bits, 32);
    } else {
        ip_header->src_address = ip_config->default_src_address;
    }
    if (flags & GG_IPV4_HEADER_COMPRESSION_HAS_DST_ADDRESS) {
        ip_header->dst_address = GG_BitInputStream_Read(&bits, 32);
    } else {
        ip_header->dst_address = ip_config->default_dst_address;
    }

    // sanity check
    size_t header_size = 4 * ip_header->ihl;
    if (header_size < GG_IPV4_MIN_IP_HEADER_SIZE) {
        return GG_ERROR_INVALID_FORMAT;
    }

    // copy the options
    size_t options_size = (4 * (ip_header->ihl - GG_IPV4_HEADER_MIN_IHL));
    if (options_size) {
        for (unsigned int i = 0; i < options_size; i++) {
            ip_header->options[i] = (uint8_t)GG_BitInputStream_Read(&bits, 8);
        }
    }

    // UDP
    if (ip_header->protocol == GG_IPV4_PROTOCOL_UDP) {
        header_size += GG_UDP_HEADER_SIZE;
        if ((flags & GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_MASK) ==
            GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_A) {
            udp_header->src_port = ip_config->udp_src_ports[0];
        } else if ((flags & GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_MASK) ==
                   GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_B) {
            udp_header->src_port = ip_config->udp_src_ports[1];
        } else if ((flags & GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_MASK) ==
                   GG_IPV4_HEADER_COMPRESSION_UDP_SRC_PORT_C) {
            udp_header->src_port = ip_config->udp_src_ports[2];
        } else {
            udp_header->src_port = (uint16_t)GG_BitInputStream_Read(&bits, 16);
        }
        if ((flags & GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_MASK) ==
            GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_A) {
            udp_header->dst_port = ip_config->udp_dst_ports[0];
        } else if ((flags & GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_MASK) ==
                   GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_B) {
            udp_header->dst_port = ip_config->udp_dst_ports[1];
        } else if ((flags & GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_MASK) ==
                   GG_IPV4_HEADER_COMPRESSION_UDP_DST_PORT_C) {
            udp_header->dst_port = ip_config->udp_dst_ports[2];
        } else {
            udp_header->dst_port = (uint16_t)GG_BitInputStream_Read(&bits, 16);
        }
        if (flags & GG_IPV4_HEADER_COMPRESSION_UDP_HAS_LENGTH) {
            udp_header->length = (uint16_t)GG_BitInputStream_Read(&bits, 16);
        } // don't handle the other case here, because we need to know the compressed header size
        udp_header->checksum = 0;
    }

    // compute the compressed header size
    size_t variable_size = (GG_BitInputStream_GetPosition(&bits) + 7) / 8;
    *compressed_header_size = GG_IPV4_HEADER_COMPRESSION_FIXED_SIZE + variable_size;
    if (*compressed_header_size > data_size) {
        return GG_ERROR_INVALID_FORMAT;
    }

    // compute the total length
    size_t payload_size = data_size - *compressed_header_size;
    ip_header->total_length = (uint16_t)(header_size + payload_size);

    // adjust the UDP length if needed
    if (ip_header->protocol == GG_IPV4_PROTOCOL_UDP && !(flags & GG_IPV4_HEADER_COMPRESSION_UDP_HAS_LENGTH)) {
        udp_header->length = (uint16_t)(payload_size - GG_UDP_HEADER_SIZE);
    }

    // done
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_Ipv4FrameAssembler_GetFeedBuffer(GG_FrameAssembler* _self, uint8_t** buffer, size_t* buffer_size)
{
    GG_ASSERT(buffer);
    GG_ASSERT(buffer_size);
    GG_Ipv4FrameAssembler* self = GG_SELF(GG_Ipv4FrameAssembler, GG_FrameAssembler);

    // if we're skipping data, just return the largest buffer we can bear
    if (self->skip) {
        *buffer      = self->buffer;
        *buffer_size = GG_MIN(self->skip, self->buffer_size);
        return;
    } else {
        GG_ASSERT(self->buffer_size >= self->payload_size);
        *buffer = &self->buffer[self->payload_size];
    }

    // if we're still accumulating the header, only accept that much
    if (self->packet_size == 0) {
        *buffer_size = GG_IPV4_MIN_PARTIAL_HEADER_SIZE - self->payload_size;
    } else {
        *buffer_size = self->buffer_size - self->payload_size;
    }
}

//----------------------------------------------------------------------
static GG_Result
GG_Ipv4FrameAssembler_DecompressAndEmitPacket(GG_Ipv4FrameAssembler* self, GG_Buffer** frame)
{
    if (!self->enable_decompression) {
        GG_LOG_WARNING("header decompression not supported");
        *frame = NULL;
        return GG_ERROR_NOT_SUPPORTED;
    }

    // decompress the headers
    GG_Ipv4PacketHeader ip_header;
    GG_UdpPacketHeader udp_header;
    size_t compressed_header_size = 0;
    GG_Result result = GG_Ipv4_DecompressHeaders(self->buffer,
                                                 self->packet_size,
                                                 &self->ip_config,
                                                 &ip_header,
                                                 &udp_header,
                                                 &compressed_header_size);
    if (GG_FAILED(result)) {
        GG_LOG_WARNING("header decompression failed (%d)", result);
        return result;
    }
    size_t decompressed_header_size = ip_header.ihl * 4 + (ip_header.protocol == GG_IPV4_PROTOCOL_UDP ? 8 : 0);
    GG_LOG_FINER("decompressed header: %u -> %u", (int)compressed_header_size, (int)decompressed_header_size);

    // compute the final packet size
    GG_ASSERT(compressed_header_size <= self->packet_size);
    GG_ASSERT(compressed_header_size <= ip_header.total_length);

    // allocate a packet
    GG_DynamicBuffer* packet;
    result = GG_DynamicBuffer_Create(ip_header.total_length, &packet);
    if (GG_FAILED(result)) {
        return result;
    }
    GG_DynamicBuffer_SetDataSize(packet, ip_header.total_length);

    // serialize the headers
    // NOTE: GG_Ipv4_DecompressHeaders guarantees us that ip_header.total_length is correct
    // with respect to the other header size fields
    size_t ip_header_size = 4 * ip_header.ihl;
    size_t buffer_size = ip_header_size;
    uint8_t* output = GG_DynamicBuffer_UseData(packet);
    result = GG_Ipv4PacketHeader_Serialize(&ip_header, output, &buffer_size, true);
    if (GG_FAILED(result)) {
        return result;
    }
    output += ip_header_size;
    if (ip_header.protocol == GG_IPV4_PROTOCOL_UDP) {
        result = GG_UdpPacketHeader_Serialize(&udp_header, output);
        output += GG_UDP_HEADER_SIZE;
    }

    // copy the payload
    if (self->packet_size > compressed_header_size) {
        memcpy(output, self->buffer + compressed_header_size, self->packet_size - compressed_header_size);
    }
    *frame = GG_DynamicBuffer_AsBuffer(packet);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_Ipv4FrameAssembler_CopyAndEmitPacket(GG_Ipv4FrameAssembler* self, GG_Buffer** frame)
{
    // allocate a packet
    GG_DynamicBuffer* packet;
    GG_Result result = GG_DynamicBuffer_Create(self->packet_size, &packet);
    if (GG_SUCCEEDED(result)) {
        // copy the data
        memcpy(GG_DynamicBuffer_UseData(packet), self->buffer, self->packet_size);
        GG_DynamicBuffer_SetDataSize(packet, self->packet_size);
        *frame = GG_DynamicBuffer_AsBuffer(packet);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_Ipv4FrameAssembler_EmitPacket(GG_Ipv4FrameAssembler* self, GG_Buffer** frame)
{
    GG_Result result;
    if (self->buffer[0] & GG_IPV4_HEADER_COMPRESSION_PACKET_IS_COMPRESSED) {
        // this is a compressed packet
        result = GG_Ipv4FrameAssembler_DecompressAndEmitPacket(self, frame);
    } else {
        // this is a normal packet
        result = GG_Ipv4FrameAssembler_CopyAndEmitPacket(self, frame);
    }

    // remap IP addresses if required
    if (GG_SUCCEEDED(result) && self->enable_remapping) {
        uint8_t* packet = GG_Buffer_UseData(*frame);
        uint32_t src_address = GG_BytesToInt32Be(&packet[GG_IPV4_HEADER_SOURCE_ADDRESS_OFFSET]);
        bool recompute_checksum = false;
        if (src_address == self->ip_map.src_address) {
            GG_BytesFromInt32Be(&packet[GG_IPV4_HEADER_SOURCE_ADDRESS_OFFSET],
                                self->ip_map.remapped_src_address);
            recompute_checksum = true;
        }
        uint32_t dst_address = GG_BytesToInt32Be(&packet[GG_IPV4_HEADER_DESTINATION_ADDRESS_OFFSET]);
        if (dst_address == self->ip_map.dst_address) {
            GG_BytesFromInt32Be(&packet[GG_IPV4_HEADER_DESTINATION_ADDRESS_OFFSET],
                                 self->ip_map.remapped_dst_address);
            recompute_checksum = true;
        }

        // if one of the addresses has been remapped, we need to recompute the checksum
        if (recompute_checksum) {
            uint8_t ihl = packet[0] & 0x0F;
            size_t packet_size =  GG_Buffer_GetDataSize(*frame);
            if (ihl >= GG_IPV4_HEADER_MIN_IHL && ihl * 4 <= packet_size) {
                packet[10] = 0;
                packet[11] = 0;
                uint16_t checksum = ~GG_Ipv4Checksum(packet, 4 * ihl);
                packet[10] = (uint8_t)(checksum >> 8);
                packet[11] = (uint8_t)(checksum);

                // zero-out the UDP checksum if this is a UDP packet
                uint8_t protocol = packet[9];
                if (protocol == GG_IPV4_PROTOCOL_UDP &&
                    (size_t)((ihl * 4) + GG_UDP_HEADER_SIZE) <= packet_size) {
                    packet[ihl * 4 + 6] = 0;
                    packet[ihl * 4 + 7] = 0;
                }
            }
        }
    }

    // reset for a new packet
    self->packet_size  = 0;
    self->payload_size = 0;

    return result;
}

//----------------------------------------------------------------------
static GG_Result
GG_Ipv4FrameAssembler_Feed(GG_FrameAssembler* _self, size_t* data_size, GG_Buffer** frame)
{
    GG_ASSERT(data_size);
    GG_ASSERT(frame);
    GG_Ipv4FrameAssembler* self = GG_SELF(GG_Ipv4FrameAssembler, GG_FrameAssembler);

    // default return value
    *frame = NULL;

    // if we're skipping data, consume until we've skipped what we need to skip
    if (self->skip) {
        if (*data_size <= self->skip) {
            self->skip -= *data_size;
        } else {
            *data_size = self->skip;
            self->skip = 0;
        }
        return GG_SUCCESS;
    }

    // if we're still waiting for a header, try to fill it
    size_t consumed = 0;
    if (self->packet_size == 0) {
        // compute how much is needed to complete a minimum header
        GG_ASSERT(self->payload_size < GG_IPV4_MIN_PARTIAL_HEADER_SIZE);
        size_t needed = GG_IPV4_MIN_PARTIAL_HEADER_SIZE - self->payload_size;
        if (needed > *data_size) {
            // there's less than we need, take everything and return
            self->payload_size += *data_size;
            return GG_SUCCESS;
        }

        // consume the amount we need to complete the header
        self->payload_size += needed;
        consumed = needed;

        // header complete, parse the total packet size
        self->packet_size = GG_BytesToInt16Be(&self->buffer[2]);
        GG_LOG_FINEST("got packet header, packet_size=%u", (int)self->packet_size);

        // sanity check
        if (self->packet_size < GG_IPV4_MIN_PARTIAL_HEADER_SIZE) {
            // uh.. what?
            GG_LOG_WARNING("this doesn't look like a valid packet");
            GG_LOG_COMMS_ERROR(GG_LIB_PROTOCOL_PACKET_TOO_SMALL);

            GG_FrameAssembler_Reset(_self);
            *data_size = consumed;
            return GG_ERROR_INVALID_SYNTAX;
        }
        if (self->packet_size > self->buffer_size) {
            GG_LOG_WARNING("packet too large, will skip");
            GG_LOG_COMMS_ERROR(GG_LIB_PROTOCOL_PACKET_TOO_LARGE);

            self->skip = self->packet_size - self->payload_size;
            self->payload_size = 0;
            self->packet_size = 0;
            *data_size = consumed;
            return GG_SUCCESS;
        }
    }

    // consume up to the packet size
    if (consumed < *data_size) {
        size_t will_take = GG_MIN(*data_size - consumed, self->packet_size - self->payload_size);
        self->payload_size += will_take;
        consumed           += will_take;
    }

    // say how much we consumed
    *data_size = consumed;

    // emit a packet if one is complete
    if (self->payload_size == self->packet_size) {
        return GG_Ipv4FrameAssembler_EmitPacket(self, frame);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_Ipv4FrameAssembler_Reset(GG_FrameAssembler* _self)
{
    GG_Ipv4FrameAssembler* self = GG_SELF(GG_Ipv4FrameAssembler, GG_FrameAssembler);

    self->skip         = 0;
    self->payload_size = 0;
    self->packet_size  = 0;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_Ipv4FrameAssembler, GG_FrameAssembler) {
    .GetFeedBuffer = GG_Ipv4FrameAssembler_GetFeedBuffer,
    .Feed          = GG_Ipv4FrameAssembler_Feed,
    .Reset         = GG_Ipv4FrameAssembler_Reset
};

//----------------------------------------------------------------------
#if defined(GG_CONFIG_ENABLE_INSPECTION)
static GG_Result
GG_Ipv4FrameAssembler_Inspect(GG_Inspectable* _self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(options);
    GG_Ipv4FrameAssembler* self = GG_SELF(GG_Ipv4FrameAssembler, GG_Inspectable);

    GG_Inspector_OnBoolean(inspector, "enable_decompression", self->enable_decompression);
    GG_Inspector_OnBoolean(inspector, "enable_remapping",     self->enable_remapping);
    GG_Inspector_OnInteger(inspector, "skip",         self->skip,         GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector, "payload_size", self->payload_size, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector, "packet_size",  self->packet_size,  GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector, "buffer_size",  self->buffer_size,  GG_INSPECTOR_FORMAT_HINT_UNSIGNED);

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(GG_Ipv4FrameAssembler, GG_Inspectable) {
    .Inspect = GG_Ipv4FrameAssembler_Inspect
};
#endif

//----------------------------------------------------------------------
GG_Result
GG_Ipv4FrameAssembler_Create(uint16_t                                 max_packet_size,
                             const GG_Ipv4FrameSerializationIpConfig* ip_config,
                             const GG_Ipv4FrameAssemnblerIpMap*       ip_map,
                             GG_Ipv4FrameAssembler**                  assembler)
{
    if (max_packet_size < GG_IPV4_MIN_IP_HEADER_SIZE) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // allocate a new object, with space for the buffer at the end
    size_t object_size = sizeof(GG_Ipv4FrameAssembler) + max_packet_size;
    *assembler = GG_AllocateZeroMemory(object_size);
    if (*assembler == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // copy the IP config if supplied
    if (ip_config) {
        (*assembler)->enable_decompression = true;
        (*assembler)->ip_config = *ip_config;
    }

    // copy the IP map if supplied
    if (ip_map) {
        (*assembler)->enable_remapping = true;
        (*assembler)->ip_map = *ip_map;
    }

    // setup the object
    (*assembler)->buffer_size = max_packet_size;

    // setup the vtables
    GG_SET_INTERFACE(*assembler, GG_Ipv4FrameAssembler, GG_FrameAssembler);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(*assembler, GG_Ipv4FrameAssembler, GG_Inspectable));

    // bind to the current thread
    GG_THREAD_GUARD_BIND(*assembler);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_Ipv4FrameAssembler_Destroy(GG_Ipv4FrameAssembler* self)
{
    if (self == NULL) return;
    GG_THREAD_GUARD_CHECK_BINDING(self);

    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_FrameAssembler*
GG_Ipv4FrameAssembler_AsFrameAssembler(GG_Ipv4FrameAssembler* self)
{
    return GG_CAST(self, GG_FrameAssembler);
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
GG_Inspectable*
GG_Ipv4FrameAssembler_AsInspectable(GG_Ipv4FrameAssembler* self)
{
    return GG_CAST(self, GG_Inspectable);
}
#endif

//----------------------------------------------------------------------
static GG_Result
GG_Ipv4FrameSerializer_SerializeFrame(GG_FrameSerializer* _self,
                                      const uint8_t*      frame,
                                      size_t              frame_size,
                                      GG_RingBuffer*      output_buffer)
{
    GG_Ipv4FrameSerializer* self = GG_SELF(GG_Ipv4FrameSerializer, GG_FrameSerializer);

    // check the parameters
    if (frame_size >= output_buffer->size) {
        // this would never fit even if the buffer was empty
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // check if we have enough space in the ring buffer
    size_t space_available = GG_RingBuffer_GetSpace(output_buffer);
    GG_LOG_FINE("space available in ring buffer = %u", (int)space_available);
    if (frame_size + 2 > space_available) {
        // we're being conservative here, checking the size even before compression.
        // (we could me more precise and compute the actual size needed with compression,
        // but that would require us to perform at least partial compression first)
        // worst cast, the serialized size is the original packet size plus two extra bytes
        // of flags.
        return GG_ERROR_WOULD_BLOCK;
    }

    // serialize
    if (self->enable_compression) {
        // parse the frame
        GG_Ipv4PacketHeader ip_header;
        GG_Result result = GG_Ipv4PacketHeader_Parse(&ip_header, frame, frame_size);
        if (GG_FAILED(result)) {
            return result;
        }
        size_t ip_header_size = 4 * ip_header.ihl;
        size_t header_size = ip_header_size;
        if (frame_size < ip_header_size) {
            return GG_ERROR_INVALID_FORMAT;
        }
        GG_UdpPacketHeader udp_header;
        if (ip_header.protocol == GG_IPV4_PROTOCOL_UDP) {
            header_size += GG_UDP_HEADER_SIZE;
            if (frame_size < header_size) {
                return GG_ERROR_INVALID_FORMAT;
            }
            result = GG_UdpPacketHeader_Parse(&udp_header, frame + ip_header_size, frame_size - ip_header_size);
            if (GG_FAILED(result)) {
                return result;
            }
        }

        // serialize the headers into a local buffer and copy it to the output ring buffer
        size_t compressed_header_size = sizeof(self->workspace);
        GG_Ipv4_CompressHeaders(&ip_header,
                                ip_header.protocol == GG_IPV4_PROTOCOL_UDP ? &udp_header : NULL,
                                &self->ip_config,
                                self->workspace,
                                &compressed_header_size);
        GG_LOG_FINER("compressed header: %u -> %u", (int)header_size, (int)compressed_header_size);
        GG_RingBuffer_Write(output_buffer, self->workspace, compressed_header_size);

        // copy the payload
        GG_RingBuffer_Write(output_buffer, frame + header_size, frame_size - header_size);
    } else {
        // copy the data as-is into the ring buffer
        GG_RingBuffer_Write(output_buffer, frame, frame_size);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_Ipv4FrameSerializer, GG_FrameSerializer) {
    .SerializeFrame = GG_Ipv4FrameSerializer_SerializeFrame
};

//----------------------------------------------------------------------
GG_Result
GG_Ipv4FrameSerializer_Create(const GG_Ipv4FrameSerializationIpConfig* ip_config,
                              GG_Ipv4FrameSerializer**                 serializer)
{
    *serializer = GG_AllocateZeroMemory(sizeof(GG_Ipv4FrameSerializer));
    if (!*serializer) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // copy the config if supplied
    if (ip_config) {
        (*serializer)->enable_compression = true;
        (*serializer)->ip_config = *ip_config;
    }

    // setup interfaces
    GG_SET_INTERFACE(*serializer, GG_Ipv4FrameSerializer, GG_FrameSerializer);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_FrameSerializer*
GG_Ipv4FrameSerializer_AsFrameSerializer(GG_Ipv4FrameSerializer* self)
{
    return GG_CAST(self, GG_FrameSerializer);
}

//----------------------------------------------------------------------
void
GG_Ipv4FrameSerializer_Destroy(GG_Ipv4FrameSerializer* self)
{
    if (!self) return;

    GG_ClearAndFreeObject(self, 1);
}
