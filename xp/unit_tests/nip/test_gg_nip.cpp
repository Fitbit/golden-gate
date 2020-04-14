/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include <cstring>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_port.h"
#include "xp/module/gg_module.h"
#include "xp/utils/gg_memory_data_source.h"
#include "xp/utils/gg_memory_data_sink.h"
#include "xp/nip/gg_nip.h"
#include "xp/sockets/ports/nip/gg_nip_sockets.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_NIP)
{
    void setup(void) {
        GG_Nip_Initialize();
    }

    void teardown(void) {
        GG_Nip_Terminate();
    }
};

//----------------------------------------------------------------------
TEST(GG_NIP, Test_NipSockets) {
    GG_DatagramSocket* socket;

    uint8_t source_data1_bytes[34] = {
        0x45, 0x00, 0x00, 0x22, 0xb7, 0x82, 0x00, 0x00,
        0x40, 0x11, 0xab, 0x40, 0x0a, 0x01, 0x02, 0x03,
        0x0a, 0x01, 0x02, 0x04, 0xde, 0x0b, 0x04, 0xd2,
        0x00, 0x0e, 0xc1, 0x0f, 0x68, 0x65, 0x6c, 0x6c,
        0x6f, 0x0a
    };
    GG_DynamicBuffer* source_data1;
    GG_DynamicBuffer_Create(0, &source_data1);
    GG_DynamicBuffer_AppendData(source_data1, source_data1_bytes, sizeof(source_data1_bytes));
    GG_MemoryDataSource* transport_source;
    GG_MemoryDataSource_Create(GG_DynamicBuffer_AsBuffer(source_data1), sizeof(source_data1_bytes), &transport_source);
    CHECK_TRUE(transport_source != NULL);

    GG_MemoryDataSink* transport_sink;
    GG_MemoryDataSink_Create(&transport_sink);
    CHECK_TRUE(transport_sink != NULL);

    GG_IpAddress netif_address;
    GG_IpAddress_SetFromString(&netif_address, "169.254.0.2");
    GG_Result result = GG_Nip_Configure(&netif_address);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_DataSource_SetDataSink(GG_Nip_AsDataSource(), GG_MemoryDataSink_AsDataSink(transport_sink));
    GG_DataSource_SetDataSink(GG_MemoryDataSource_AsDataSource(transport_source), GG_Nip_AsDataSink());

    result = GG_NipDatagramSocket_Create(NULL, NULL, false, 1024, &socket);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(socket != NULL);

    GG_DynamicBuffer* data1;
    GG_DynamicBuffer_Create(0, &data1);
    uint8_t data1_bytes[3] = {0x01, 0x02, 0x03};
    GG_DynamicBuffer_AppendData(data1, data1_bytes, sizeof(data1_bytes));

    GG_DataSink* socket_sink = GG_DatagramSocket_AsDataSink(socket);
    result = GG_DataSink_PutData(socket_sink, GG_DynamicBuffer_AsBuffer(data1), NULL);
    LONGS_EQUAL(result, GG_ERROR_INVALID_STATE);

    GG_SocketAddressMetadata metadata =
        GG_DESTINATION_SOCKET_ADDRESS_METADATA_INITIALIZER(GG_IP_ADDRESS_NULL_INITIALIZER, 1234);
    GG_IpAddress_SetFromString(&metadata.socket_address.address, "169.254.0.3");
    result = GG_DataSink_PutData(socket_sink, GG_DynamicBuffer_AsBuffer(data1), &metadata.base);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_Buffer* received_buffer = GG_MemoryDataSink_GetBuffer(transport_sink);
    LONGS_EQUAL(20 + 8 + 3, GG_Buffer_GetDataSize(received_buffer));

    GG_MemoryDataSink* udp_sink;
    GG_MemoryDataSink_Create(&udp_sink);
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(socket),
                              GG_MemoryDataSink_AsDataSink(udp_sink));
    GG_MemoryDataSource_Start(transport_source);
    GG_Buffer* udp_buffer = GG_MemoryDataSink_GetBuffer(udp_sink);
    LONGS_EQUAL(0, GG_Buffer_GetDataSize(udp_buffer));

    GG_IpAddress_SetFromString(&netif_address, "10.1.2.4");
    result = GG_Nip_Configure(&netif_address);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_DataSource_SetDataSink(GG_Nip_AsDataSource(), GG_MemoryDataSink_AsDataSink(transport_sink));
    GG_DataSource_SetDataSink(GG_MemoryDataSource_AsDataSource(transport_source), GG_Nip_AsDataSink());
    GG_MemoryDataSource_Rewind(transport_source);
    GG_MemoryDataSource_Start(transport_source);
    LONGS_EQUAL(6, GG_Buffer_GetDataSize(udp_buffer));
    MEMCMP_EQUAL("hello\n", GG_Buffer_GetData(udp_buffer), 6);

    GG_DatagramSocket_Destroy(socket);
}
