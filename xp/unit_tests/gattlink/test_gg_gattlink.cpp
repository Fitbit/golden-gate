#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_types.h"
#include "xp/gattlink/gg_gattlink.h"

static size_t s_max_packet_size = 8;

typedef struct {
    uint8_t send_buffer[1024];
    uint8_t curr_offset;
    uint8_t unread_bytes;
} SendBuf;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_GattlinkSessionConfig unittest_session_config;
static GG_TimerScheduler* TimerScheduler;
static uint32_t TimerSchedulerNow;

//----------------------------------------------------------------------
//  Gattlink Client
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_GattlinkClient);

    bool                 sendRawDataShouldFail;
    bool                 consumedFunctionMocked;
    GG_GattlinkProtocol* client;
    SendBuf              send_buf;
} GattlinkClient;

static GattlinkClient gattlink_client;

//----------------------------------------------------------------------
static void
AddToSendBuf(SendBuf *send_buf, const void *data, size_t data_len)
{
    // Sanity check we haven't made a mistake in the unit test itself
    CHECK((data_len + send_buf->curr_offset + send_buf->unread_bytes) < sizeof(send_buf->send_buffer));
    uint8_t *start_ptr = &send_buf->send_buffer[send_buf->curr_offset + send_buf->unread_bytes];
    memcpy(start_ptr, data, data_len);
    send_buf->unread_bytes += data_len;
}

#define MOCK_REF_NAME_LEN 32
static void
BuildDataMockRefName(char *name, size_t name_len, int psn, int ack)
{
    int bytes_written = 0;
    if (ack != -1) {
        bytes_written = snprintf(name, name_len, "ack%d", ack);
    }
    if (psn != -1) {
        name += bytes_written;
        snprintf(name, name_len - (size_t)bytes_written, "psn%d", psn);
    }
}

static void
BuildControlMockRefName(char *name, uint8_t control_id)
{
    if (control_id == 0) {
        strncpy(name, "ctrl_rr", MOCK_REF_NAME_LEN);
    } else if (control_id == 1) {
        strncpy(name, "ctrl_rc", MOCK_REF_NAME_LEN);
    } else {
        strncpy(name, "ctrl_unrecognized", MOCK_REF_NAME_LEN);
    }
}

static void
OpenGattlink(uint8_t rx_window_size, uint8_t tx_window_size)
{
    GG_GattlinkProtocol_Start(gattlink_client.client);
    uint8_t response[] = { 0x81, 0x00, 0x00, rx_window_size, tx_window_size };
    GG_Result result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &response, sizeof(response));
    CHECK_EQUAL(GG_SUCCESS, result);
}

static void ForceSendAndPayloadAck(uint8_t psn)
{
    GG_Result result;
    uint8_t data[2];

    memset(&data[0], psn, sizeof(data));
    AddToSendBuf(&gattlink_client.send_buf, &data[1], sizeof(data) - 1);

    char name[MOCK_REF_NAME_LEN];
    BuildDataMockRefName(name, sizeof(name), psn, -1);
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &data[0], sizeof(data));
    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(gattlink_client.client);

    uint8_t ack = 0x40 | (psn & 0x1f);
    mock().expectNoCall("GattlinkClient_NotifyIncomingDataAvailable");

    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &ack, sizeof(ack));
    CHECK_EQUAL(GG_SUCCESS, result);

    // an ack should not result in any client read-able data
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), 0);

    mock().checkExpectations();
    mock().clear();
}

//----------------------------------------------------------------------
static void
GattlinkClient_NotifySessionUp(GG_GattlinkClient* _self)
{
    GG_COMPILER_UNUSED(_self);
    mock().actualCall("GattlinkClient_NotifySessionUp");
}

//----------------------------------------------------------------------
static void
GattlinkClient_NotifySessionReset(GG_GattlinkClient* _self)
{
    GG_COMPILER_UNUSED(_self);
    mock().actualCall("GattlinkClient_NotifySessionReset");
}

//----------------------------------------------------------------------
static void
GattlinkClient_NotifySessionStalled(GG_GattlinkClient* _self, uint32_t stalled_time)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(stalled_time);
}

//----------------------------------------------------------------------
static GG_Result
GattlinkClient_SendRawData(GG_GattlinkClient* _self, const void *tx_raw_data, size_t tx_raw_data_len)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);
    if (self->sendRawDataShouldFail) {
        GG_COMPILER_UNUSED(tx_raw_data);
        GG_COMPILER_UNUSED(tx_raw_data_len);
        mock().actualCall("GattlinkClient_SendRawDataForceFailure");
        return GG_ERROR_INTERNAL;
    }

    if (tx_raw_data_len == 0) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    const uint8_t *data = (const uint8_t *)tx_raw_data;
    uint8_t hdr = data[0];

    char name[MOCK_REF_NAME_LEN];
    if (hdr & 0x80) { // it's a control packet
        BuildControlMockRefName(name, hdr & 0x7f);
    } else {
        int ack = -1;
        int psn  = -1;
        if ((hdr & 0x40) != 0) {
            ack = ((int)hdr)&0x1f;
            if (tx_raw_data_len > 1) {
                psn = data[1] & 0x1f;
            }
        } else {
            psn = data[0] & 0x1f;
        }
        BuildDataMockRefName(name, sizeof(name), psn, ack);
    }

    mock()
        .actualCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, (const uint8_t *)tx_raw_data, tx_raw_data_len);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static size_t
GattlinkClient_GetTransportMaxPacketSize(GG_GattlinkClient* _self)
{
    GG_COMPILER_UNUSED(_self);
    return s_max_packet_size;
}

//----------------------------------------------------------------------
static size_t
GattlinkClient_GetOutgoingDataAvailable(GG_GattlinkClient* _self)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);
    return self->send_buf.unread_bytes;
}
//----------------------------------------------------------------------
static int
GattlinkClient_GetOutgoingData(GG_GattlinkClient* _self, size_t offset, void* buffer, size_t size)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);
    if (size > (self->send_buf.unread_bytes - offset)) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    uint8_t *start_ptr = &self->send_buf.send_buffer[self->send_buf.curr_offset];
    memcpy(buffer, &start_ptr[offset], size);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GattlinkClient_ConsumeOutgoingData(GG_GattlinkClient* _self, size_t num_bytes)
{
    GattlinkClient* self = GG_SELF(GattlinkClient, GG_GattlinkClient);

    if (self->consumedFunctionMocked) {
        mock()
            .actualCall("GattlinkClient_ConsumeOutgoingData")
            .onObject(self)
            .withIntParameter("num_bytes", (int)num_bytes);
    }

    if (num_bytes > self->send_buf.unread_bytes) {
        return;
    }

    self->send_buf.unread_bytes -= num_bytes;
    self->send_buf.curr_offset += num_bytes;

    if (self->send_buf.unread_bytes == 0) { // all read, reset
        memset(&self->send_buf, 0x00, sizeof(self->send_buf));
    }
}

//----------------------------------------------------------------------
static void
GattlinkClient_NotifyIncomingDataAvailable(GG_GattlinkClient* _self)
{
    GG_COMPILER_UNUSED(_self);
    mock().actualCall("GattlinkClient_NotifyIncomingDataAvailable");
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GattlinkClient, GG_GattlinkClient) {
    GattlinkClient_GetOutgoingDataAvailable,
    GattlinkClient_GetOutgoingData,
    GattlinkClient_ConsumeOutgoingData,
    GattlinkClient_NotifyIncomingDataAvailable,
    GattlinkClient_GetTransportMaxPacketSize,
    GattlinkClient_SendRawData,
    GattlinkClient_NotifySessionUp,
    GattlinkClient_NotifySessionReset,
    GattlinkClient_NotifySessionStalled
};

TEST_GROUP(GATTLINK)
{
    void setup(void) {
        GG_Result result;

        mock().disable(); // individual functions using mocks can enable

        result = GG_TimerScheduler_Create(&TimerScheduler);
        CHECK_EQUAL(GG_SUCCESS, result);

        unittest_session_config.max_tx_window_size = 12;
        unittest_session_config.max_rx_window_size = 12;

        memset(&gattlink_client.send_buf, 0x00, sizeof(gattlink_client.send_buf));

        gattlink_client.consumedFunctionMocked = false;
        GG_SET_INTERFACE(&gattlink_client, GattlinkClient, GG_GattlinkClient);
        result = GG_GattlinkProtocol_Create(GG_CAST(&gattlink_client, GG_GattlinkClient),
                                            &unittest_session_config,
                                            TimerScheduler,
                                            &gattlink_client.client);
        CHECK_EQUAL(GG_SUCCESS, result);
    }

    void teardown(void) {
        GG_GattlinkProtocol_Destroy(gattlink_client.client);
        GG_TimerScheduler_Destroy(TimerScheduler);
        TimerScheduler = NULL;
        mock().clear();
    }
};

TEST(GATTLINK, Test_GattlinkSelfInitiatedOpen)
{
    GG_Result result;
    uint8_t reset_req[] = { 0x80 };
    char name[MOCK_REF_NAME_LEN];

    mock().enable();
    BuildControlMockRefName(name, reset_req[0] & 0x7f);

    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &reset_req[0], sizeof(reset_req));

    result = GG_GattlinkProtocol_Start(gattlink_client.client);
    CHECK_EQUAL(GG_SUCCESS, result);

    // Send a response
    uint8_t response[] = { 0x81, 0x00, 0x00, 0x4, 0x4 };
    mock().expectOneCall("GattlinkClient_NotifySessionUp");

    uint8_t rc[] = {
        0x81,
        0x00,
        0x00,
        unittest_session_config.max_rx_window_size,
        unittest_session_config.max_tx_window_size
    };
    BuildControlMockRefName(name,  rc[0] & 0x7f);
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &rc[0], sizeof(rc));

    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &response, sizeof(response));
    CHECK_EQUAL(GG_SUCCESS, result);

    mock().checkExpectations();
}

TEST(GATTLINK, Test_GattlinkRemoteInitiatedReset)
{
    GG_Result result;

    OpenGattlink(0x8, 0x8);

    // TODO:
    uint8_t remote_reset[] = { 0x80, 0x00, 0x00, 0x4, 0x4 };
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &remote_reset[0], sizeof(remote_reset));
    CHECK_EQUAL(GG_SUCCESS, result);
}

TEST(GATTLINK, Test_GattlinkInboundData)
{
    GG_Result result;

    OpenGattlink(0x8, 0x8);
    mock().enable();

    // Check that no data is a no-op
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, NULL, 0);
    CHECK_EQUAL(GG_SUCCESS, result);

    for (int psn = 0; psn < 4; psn++) {
        mock().expectOneCall("GattlinkClient_NotifyIncomingDataAvailable");

        uint8_t raw_data[] = { 0x00, 0x1, 0x2, 0x3 };
        raw_data[0] |= psn;
        const uint8_t expected_data_len = sizeof(raw_data) - 1;
        result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
        CHECK_EQUAL(GG_SUCCESS, result);
        LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), expected_data_len);

        uint8_t receive_data[expected_data_len];
        // make sure we can receive things in pieces
        for (size_t i = 0; i < expected_data_len; i++) {
            result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, i, &receive_data[i], 1);
            CHECK_EQUAL(GG_SUCCESS, result);
        }

        // we shouldn't be able to receive anything if we are out of bounds
        result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, expected_data_len, &receive_data[0], 1);
        CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

        // verify data received is correct
        MEMCMP_EQUAL(&raw_data[1], &receive_data[0], expected_data_len);

        // now test receiving data all in one piece
        memset(&receive_data[0], 0x00, sizeof(receive_data));
        result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, 0, &receive_data[0], expected_data_len);
        CHECK_EQUAL(GG_SUCCESS, result);
        MEMCMP_EQUAL(&raw_data[1], &receive_data[0], expected_data_len);

        // should be a no-op if we try to consume too much
        result = GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, expected_data_len * 10);
        CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);
        // actually consume the data
        result = GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, expected_data_len);
        CHECK_EQUAL(GG_SUCCESS, result);
        // we shouldn't be able to receive anything if all has been consumed
        result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, 0, &receive_data[0], 1);
        CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

        mock().checkExpectations();
    }
}

TEST(GATTLINK, Test_GattlinkInboundSendAckAfterTimeout)
{
    GG_Result result;

    OpenGattlink(0x8, 0x8);

    mock().enable();
    mock().expectNCalls(2, "GattlinkClient_NotifyIncomingDataAvailable");

    // Fire receiving and consuming a first packet with psn 0
    uint8_t raw_data[] = { 0x00, 0xA, 0xB, 0xC };
    const uint8_t expected_data_len = sizeof(raw_data) - 1;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), expected_data_len);

    uint8_t receive_data[expected_data_len];
    result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, 0, &receive_data[0], expected_data_len);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(&raw_data[1], &receive_data[0], expected_data_len);
    GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, expected_data_len);

    // Fire receiving and consuming a second packet with psn 1
    raw_data[0] |= 1;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), expected_data_len);

    result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, 0, &receive_data[0], expected_data_len);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(&raw_data[1], &receive_data[0], expected_data_len);
    GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, expected_data_len);

    uint8_t ack_payload[] = { 0x41 };
    char name[MOCK_REF_NAME_LEN];
    BuildDataMockRefName(name, sizeof(name), -1, 1);
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &ack_payload[0], sizeof(ack_payload));

    // force an ack timeout
    TimerSchedulerNow += 400;
    GG_TimerScheduler_SetTime(TimerScheduler, TimerSchedulerNow);

    mock().checkExpectations();
}

TEST(GATTLINK, Test_GattlinkInboundReSendAck)
{
    GG_Result result;

    OpenGattlink(0x8, 0x8);

    mock().enable();
    mock().expectNCalls(2, "GattlinkClient_NotifyIncomingDataAvailable");

    // Fire receiving and consuming a first packet with psn 0
    uint8_t raw_data[] = { 0x00, 0xA, 0xB, 0xC };
    const uint8_t expected_data_len = sizeof(raw_data) - 1;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), expected_data_len);

    uint8_t receive_data[expected_data_len];
    result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, 0, &receive_data[0], expected_data_len);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(&raw_data[1], &receive_data[0], expected_data_len);
    GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, expected_data_len);

    // Fire receiving and consuming a second packet with psn 1
    raw_data[0] |= 1;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), expected_data_len);

    result = GG_GattlinkProtocol_GetIncomingData(gattlink_client.client, 0, &receive_data[0], expected_data_len);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL(&raw_data[1], &receive_data[0], expected_data_len);
    GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, expected_data_len);

    uint8_t ack_payload[] = { 0x41 };
    char name[MOCK_REF_NAME_LEN];
    BuildDataMockRefName(name, sizeof(name), -1, 1);
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &ack_payload[0], sizeof(ack_payload));

    // force an ack timeout
    TimerSchedulerNow += 400;
    GG_TimerScheduler_SetTime(TimerScheduler, TimerSchedulerNow);

    mock().checkExpectations();
    mock().clear();

    // Simulate ack not being received and packets being retransmitted
    // which would trigger one ack with last received packet psn
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &ack_payload[0], sizeof(ack_payload));

    raw_data[0] = 0;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_SUCCESS, result);
    raw_data[0] |= 1;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), 0);

    // force an ack timeout
    TimerSchedulerNow += 400;
    GG_TimerScheduler_SetTime(TimerScheduler, TimerSchedulerNow);

    mock().checkExpectations();
}

TEST(GATTLINK, Test_GattlinkOutboundData)
{
    const uint8_t window_size = 0x8;
    OpenGattlink(window_size, window_size);

    mock().enable();
    s_max_packet_size = 5;
    char name[MOCK_REF_NAME_LEN];

    for (uint8_t psn = 0; psn < (window_size + 1); psn++) {
        uint8_t data[] = { psn /* the hdr we will receive */ , 0x1, 0x2, 0x3, 0x4 };
        AddToSendBuf(&gattlink_client.send_buf, &data[1], sizeof(data) - 1);
        BuildDataMockRefName(name, sizeof(name), psn, -1);

        if (psn < 8) {
            mock()
                .expectOneCall("GattlinkClient_SendRawData")
                .withMemoryBufferParameter(name, &data[0], sizeof(data));
        } else {
            mock().expectNoCall("GattlinkClient_SendRawData");
        }

        // notify the client data is read to send
        GG_GattlinkProtocol_NotifyOutgoingDataAvailable(gattlink_client.client);
        mock().checkExpectations();
        mock().clear();
    }

    // Let's fire off an ACK, we would expect to then receive the last outstanding packet
    uint8_t ack[] = { 0x47 };
    uint8_t expected_receive[] = { window_size, 0x1, 0x2, 0x3, 0x4 };
    BuildDataMockRefName(name, sizeof(name), window_size, -1);

    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &expected_receive[0], sizeof(expected_receive));

    GG_Result result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &ack, sizeof(ack));
    CHECK_EQUAL(GG_SUCCESS, result);

    mock().checkExpectations();

    // firing off the same ack again should be ignored
    mock().expectNoCall("GattlinkClient_SendRawData");
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &ack, sizeof(ack));
    CHECK_EQUAL(GG_SUCCESS, result);
}

TEST(GATTLINK, Test_GattlinkOutboundDataPayloadBiggerThanTransport)
{
    const uint8_t window_size = 0x8;
    OpenGattlink(window_size, window_size);

    mock().enable();
    const uint8_t max_packet_size = 8;
    s_max_packet_size = max_packet_size;

    uint8_t data[(max_packet_size - 1) * 4];
    for (uint8_t i = 0; i < sizeof(data); i++) {
        data[i] = i;
    }
    AddToSendBuf(&gattlink_client.send_buf, &data[0], sizeof(data));

    uint8_t expected_raw_payload[4][max_packet_size];
    for (uint8_t psn = 0; psn < 4; psn++) {
        expected_raw_payload[psn][0] = psn;
        for (int i = 1; i < max_packet_size; i++) {
            expected_raw_payload[psn][i] = ((uint8_t)i - 1) + psn * (max_packet_size - 1);
        }
        char name[MOCK_REF_NAME_LEN];
        BuildDataMockRefName(name, sizeof(name), psn, -1);
        mock()
            .expectOneCall("GattlinkClient_SendRawData")
            .withMemoryBufferParameter(name, &expected_raw_payload[psn][0], sizeof(expected_raw_payload[psn]));
    }
    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(gattlink_client.client);
    mock().checkExpectations();
}

TEST(GATTLINK, Test_GattlinkRetransmit)
{
    const uint8_t window_size = 0x8;
    OpenGattlink(window_size, window_size);

    mock().enable();
    ForceSendAndPayloadAck(0);
    ForceSendAndPayloadAck(1);
    ForceSendAndPayloadAck(2);

    uint8_t data3[] = { 3, 0xA, 0xB, 0xC };
    uint8_t data4[] = { 4, 0xD, 0xE, 0xF };

    char name[MOCK_REF_NAME_LEN];
    BuildDataMockRefName(name, sizeof(name), 3, -1);
    mock()
        .expectNCalls(1, "GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &data3[0], sizeof(data3));

    BuildDataMockRefName(name, sizeof(name), 4, -1);
    mock()
        .expectNCalls(2, "GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &data4[0], sizeof(data4));

    AddToSendBuf(&gattlink_client.send_buf, &data3[1], sizeof(data3) - 1);
    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(gattlink_client.client);

    AddToSendBuf(&gattlink_client.send_buf, &data4[1], sizeof(data4) - 1);
    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(gattlink_client.client);

    // ack the first payload
    uint8_t ack = 0x40 | 0x3;
    GG_Result result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &ack, sizeof(ack));
    CHECK_EQUAL(GG_SUCCESS, result);

    // force a retransmit timeout
    TimerSchedulerNow += 8000;
    GG_TimerScheduler_SetTime(TimerScheduler, TimerSchedulerNow);

    mock().checkExpectations();
}

TEST(GATTLINK, Test_GattlinkPsnWrapAround)
{
    const uint8_t window_size = 0x8;
    OpenGattlink(window_size, window_size);

    for (uint8_t packet = 0; packet < 60; packet++) {
        uint8_t psn = packet % 32;
        ForceSendAndPayloadAck(psn);
    }
}

TEST(GATTLINK, Test_GattlinkPsnDroppedPacket)
{
    GG_Result result;

    const uint8_t window_size = 0x8;
    OpenGattlink(window_size, window_size);

    mock().enable();

    const uint8_t num_sends = 4;

    // We are only sending num_sends packets, with some drops and retransmits in the middle
    mock().expectNCalls(num_sends, "GattlinkClient_NotifyIncomingDataAvailable");

    uint8_t raw_data[] = { 0x00, 0x1, 0x2, 0x3 };
    const uint8_t expected_data_len = sizeof(raw_data) - 1;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), expected_data_len);
    GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, expected_data_len);

    // pretend packet with psn=1 got dropped
    raw_data[0] = 2;
    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
    CHECK_EQUAL(GG_ERROR_GATTLINK_UNEXPECTED_PSN, result);
    LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), 0);

    // expect ack for psn 0 only
    uint8_t ack_payload[] = { 0x40 };
    char name[MOCK_REF_NAME_LEN];
    BuildDataMockRefName(name, sizeof(name), -1, 0);
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &ack_payload[0], sizeof(ack_payload));

    // force a first ack timeout
    TimerSchedulerNow += 400;
    GG_TimerScheduler_SetTime(TimerScheduler, TimerSchedulerNow);

    // pretend ack was dropped and packet is retransmitted from psn 0
    uint8_t ack_retransmit_payload[] = { 0x43 };
    BuildDataMockRefName(name, sizeof(name), -1, 3);
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &ack_retransmit_payload[0], sizeof(ack_retransmit_payload));

    for (uint8_t psn = 0; psn < num_sends; psn++) {
        raw_data[0] = psn;
        size_t len_we_expect = (psn == 0) ? 0 : expected_data_len;
        result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &raw_data, sizeof(raw_data));
        CHECK_EQUAL(GG_SUCCESS, result);
        LONGS_EQUAL(GG_GattlinkProtocol_GetIncomingDataAvailable(gattlink_client.client), len_we_expect);
        GG_GattlinkProtocol_ConsumeIncomingData(gattlink_client.client, len_we_expect);
    }

    // force an ack timeout
    TimerSchedulerNow += 400;
    GG_TimerScheduler_SetTime(TimerScheduler, TimerSchedulerNow);

    mock().checkExpectations();
}

// A transport level send failure should result in a retransmit
TEST(GATTLINK, Test_GattlinkTransportLevelFailure)
{
    const uint8_t window_size = 0x8;
    OpenGattlink(window_size, window_size);

    gattlink_client.sendRawDataShouldFail = true;

    uint8_t data[2] = { 0x0, 0xE };
    AddToSendBuf(&gattlink_client.send_buf, &data[1], sizeof(data) - 1);

    mock().expectOneCall("GattlinkClient_SendRawDataForceFailure");
    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(gattlink_client.client);

    // reset to working send implementation
    char name[MOCK_REF_NAME_LEN];
    BuildDataMockRefName(name, sizeof(name), 0, -1);
    mock()
        .expectOneCall("GattlinkClient_SendRawData")
        .withMemoryBufferParameter(name, &data[0], sizeof(data));
    gattlink_client.sendRawDataShouldFail = false;

    // force a retransmit timeout
    TimerSchedulerNow += 8000;
    GG_TimerScheduler_SetTime(TimerScheduler, TimerSchedulerNow);

    mock().checkExpectations();
}

TEST(GATTLINK, Test_GattlinkReceivePreviousAcks)
{
    const uint8_t window_size = 0x8;
    OpenGattlink(window_size, window_size);
    gattlink_client.consumedFunctionMocked = true;

    mock().enable();
    s_max_packet_size = 5;
    char name[MOCK_REF_NAME_LEN];

    for (uint8_t psn = 0; psn < (window_size / 2); psn++) {
        uint8_t data[] = { psn /* the hdr we will receive */ , 0x1, 0x2, 0x3, 0x4 };
        AddToSendBuf(&gattlink_client.send_buf, &data[1], sizeof(data) - 1);
        BuildDataMockRefName(name, sizeof(name), psn, -1);

        mock()
            .expectOneCall("GattlinkClient_SendRawData")
            .withMemoryBufferParameter(name, &data[0], sizeof(data));

        // notify the client data is read to send
        GG_GattlinkProtocol_NotifyOutgoingDataAvailable(gattlink_client.client);
        mock().checkExpectations();
        mock().clear();
    }

    // Let's fire off an ACK for the latest packet
    uint8_t ack[] = { 0x43 };

    mock().expectOneCall("GattlinkClient_ConsumeOutgoingData")
        .onObject(&gattlink_client)
        .withIntParameter("num_bytes", 16);

    GG_Result result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &ack, sizeof(ack));
    CHECK_EQUAL(GG_SUCCESS, result);

    mock().checkExpectations();

    // Fire off an ACK for a previous packet in the window.
    uint8_t old_ack[] = { 0x41 };

    // The old ack should be ignored.
    mock() .expectNoCall("GattlinkClient_ConsumeOutgoingData");

    result = GG_GattlinkProtocol_HandleIncomingRawData(gattlink_client.client, &old_ack, sizeof(old_ack));
    CHECK_EQUAL(GG_SUCCESS, result);

    mock().checkExpectations();
}
