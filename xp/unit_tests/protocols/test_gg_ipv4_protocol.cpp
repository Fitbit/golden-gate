// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_ring_buffer.h"
#include "xp/protocols/gg_ipv4_protocol.h"

//----------------------------------------------------------------------
const unsigned int MAX_PACKETS       = 100000;
const size_t MAX_PAYLOAD_SIZE        = 17;
const size_t MAX_PACKET_SIZE         = GG_IPV4_MIN_IP_HEADER_SIZE + MAX_PAYLOAD_SIZE;
const size_t RAW_BUFFER_SIZE         = 1024;
const size_t BUFFER_FILL_THRESHOLD   = 3 * MAX_PACKET_SIZE;

//----------------------------------------------------------------------
static GG_RingBuffer input_buffer;
static uint32_t      input_counter = 0;
static uint8_t       raw_buffer[RAW_BUFFER_SIZE];
static uint8_t       workspace_buffer[MAX_PACKET_SIZE + 1];

//----------------------------------------------------------------------
static uint32_t trivial_rand() {
    static uint16_t lfsr = 0xACE1;
    static uint16_t bit;

    bit  = ((lfsr >> 0) ^ (lfsr >> 2) ^ (lfsr >> 3) ^ (lfsr >> 5) ) & 1;
    lfsr = (uint16_t)((lfsr >> 1) | (bit << 15));

    return lfsr;
}

static void
make_packet(uint16_t size, uint8_t* buffer) {
    buffer[0] = (4 << 4); // protocol == 4
    GG_BytesFromInt16Be(buffer+2, size);
    if (size > GG_IPV4_MIN_IP_HEADER_SIZE) {
        uint8_t* payload = &buffer[GG_IPV4_MIN_IP_HEADER_SIZE];

        // store the expected packet size in the first byte of the payload
        payload[0] = (uint8_t)size;

        // store the counter in the next 4 bytes if we can
        if (size >= GG_IPV4_MIN_IP_HEADER_SIZE + 5) {
            GG_BytesFromInt32Be(payload + 1, input_counter);
        }
    }
}

static void
fill_buffer() {
    if (GG_RingBuffer_GetSpace(&input_buffer) < BUFFER_FILL_THRESHOLD) {
        return;
    }
    while (input_counter < MAX_PACKETS && GG_RingBuffer_GetSpace(&input_buffer) >= MAX_PACKET_SIZE + 1) {
        // pick the next packet size
        size_t packet_size = GG_IPV4_MIN_IP_HEADER_SIZE + (trivial_rand() % (1 + MAX_PAYLOAD_SIZE));
        packet_size =  GG_MIN(packet_size, GG_RingBuffer_GetSpace(&input_buffer));

        // 1 in 10 packet will be made too large on purpose
        if (trivial_rand() % 10 == 1) {
            packet_size = MAX_PACKET_SIZE + 1;
        }

        // create the next packet in the workspace
        make_packet((uint16_t)packet_size, workspace_buffer);

        // only count packets that won't be skipped
        if (packet_size <= MAX_PACKET_SIZE) {
            ++input_counter;
        }

        // store the workspace in the input buffer
        GG_RingBuffer_Write(&input_buffer, workspace_buffer, packet_size);
    }
}

static void
check_frame(GG_Buffer* frame) {
    static uint32_t expected_counter = 0;
    size_t data_size = GG_Buffer_GetDataSize(frame);

    CHECK_TRUE(data_size >= GG_IPV4_MIN_IP_HEADER_SIZE);
    if (data_size > GG_IPV4_MIN_IP_HEADER_SIZE) {
        const uint8_t* payload = GG_Buffer_GetData(frame) + GG_IPV4_MIN_IP_HEADER_SIZE;
        CHECK_EQUAL(data_size, payload[0]);
        if (data_size >= GG_IPV4_MIN_IP_HEADER_SIZE + 5) {
            uint32_t counter = GG_BytesToInt32Be(payload + 1);
            CHECK_EQUAL(expected_counter, counter);
        }
    }
    ++expected_counter;
}

//----------------------------------------------------------------------
TEST_GROUP(GG_IPV4_PROTOCOL)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

TEST(GG_IPV4_PROTOCOL, Test_Ipv4FrameAssembler_1) {
    GG_Ipv4FrameAssembler* ipv4_frame_assembler;
    GG_FrameAssembler* frame_assembler;

    GG_Result result = GG_Ipv4FrameAssembler_Create(1, NULL, NULL, &ipv4_frame_assembler);
    CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

    result = GG_Ipv4FrameAssembler_Create(25, NULL, NULL, &ipv4_frame_assembler);
    CHECK_EQUAL(GG_SUCCESS, result);
    frame_assembler = GG_Ipv4FrameAssembler_AsFrameAssembler(ipv4_frame_assembler);

    uint8_t* feed_buffer = NULL;
    size_t   feed_buffer_size;

    GG_FrameAssembler_GetFeedBuffer(frame_assembler, &feed_buffer, &feed_buffer_size);
    CHECK(feed_buffer != NULL);
    CHECK_FALSE(feed_buffer_size > 25);
}

TEST(GG_IPV4_PROTOCOL, Test_Ipv4FrameAssembler_2) {
    GG_RingBuffer_Init(&input_buffer, raw_buffer, sizeof(raw_buffer));

    GG_Ipv4FrameAssembler* ipv4_frame_assembler;
    GG_Result result = GG_Ipv4FrameAssembler_Create(MAX_PACKET_SIZE, NULL, NULL, &ipv4_frame_assembler);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_FrameAssembler* frame_assembler = GG_Ipv4FrameAssembler_AsFrameAssembler(ipv4_frame_assembler);

    while (input_counter < MAX_PACKETS) {
        // ensure that we have something to feed
        fill_buffer();

        // ask the assembler where to feed
        uint8_t* feed_buffer;
        size_t   feed_buffer_size;
        GG_FrameAssembler_GetFeedBuffer(frame_assembler, &feed_buffer, &feed_buffer_size);
        CHECK_FALSE(feed_buffer == NULL);
        CHECK_FALSE(feed_buffer_size == 0);

        // feed if we can
        if (feed_buffer_size) {
            GG_Buffer* frame = NULL;
            size_t     data_size;

            // feed a random amount up to the max
            data_size = trivial_rand() % (1 + feed_buffer_size);
            if (data_size) {
                // copy some bytes from the input buffer
                size_t bytes_peeked = GG_RingBuffer_Peek(&input_buffer, feed_buffer, 0, data_size);
                CHECK_EQUAL(data_size, bytes_peeked);
                result = GG_FrameAssembler_Feed(frame_assembler, &data_size, &frame);
                CHECK_EQUAL(GG_SUCCESS, result);
                CHECK(data_size <= bytes_peeked);

                // advance the cursor by the amount we could feed
                GG_RingBuffer_MoveOut(&input_buffer, data_size);

                // check if we got a frame
                if (frame) {
                    check_frame(frame);
                    GG_Buffer_Release(frame);
                    frame = NULL;
                }
            }
        }
    }
}

TEST(GG_IPV4_PROTOCOL, Test_Ipv4Checksum) {
    uint8_t packet1[20] = {
        0x45, 0x00, 0x00, 0x22, 0x1b, 0xee, 0x00, 0x00, 0x40, 0x11, 0x00, 0x00, 0x0a, 0x01, 0x02, 0x03,
        0x0a, 0x01, 0x02, 0x04
    };
    uint16_t checksum = GG_Ipv4Checksum(packet1, sizeof(packet1));
    LONGS_EQUAL(0xb92a, checksum);

    uint8_t packet2[22] = {
        0x45, 0x00, 0x00, 0x22, 0x1b, 0xee, 0x00, 0x00, 0x40, 0x11, 0x00, 0x00, 0x0a, 0x01, 0x02, 0x03,
        0x0a, 0x01, 0x02, 0x04, 0xFF, 0xFE
    };
    checksum = GG_Ipv4Checksum(packet2, sizeof(packet2));
    LONGS_EQUAL(0xb929, checksum);
}

TEST(GG_IPV4_PROTOCOL, Test_Ipv4Header) {
    GG_Ipv4PacketHeader header;
    header.version = 4;
    header.ihl = 0;
    header.dscp = 0;
    header.ecn = 0;
    header.total_length = 1234;
    header.identification = 0x1234;
    header.flags = 2;
    header.ttl = 23;
    header.fragment_offset = 456;
    header.protocol = GG_IPV4_PROTOCOL_UDP;
    header.src_address = 0x11223344;
    header.dst_address = 0x55667788;

    uint8_t buffer[24];
    size_t buffer_size = 0;

    // IHL too small
    GG_Result result = GG_Ipv4PacketHeader_Serialize(&header, NULL, &buffer_size, false);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result)

    // IHL too large
    header.ihl = 20;
    result = GG_Ipv4PacketHeader_Serialize(&header, NULL, &buffer_size, false);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result)

    // IHL in range
    header.ihl = 6;
    result = GG_Ipv4PacketHeader_Serialize(&header, NULL, &buffer_size, false);
    LONGS_EQUAL(GG_SUCCESS, result)

    buffer_size = 23; // too small
    result = GG_Ipv4PacketHeader_Serialize(&header, buffer, &buffer_size, false);
    LONGS_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    LONGS_EQUAL(24, buffer_size);

    buffer_size = 24; // right size
    result = GG_Ipv4PacketHeader_Serialize(&header, buffer, &buffer_size, false);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(24, buffer_size);

    GG_Ipv4PacketHeader header2;
    result = GG_Ipv4PacketHeader_Parse(&header2, buffer, buffer_size);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(header.version, header2.version);
    LONGS_EQUAL(header.ihl, header2.ihl);
    LONGS_EQUAL(header.dscp, header2.dscp);
    LONGS_EQUAL(header.ecn, header2.ecn);
    LONGS_EQUAL(header.total_length, header2.total_length);
    LONGS_EQUAL(header.identification, header2.identification);
    LONGS_EQUAL(header.flags, header2.flags);
    LONGS_EQUAL(header.ttl, header2.ttl);
    LONGS_EQUAL(header.fragment_offset, header2.fragment_offset);
    LONGS_EQUAL(header.protocol, header2.protocol);
    LONGS_EQUAL(header.src_address, header2.src_address);
    LONGS_EQUAL(header.dst_address, header2.dst_address);
    MEMCMP_EQUAL(header.options, header2.options, 4);

    // packet that's too short
    result = GG_Ipv4PacketHeader_Parse(&header2, buffer, 3);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result)

    // packet with an invalid version
    buffer[0] = 0x15;
    result = GG_Ipv4PacketHeader_Parse(&header2, buffer, buffer_size);
    LONGS_EQUAL(GG_ERROR_INVALID_FORMAT, result)

    // invalid IHL
    buffer[0] = 0x41;
    result = GG_Ipv4PacketHeader_Parse(&header2, buffer, 20);
    LONGS_EQUAL(GG_ERROR_INVALID_FORMAT, result)

    // invalid IHL
    buffer[0] = 0x46;
    result = GG_Ipv4PacketHeader_Parse(&header2, buffer, 20);
    LONGS_EQUAL(GG_ERROR_INVALID_FORMAT, result)
}

TEST(GG_IPV4_PROTOCOL, Test_UdpHeader) {
    GG_UdpPacketHeader header;
    header.checksum = 0xabcd;
    header.src_port = 0x1234;
    header.dst_port = 0x5678;
    header.length   = 0x1000;

    uint8_t buffer[8];

    GG_Result result = GG_UdpPacketHeader_Serialize(&header, buffer);
    LONGS_EQUAL(GG_SUCCESS, result)

    // too short
    result = GG_UdpPacketHeader_Parse(&header, buffer, 7);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result)

    // right size
    result = GG_UdpPacketHeader_Parse(&header, buffer, 8);
    LONGS_EQUAL(GG_SUCCESS, result)

    LONGS_EQUAL(header.checksum, 0xabcd)
    LONGS_EQUAL(header.src_port, 0x1234)
    LONGS_EQUAL(header.dst_port, 0x5678)
    LONGS_EQUAL(header.length, 0x1000)
}

TEST(GG_IPV4_PROTOCOL, Test_HeaderCompression) {
    GG_Ipv4FrameSerializationIpConfig ip_config = {
        .default_src_address = 0x01020304,
        .default_dst_address = 0x04050607,
        .udp_src_ports = { 1007, 1008, 1009 },
        .udp_dst_ports = { 2001, 2002, 2003 }
    };
    GG_Ipv4FrameSerializer* serializer;
    GG_Result result = GG_Ipv4FrameSerializer_Create(&ip_config, &serializer);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_Ipv4FrameAssembler* assembler;
    result = GG_Ipv4FrameAssembler_Create(1280, &ip_config, NULL, &assembler);
    LONGS_EQUAL(GG_SUCCESS, result);

    uint8_t packet[1000];
    uint8_t serialized_buffer[1000];
    GG_RingBuffer serialized;
    uint8_t protocols[4] = {
        GG_IPV4_PROTOCOL_TCP,
        GG_IPV4_PROTOCOL_UDP,
        GG_IPV4_PROTOCOL_ICMP,
        99
    };
    for (unsigned int i = 0; i < 100000; i ++) {
        GG_RingBuffer_Init(&serialized, serialized_buffer, sizeof(serialized_buffer));

        // make a packet
        size_t payload_size = trivial_rand() % 300;
        GG_Ipv4PacketHeader ip_header = { 0 };
        ip_header.version = 4;
        ip_header.ihl = 5 + (trivial_rand() % 11);
        ip_header.dscp = trivial_rand() & 0x3F;
        ip_header.ecn = trivial_rand() & 0x03;
        ip_header.identification = trivial_rand() & 0xFFFF;
        ip_header.flags = trivial_rand() & 0x07;
        ip_header.fragment_offset = 100 * (trivial_rand() % 4);
        ip_header.ttl = 30 * (trivial_rand() % 3);
        ip_header.protocol = protocols[trivial_rand() % 4];
        ip_header.checksum = 0;
        ip_header.src_address = (trivial_rand() % 2) ? 0x11111111 : ip_config.default_src_address;
        ip_header.dst_address = (trivial_rand() % 2) ? 0x22222222 : ip_config.default_dst_address;
        size_t option_size = 4 * (ip_header.ihl - 5);
        for (unsigned int j = 0; j < option_size; j++) {
            ip_header.options[j] = trivial_rand() & 0xFF;
        }
        if (ip_header.protocol == GG_IPV4_PROTOCOL_UDP) {
            ip_header.total_length = (uint16_t)((4 * ip_header.ihl) + GG_UDP_HEADER_SIZE + payload_size);
        } else {
            ip_header.total_length = (uint16_t)((4 * ip_header.ihl) + payload_size);
        }

        GG_UdpPacketHeader udp_header = { 0 };
        unsigned int pick = trivial_rand() % 4;
        udp_header.src_port = pick < 3 ? ip_config.udp_src_ports[pick] : (trivial_rand() % 0xFFFF);
        udp_header.dst_port = pick < 3 ? ip_config.udp_dst_ports[pick] : (trivial_rand() % 0xFFFF);

        // serialize the packet headers
        size_t ip_header_size = sizeof(packet);
        result = GG_Ipv4PacketHeader_Serialize(&ip_header, packet, &ip_header_size, true);
        LONGS_EQUAL(GG_SUCCESS, result);
        LONGS_EQUAL(4 * ip_header.ihl, ip_header_size);
        uint8_t* payload = &packet[ip_header_size];
        if (ip_header.protocol == GG_IPV4_PROTOCOL_UDP) {
            GG_UdpPacketHeader_Serialize(&udp_header, &packet[ip_header_size]);
            payload += GG_UDP_HEADER_SIZE;
        }
        for (unsigned int j = 0; j < payload_size; j++) {
            payload[j] = trivial_rand() & 0xFF;
        }

        result = GG_FrameSerializer_SerializeFrame(GG_Ipv4FrameSerializer_AsFrameSerializer(serializer),
                                                   packet,
                                                   ip_header.total_length,
                                                   &serialized);
        LONGS_EQUAL(GG_SUCCESS, result);
        size_t serialized_size = GG_RingBuffer_GetAvailable(&serialized);
        CHECK_TRUE(serialized_size <= (size_t)(ip_header.total_length + 2)); // shouldn't expand by more than 2 bytes

        for (;;) {
            uint8_t* feed_buffer = NULL;
            size_t feed_buffer_size;
            GG_FrameAssembler_GetFeedBuffer(GG_Ipv4FrameAssembler_AsFrameAssembler(assembler),
                                            &feed_buffer,
                                            &feed_buffer_size);
            size_t can_feed = GG_MIN(feed_buffer_size, GG_RingBuffer_GetAvailable(&serialized));
            if (!can_feed) {
                break;
            }
            GG_RingBuffer_Read(&serialized, feed_buffer, can_feed);
            GG_Buffer* frame = NULL;
            size_t feed_size = can_feed;
            result = GG_FrameAssembler_Feed(GG_Ipv4FrameAssembler_AsFrameAssembler(assembler), &feed_size, &frame);
            LONGS_EQUAL(GG_SUCCESS, result);
            if (frame) {
                // compare that we get the exact same packet
                size_t frame_size = GG_Buffer_GetDataSize(frame);
                const uint8_t* frame_data = GG_Buffer_GetData(frame);
                LONGS_EQUAL(ip_header.total_length, frame_size);
                MEMCMP_EQUAL(packet, frame_data, frame_size);

                GG_Buffer_Release(frame);
                frame = NULL;
                break;
            }
        }
    }

    GG_Ipv4FrameAssembler_Destroy(assembler);
    GG_Ipv4FrameSerializer_Destroy(serializer);
}

TEST(GG_IPV4_PROTOCOL, Test_IpRemapping) {
    GG_Ipv4FrameSerializationIpConfig ip_config = {
        .default_src_address = 0x01020304,
        .default_dst_address = 0x04050607,
        .udp_src_ports = { 1007, 1008, 1009 },
        .udp_dst_ports = { 2001, 2002, 2003 }
    };
    GG_Ipv4FrameAssemnblerIpMap ip_map = {
        .src_address          = 0x01020304,
        .remapped_src_address = 0x02020304,
        .dst_address          = 0x03020304,
        .remapped_dst_address = 0x04020304
    };
    GG_Ipv4FrameSerializer* serializer;
    GG_Result result = GG_Ipv4FrameSerializer_Create(&ip_config, &serializer);
    LONGS_EQUAL(GG_SUCCESS, result)

    GG_Ipv4FrameAssembler* assembler;
    result = GG_Ipv4FrameAssembler_Create(1280, &ip_config, &ip_map, &assembler);
    LONGS_EQUAL(GG_SUCCESS, result)

    GG_Ipv4PacketHeader ip_header;
    ip_header.version = 4;
    ip_header.ihl = 5;
    ip_header.dscp = 0;
    ip_header.ecn = 0;
    ip_header.identification = 0;
    ip_header.flags = 0;
    ip_header.fragment_offset = 0;
    ip_header.ttl = 0;
    ip_header.protocol = GG_IPV4_PROTOCOL_UDP;
    ip_header.checksum = 0;
    ip_header.src_address = 0x01020304;
    ip_header.dst_address = 0x03020304;
    ip_header.total_length = 28;

    GG_UdpPacketHeader udp_header;
    udp_header.src_port = 0x1234;
    udp_header.dst_port = 0x5678;
    udp_header.checksum = 0;
    udp_header.length   = 8;

    uint8_t buffer[20 + 8];
    size_t buffer_size = sizeof(buffer);
    result = GG_Ipv4PacketHeader_Serialize(&ip_header, buffer, &buffer_size, true);
    LONGS_EQUAL(GG_SUCCESS, result)
    result = GG_UdpPacketHeader_Serialize(&udp_header, &buffer[20]);
    LONGS_EQUAL(GG_SUCCESS, result)
    buffer_size = sizeof(buffer);

    GG_Buffer* frame = NULL;
    size_t bytes_left = buffer_size;
    size_t assembler_buffer_size = 0;
    do {
        uint8_t* assembler_buffer = NULL;
        GG_FrameAssembler_GetFeedBuffer(GG_Ipv4FrameAssembler_AsFrameAssembler(assembler),
                                        &assembler_buffer,
                                        &assembler_buffer_size);
        CHECK_TRUE(assembler_buffer_size != 0)
        size_t feed_size = GG_MIN(assembler_buffer_size, bytes_left);
        CHECK_TRUE(bytes_left <= buffer_size)
        memcpy(assembler_buffer, &buffer[buffer_size - bytes_left], feed_size);
        result = GG_FrameAssembler_Feed(GG_Ipv4FrameAssembler_AsFrameAssembler(assembler),
                                        &feed_size,
                                        &frame);
        LONGS_EQUAL(GG_SUCCESS, result)
        CHECK_TRUE(feed_size != 0)
        CHECK_TRUE(feed_size <= bytes_left)
        bytes_left -= feed_size;
    } while (bytes_left);

    CHECK_TRUE(frame != NULL)

    const uint8_t* packet = GG_Buffer_GetData(frame);
    size_t packet_size = GG_Buffer_GetDataSize(frame);
    result = GG_Ipv4PacketHeader_Parse(&ip_header, packet, packet_size);
    LONGS_EQUAL(GG_SUCCESS, result)
    LONGS_EQUAL(ip_map.remapped_src_address, ip_header.src_address)
    LONGS_EQUAL(ip_map.remapped_dst_address, ip_header.dst_address)

    GG_Buffer_Release(frame);
    GG_Ipv4FrameAssembler_Destroy(assembler);
    GG_Ipv4FrameSerializer_Destroy(serializer);
}
