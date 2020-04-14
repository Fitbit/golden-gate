/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_port.h"
#include "xp/utils/gg_memory_data_sink.h"
#include "xp/protocols/gg_ipv4_protocol.h"
#include "xp/loop/gg_loop.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/lwip/gg_lwip_sockets.h"
#include "xp/lwip/gg_lwip_generic_netif.h"

// TEMP: this will eventually go in a shared init function)
extern "C" { void lwip_init(void); }

//----------------------------------------------------------------------
TEST_GROUP(GG_LWIP)
{
    void setup(void) {
        GG_Module_Initialize();
    }

    void teardown(void) {
    }
};

//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
    uint8_t          message[4];
    GG_SocketAddress peer;
} Receiver;

static GG_Result
Receiver_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    Receiver* self = GG_SELF(Receiver, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    // copy the first 4 bytes of the data
    if (GG_Buffer_GetDataSize(data) >= 4) {
        memcpy(self->message, GG_Buffer_GetData(data), 4);
    }

    // check the metadata and copy it
    if (metadata && metadata->type == GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS) {
        const GG_SocketAddressMetadata* socket_metadata = (const GG_SocketAddressMetadata*)metadata;
        self->peer = socket_metadata->socket_address;
    }

    return GG_SUCCESS;
}

static GG_Result
Receiver_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(Receiver, GG_DataSink) {
    Receiver_PutData,
    Receiver_SetListener
};

//----------------------------------------------------------------------
TEST(GG_LWIP, Test_LwipOneSocket) {
    GG_Result result;

    // create a netif
    GG_LwipGenericNetworkInterface* lwip_if;
    result = GG_LwipGenericNetworkInterface_Create(0, NULL, &lwip_if);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(lwip_if != NULL);

    // register the netif
    GG_IpAddress my_addr;
    GG_IpAddress my_netmask;
    GG_IpAddress my_gateway;
    GG_IpAddress_SetFromString(&my_addr,    "169.254.100.4");
    GG_IpAddress_SetFromString(&my_netmask, "255.255.255.254");
    GG_IpAddress_SetFromString(&my_gateway, "169.254.100.5");
    GG_LwipGenericNetworkInterface_Register(lwip_if, &my_addr, &my_netmask, &my_gateway, true);

    // create an lwip socket to send and receive
    GG_DatagramSocket* socket;
    GG_SocketAddress local_address;
    GG_IpAddress_SetFromString(&local_address.address, "0.0.0.0");
    local_address.port = 1234;
    GG_SocketAddress remote_address;
    GG_IpAddress_SetFromString(&remote_address.address, "169.254.100.5");
    remote_address.port = 1235;
    result = GG_LwipDatagramSocket_Create(&local_address, &remote_address, false, 1024, &socket);
    CHECK_EQUAL(GG_SUCCESS, result);

    // setup a sink to receive the data from the socket
    Receiver socket_receiver;
    memset(socket_receiver.message, 0, sizeof(socket_receiver.message));
    socket_receiver.peer.port = 0;
    GG_IpAddress_SetFromInteger(&socket_receiver.peer.address, 0);
    GG_SET_INTERFACE(&socket_receiver, Receiver, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(socket), GG_CAST(&socket_receiver, GG_DataSink));

    // setup a sink to receive the data from the netif
    GG_MemoryDataSink* netif_sink;
    result = GG_MemoryDataSink_Create(&netif_sink);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if),
                              GG_MemoryDataSink_AsDataSink(netif_sink));

    // send a message through the socket
    uint8_t message[4] = { 0x01, 0x02, 0x03, 0x04 };
    GG_StaticBuffer message_buffer;
    GG_StaticBuffer_Init(&message_buffer, message, sizeof(message));
    result = GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(socket),
                                 GG_StaticBuffer_AsBuffer(&message_buffer),
                                 NULL);
    CHECK_EQUAL(GG_SUCCESS, result);

    // check that the IP packet we received is what we expect
    GG_Buffer* ip_packet_buffer = GG_MemoryDataSink_GetBuffer(netif_sink);
    LONGS_EQUAL(20 + 8 + sizeof(message), GG_Buffer_GetDataSize(ip_packet_buffer));
    const uint8_t* ip_packet = GG_Buffer_GetData(ip_packet_buffer);
    MEMCMP_EQUAL(my_addr.ipv4, &ip_packet[12], 4);                       // IP src addr
    MEMCMP_EQUAL(remote_address.address.ipv4, &ip_packet[16], 4);        // IP dst addr
    LONGS_EQUAL(local_address.port, GG_BytesToInt16Be(&ip_packet[20]));  // UDP src port
    LONGS_EQUAL(remote_address.port, GG_BytesToInt16Be(&ip_packet[22])); // UDP dst port
    LONGS_EQUAL(8 + sizeof(message), GG_BytesToInt16Be(&ip_packet[24])); // UDP length

    // swap the IP addresses and ports, and send the packet back up
    GG_DynamicBuffer* out_buffer;
    result = GG_DynamicBuffer_Create(GG_Buffer_GetDataSize(ip_packet_buffer), &out_buffer);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DynamicBuffer_SetData(out_buffer, ip_packet, GG_Buffer_GetDataSize(ip_packet_buffer));
    uint8_t* out = GG_DynamicBuffer_UseData(out_buffer);
    out[15] = 5;
    out[19] = 4;
    out[21] = 0xd3;
    out[23] = 0xd2;
    out[10] = 0;
    out[11] = 0;
    uint16_t checksum = ~GG_Ipv4Checksum(out, 20);
    out[10] = (checksum >> 8) & 0xFF;
    out[11] = (checksum)      & 0xFF;
    result = GG_DataSink_PutData(GG_LwipGenericNetworkInterface_AsDataSink(lwip_if),
                                 GG_DynamicBuffer_AsBuffer(out_buffer),
                                 NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(message, socket_receiver.message, sizeof(message));

    // cleanup
    GG_DynamicBuffer_Release(out_buffer);
    GG_DatagramSocket_Destroy(socket);
    GG_LwipGenericNetworkInterface_Deregister(lwip_if);
    GG_LwipGenericNetworkInterface_Destroy(lwip_if);
}

//----------------------------------------------------------------------
TEST(GG_LWIP, Test_LwipTwoSockets) {
    // create a loop
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_Loop_BindToCurrentThread(loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // create a first netif
    GG_LwipGenericNetworkInterface* lwip_if_1;
    result = GG_LwipGenericNetworkInterface_Create(0, NULL, &lwip_if_1);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(lwip_if_1 != NULL);

    // create a second netif
    GG_LwipGenericNetworkInterface* lwip_if_2;
    result = GG_LwipGenericNetworkInterface_Create(0, NULL, &lwip_if_2);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(lwip_if_2 != NULL);

    // connect the two network intefaces to each other
    // so that the data sent by one is received by the other and the other way around too.
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if_1),
                              GG_LwipGenericNetworkInterface_AsDataSink(lwip_if_2));
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if_2),
                              GG_LwipGenericNetworkInterface_AsDataSink(lwip_if_1));

    // register the first netif
    GG_IpAddress my_addr_1;
    GG_IpAddress my_netmask_1;
    GG_IpAddress my_gateway_1;
    GG_IpAddress_SetFromString(&my_addr_1,    "10.0.0.1");
    GG_IpAddress_SetFromString(&my_netmask_1, "255.255.255.255");
    GG_IpAddress_SetFromString(&my_gateway_1, "10.0.1.1");
    GG_LwipGenericNetworkInterface_Register(lwip_if_1, &my_addr_1, &my_netmask_1, &my_gateway_1, true);

    // register the second netif
    GG_IpAddress my_addr_2;
    GG_IpAddress my_netmask_2;
    GG_IpAddress my_gateway_2;
    GG_IpAddress_SetFromString(&my_addr_2,    "10.0.1.1");
    GG_IpAddress_SetFromString(&my_netmask_2, "255.255.255.255");
    GG_IpAddress_SetFromString(&my_gateway_2, "10.0.0.1");
    GG_LwipGenericNetworkInterface_Register(lwip_if_2, &my_addr_2, &my_netmask_2, &my_gateway_2, false);

    // create an lwip socket to send
    GG_DatagramSocket* send_socket;
    GG_SocketAddress send_local_address;
    GG_IpAddress_SetFromString(&send_local_address.address, "10.0.1.1");
    send_local_address.port = 1234;
    GG_SocketAddress send_remote_address;
    GG_IpAddress_SetFromString(&send_remote_address.address, "10.0.0.1");
    send_remote_address.port = 1235;
    GG_LwipDatagramSocket_Create(&send_local_address, &send_remote_address, false, 1024, &send_socket);

    // create an lwip socket to receive
    GG_DatagramSocket* receive_socket;
    GG_SocketAddress receive_local_address;
    receive_local_address.address = GG_IpAddress_Any;
    receive_local_address.port = 1235;
    GG_LwipDatagramSocket_Create(&receive_local_address, NULL, false, 1024, &receive_socket);

    // setup a sink to receive the data
    Receiver receiver;
    memset(receiver.message, 0, sizeof(receiver.message));
    receiver.peer.port = 0;
    GG_IpAddress_SetFromInteger(&receiver.peer.address, 0);
    GG_SET_INTERFACE(&receiver, Receiver, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(receive_socket), GG_CAST(&receiver, GG_DataSink));

    // send a message through the socket
    uint8_t message[4] = { 0x01, 0x02, 0x03, 0x04 };
    GG_StaticBuffer message_buffer;
    GG_StaticBuffer_Init(&message_buffer, message, sizeof(message));
    result = GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(send_socket),
                                 GG_StaticBuffer_AsBuffer(&message_buffer),
                                 NULL);

    // check that the message arrived intact
    MEMCMP_EQUAL(message, receiver.message, sizeof(message));

    // check that the IP address and port of the sender is seen by the receiver
    CHECK_EQUAL(1234, receiver.peer.port);
    CHECK_EQUAL(10, receiver.peer.address.ipv4[0]);
    CHECK_EQUAL(0, receiver.peer.address.ipv4[1]);
    CHECK_EQUAL(1, receiver.peer.address.ipv4[2]);
    CHECK_EQUAL(1, receiver.peer.address.ipv4[3]);

    // cleanup
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if_1), NULL);
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if_2), NULL);
    GG_DatagramSocket_Destroy(send_socket);
    GG_DatagramSocket_Destroy(receive_socket);
    GG_LwipGenericNetworkInterface_Deregister(lwip_if_1);
    GG_LwipGenericNetworkInterface_Deregister(lwip_if_2);
    GG_LwipGenericNetworkInterface_Destroy(lwip_if_1);
    GG_LwipGenericNetworkInterface_Destroy(lwip_if_2);
    GG_Loop_Destroy(loop);
}

//----------------------------------------------------------------------
TEST(GG_LWIP, Test_LwipTwoInterfaces) {
    GG_Result result;

    // create netif 1
    GG_LwipGenericNetworkInterface* lwip_if_1;
    result = GG_LwipGenericNetworkInterface_Create(0, NULL, &lwip_if_1);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(lwip_if_1 != NULL);

    // register netif 1
    GG_IpAddress my_addr_1;
    GG_IpAddress my_netmask_1;
    GG_IpAddress my_gateway_1;
    GG_IpAddress_SetFromString(&my_addr_1,    "169.254.100.4");
    GG_IpAddress_SetFromString(&my_netmask_1, "255.255.255.254");
    GG_IpAddress_SetFromString(&my_gateway_1, "169.254.100.5");
    GG_LwipGenericNetworkInterface_Register(lwip_if_1, &my_addr_1, &my_netmask_1, &my_gateway_1, true);

    // setup a sink to receive the data from netif 1
    GG_MemoryDataSink* netif_sink_1;
    result = GG_MemoryDataSink_Create(&netif_sink_1);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if_1),
                              GG_MemoryDataSink_AsDataSink(netif_sink_1));

    // create netif 2
    GG_LwipGenericNetworkInterface* lwip_if_2;
    result = GG_LwipGenericNetworkInterface_Create(0, NULL, &lwip_if_2);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(lwip_if_2 != NULL);

    // register netif 2
    GG_IpAddress my_addr_2;
    GG_IpAddress my_netmask_2;
    GG_IpAddress my_gateway_2;
    GG_IpAddress_SetFromString(&my_addr_2,    "169.254.100.6");
    GG_IpAddress_SetFromString(&my_netmask_2, "255.255.255.254");
    GG_IpAddress_SetFromString(&my_gateway_2, "169.254.100.7");
    GG_LwipGenericNetworkInterface_Register(lwip_if_2, &my_addr_2, &my_netmask_2, &my_gateway_2, true);

    // setup a sink to receive the data from netif 2
    GG_MemoryDataSink* netif_sink_2;
    result = GG_MemoryDataSink_Create(&netif_sink_2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_LwipGenericNetworkInterface_AsDataSource(lwip_if_2),
                              GG_MemoryDataSink_AsDataSink(netif_sink_2));

    // create an lwip socket to send and receive
    GG_DatagramSocket* socket;
    GG_SocketAddress local_address;
    GG_IpAddress_SetFromString(&local_address.address, "0.0.0.0");
    local_address.port = 1234;
    result = GG_LwipDatagramSocket_Create(&local_address, NULL, false, 1024, &socket);
    CHECK_EQUAL(GG_SUCCESS, result);

    // setup a sink to receive the data from the socket
    Receiver socket_receiver;
    memset(socket_receiver.message, 0, sizeof(socket_receiver.message));
    socket_receiver.peer.port = 0;
    GG_IpAddress_SetFromInteger(&socket_receiver.peer.address, 0);
    GG_SET_INTERFACE(&socket_receiver, Receiver, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(socket), GG_CAST(&socket_receiver, GG_DataSink));

    // send a message to GW 1 through the socket
    uint8_t message[4] = { 0x01, 0x02, 0x03, 0x04 };
    GG_StaticBuffer message_buffer;
    GG_StaticBuffer_Init(&message_buffer, message, sizeof(message));
    GG_SocketAddressMetadata socket_metadata = GG_DESTINATION_SOCKET_ADDRESS_METADATA_INITIALIZER(my_gateway_1, 1234);
    result = GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(socket),
                                 GG_StaticBuffer_AsBuffer(&message_buffer),
                                 &socket_metadata.base);
    CHECK_EQUAL(GG_SUCCESS, result);

    // check that netif 2 didn't receive anything
    LONGS_EQUAL(0, GG_Buffer_GetDataSize(GG_MemoryDataSink_GetBuffer(netif_sink_2)));

    // check that the IP packet we received on netif 1 is what we expect
    GG_Buffer* ip_packet_buffer = GG_MemoryDataSink_GetBuffer(netif_sink_1);
    LONGS_EQUAL(20 + 8 + sizeof(message), GG_Buffer_GetDataSize(ip_packet_buffer));
    const uint8_t* ip_packet = GG_Buffer_GetData(ip_packet_buffer);
    MEMCMP_EQUAL(my_addr_1.ipv4, &ip_packet[12], 4);                     // IP src addr
    MEMCMP_EQUAL(my_gateway_1.ipv4, &ip_packet[16], 4);                  // IP dst addr
    LONGS_EQUAL(1234, GG_BytesToInt16Be(&ip_packet[22]));                // UDP dst port
    LONGS_EQUAL(8 + sizeof(message), GG_BytesToInt16Be(&ip_packet[24])); // UDP length

    // send a message to GW 2 through the socket
    GG_SocketAddressMetadata socket_metadata_2 = GG_DESTINATION_SOCKET_ADDRESS_METADATA_INITIALIZER(my_gateway_2, 1234);
    result = GG_DataSink_PutData(GG_DatagramSocket_AsDataSink(socket),
                                 GG_StaticBuffer_AsBuffer(&message_buffer),
                                 &socket_metadata_2.base);
    CHECK_EQUAL(GG_SUCCESS, result);

    // check that netif 1 didn't receive anything
    LONGS_EQUAL(20 + 8 + sizeof(message), GG_Buffer_GetDataSize(GG_MemoryDataSink_GetBuffer(netif_sink_2)));

    // check that the IP packet we received on netif 2 is what we expect
    ip_packet_buffer = GG_MemoryDataSink_GetBuffer(netif_sink_2);
    LONGS_EQUAL(20 + 8 + sizeof(message), GG_Buffer_GetDataSize(ip_packet_buffer));
    ip_packet = GG_Buffer_GetData(ip_packet_buffer);
    MEMCMP_EQUAL(my_addr_2.ipv4, &ip_packet[12], 4);                     // IP src addr
    MEMCMP_EQUAL(my_gateway_2.ipv4, &ip_packet[16], 4);                  // IP dst addr
    LONGS_EQUAL(1234, GG_BytesToInt16Be(&ip_packet[22]));                // UDP dst port
    LONGS_EQUAL(8 + sizeof(message), GG_BytesToInt16Be(&ip_packet[24])); // UDP length

    // send an IP packet up to my_addr_1
    GG_DynamicBuffer* out_buffer;
    result = GG_DynamicBuffer_Create(GG_Buffer_GetDataSize(ip_packet_buffer), &out_buffer);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DynamicBuffer_SetData(out_buffer, ip_packet, GG_Buffer_GetDataSize(ip_packet_buffer));
    uint8_t* out = GG_DynamicBuffer_UseData(out_buffer);
    out[15] = 5;
    out[19] = 4;
    out[20] = 0x04;
    out[21] = 0xd3;
    out[22] = 0x04;
    out[23] = 0xd2;
    out[26] = 0;
    out[27] = 0;
    out[10] = 0;
    out[11] = 0;
    uint16_t checksum = ~GG_Ipv4Checksum(out, 20);
    out[10] = (checksum >> 8) & 0xFF;
    out[11] = (checksum)      & 0xFF;
    result = GG_DataSink_PutData(GG_LwipGenericNetworkInterface_AsDataSink(lwip_if_1),
                                 GG_DynamicBuffer_AsBuffer(out_buffer),
                                 NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(message, socket_receiver.message, sizeof(message));
    memset(socket_receiver.message, 0, sizeof(socket_receiver.message));

    // send an IP packet up to my_addr_2
    out[15] = 7;
    out[19] = 6;
    out[20] = 0x04;
    out[21] = 0xd3;
    out[22] = 0x04;
    out[23] = 0xd2;
    out[26] = 0;
    out[27] = 0;
    out[10] = 0;
    out[11] = 0;
    checksum = ~GG_Ipv4Checksum(out, 20);
    out[10] = (checksum >> 8) & 0xFF;
    out[11] = (checksum)      & 0xFF;
    result = GG_DataSink_PutData(GG_LwipGenericNetworkInterface_AsDataSink(lwip_if_2),
                                 GG_DynamicBuffer_AsBuffer(out_buffer),
                                 NULL);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(message, socket_receiver.message, sizeof(message));

    // cleanup
    GG_DynamicBuffer_Release(out_buffer);
    GG_DatagramSocket_Destroy(socket);
    GG_LwipGenericNetworkInterface_Deregister(lwip_if_1);
    GG_LwipGenericNetworkInterface_Destroy(lwip_if_1);
    GG_LwipGenericNetworkInterface_Deregister(lwip_if_2);
    GG_LwipGenericNetworkInterface_Destroy(lwip_if_2);
}
