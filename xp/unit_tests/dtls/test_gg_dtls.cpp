// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_lists.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_utils.h"
#include "xp/loop/gg_loop.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/utils/gg_async_pipe.h"
#include "xp/utils/gg_memory_data_source.h"
#include "xp/utils/gg_memory_data_sink.h"
#include "xp/tls/gg_tls.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_DTLS)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

static uint8_t PSK[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
static uint8_t PSK_IDENTITY[5] = {
    'h', 'e', 'l', 'l', 'o'
};

static uint8_t BOGUS_PSK[16] = {
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
    0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF
};
static uint8_t BOGUS_PSK_IDENTITY[5] = {
    'z', 'e', 'l', 'l', 'o'
};

//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);

    unsigned int buffers_received;
    unsigned int bytes_received;
    uint8_t      bytes[128];
} VerifierSink;

//----------------------------------------------------------------------
static GG_Result
VerifierSink_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    VerifierSink* self = (VerifierSink*)GG_SELF(VerifierSink, GG_DataSink);
    GG_COMPILER_UNUSED(metadata);

    // printf("=== got data, size=%u\n", (int)GG_Buffer_GetDataSize(data));
    if (self->bytes_received < sizeof(self->bytes)) {
        unsigned int chunk = (unsigned int)GG_MIN(sizeof(self->bytes) - self->bytes_received,
                                                  GG_Buffer_GetDataSize(data));

        memcpy(&self->bytes[self->bytes_received], GG_Buffer_GetData(data), chunk);
        self->bytes_received += chunk;
    }
    ++self->buffers_received;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
VerifierSink_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(VerifierSink, GG_DataSink) {
    VerifierSink_PutData,
    VerifierSink_SetListener
};

static VerifierSink TestSink;

//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_TlsKeyResolver);

    const uint8_t* psk_identity;
    size_t         psk_identity_size;
    const uint8_t* psk;
    size_t         psk_size;
} StaticPskResolver;

static GG_Result
StaticPskResolver_ResolvePsk(GG_TlsKeyResolver* _self,
                             const uint8_t*     key_identity,
                             size_t             key_identify_size,
                             uint8_t*           key,
                             size_t*            key_size)
{
    StaticPskResolver* self = (StaticPskResolver*)GG_SELF(StaticPskResolver, GG_TlsKeyResolver);

    // check that the identity matches what we have
    if (key_identify_size != self->psk_identity_size ||
        memcmp(key_identity, self->psk_identity, key_identify_size)) {
        return GG_ERROR_NO_SUCH_ITEM;
    }

    // check that the key can fit
    if (*key_size < self->psk_size) {
        *key_size = self->psk_size;
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // copy the key
    memcpy(key, self->psk, self->psk_size);
    *key_size = self->psk_size;

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(StaticPskResolver, GG_TlsKeyResolver) {
    StaticPskResolver_ResolvePsk
};

static StaticPskResolver PskResolver;

typedef enum {
    SD_TEST_NORMAL,
    SD_TEST_WRONG_CLIENT_KEY,
    SD_TEST_WRONG_SERVER_KEY,
    SD_TEST_WRONG_CLIENT_KEY_IDENTITY,
    SD_TEST_WRONG_SERVER_KEY_IDENTITY,
    SD_TEST_BOGUS_CLIENT_CIPHER_SUITE,
    SD_TEST_BOGUS_SERVER_CIPHER_SUITE
} SingleDirectionTestSelector;

static uint16_t BogusCipherSuite = GG_TLS_RSA_WITH_NULL_MD5;

static void
TestSingleDirection(bool                        client_to_server,
                    SingleDirectionTestSelector test,
                    const uint16_t*             cipher_suites,
                    size_t                      cipher_suites_count) {
    GG_Result result;

    // create a timer scheduler
    GG_TimerScheduler* timer_scheduler = NULL;
    result = GG_TimerScheduler_Create(&timer_scheduler);
    CHECK_EQUAL(GG_SUCCESS, result);

    // create a DTLS client
    GG_DtlsProtocol* dtls_client = NULL;
    GG_TlsClientOptions client_options = {
        .base = {
            .cipher_suites       = test == SD_TEST_BOGUS_CLIENT_CIPHER_SUITE ? &BogusCipherSuite : cipher_suites,
            .cipher_suites_count = test == SD_TEST_BOGUS_CLIENT_CIPHER_SUITE ? 1 : cipher_suites_count
        },
        .psk_identity      = test == SD_TEST_WRONG_CLIENT_KEY_IDENTITY ? BOGUS_PSK_IDENTITY : PSK_IDENTITY,
        .psk_identity_size = test == SD_TEST_WRONG_CLIENT_KEY_IDENTITY ?
                                     sizeof(BOGUS_PSK_IDENTITY) :
                                     sizeof(PSK_IDENTITY),
        .psk               = test == SD_TEST_WRONG_CLIENT_KEY ? BOGUS_PSK : PSK,
        .psk_size          = test == SD_TEST_WRONG_CLIENT_KEY ? sizeof(BOGUS_PSK) : sizeof(PSK),
        .ticket            = NULL,
        .ticket_size       = 0
    };
    result = GG_DtlsProtocol_Create(GG_TLS_ROLE_CLIENT,
                                    &client_options.base,
                                    1024,
                                    timer_scheduler,
                                    &dtls_client);
    CHECK_EQUAL(GG_SUCCESS, result);

    // init the key resolver
    GG_SET_INTERFACE(&PskResolver, StaticPskResolver, GG_TlsKeyResolver);
    PskResolver.psk_identity      = test == SD_TEST_WRONG_SERVER_KEY_IDENTITY ? BOGUS_PSK_IDENTITY : PSK_IDENTITY;
    PskResolver.psk_identity_size = test == SD_TEST_WRONG_SERVER_KEY_IDENTITY ?
                                            sizeof(BOGUS_PSK_IDENTITY) :
                                            sizeof(PSK_IDENTITY);
    PskResolver.psk               = test == SD_TEST_WRONG_SERVER_KEY ? BOGUS_PSK : PSK;
    PskResolver.psk_size          = test == SD_TEST_WRONG_SERVER_KEY ? sizeof(BOGUS_PSK) : sizeof(PSK);

    // create a DTLS server
    GG_DtlsProtocol* dtls_server = NULL;
    GG_TlsServerOptions server_options = {
        .base = {
            .cipher_suites       = test == SD_TEST_BOGUS_SERVER_CIPHER_SUITE ? &BogusCipherSuite : cipher_suites,
            .cipher_suites_count = test == SD_TEST_BOGUS_SERVER_CIPHER_SUITE ? 1 : cipher_suites_count
        },
        .key_resolver = GG_CAST(&PskResolver, GG_TlsKeyResolver)
    };
    result = GG_DtlsProtocol_Create(GG_TLS_ROLE_SERVER,
                                    &server_options.base,
                                    1024,
                                    timer_scheduler,
                                    &dtls_server);
    CHECK_EQUAL(GG_SUCCESS, result);

    // create an async pipe to connect the client transport to the server transport
    GG_AsyncPipe* client_to_server_pipe = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler, 1, &client_to_server_pipe);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* server_to_client_pipe = NULL;
    result = GG_AsyncPipe_Create(timer_scheduler, 1, &server_to_client_pipe);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect the transport
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetTransportSideAsDataSource(dtls_client),
                              GG_AsyncPipe_AsDataSink(client_to_server_pipe));
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetTransportSideAsDataSource(dtls_server),
                              GG_AsyncPipe_AsDataSink(server_to_client_pipe));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_to_server_pipe),
                              GG_DtlsProtocol_GetTransportSideAsDataSink(dtls_server));
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(server_to_client_pipe),
                              GG_DtlsProtocol_GetTransportSideAsDataSink(dtls_client));

    // create a memory data source for a message to send, by chunks of 1 byte
    const char* msg1 = "hello1";
    GG_StaticBuffer msg1_buffer;
    GG_StaticBuffer_Init(&msg1_buffer, (const uint8_t*)msg1, strlen(msg1));
    GG_MemoryDataSource* msg1_source = NULL;
    result = GG_MemoryDataSource_Create(GG_StaticBuffer_AsBuffer(&msg1_buffer), 1, &msg1_source);
    CHECK_EQUAL(GG_SUCCESS, result);

    // connect the source as the input for the DTLS client or server
    GG_DataSource_SetDataSink(GG_MemoryDataSource_AsDataSource(msg1_source),
                              GG_DtlsProtocol_GetUserSideAsDataSink(client_to_server ?
                                                                    dtls_client :
                                                                    dtls_server));

    // setup a sink for the server
    memset(&TestSink, 0, sizeof(TestSink));
    GG_SET_INTERFACE(&TestSink, VerifierSink, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetUserSideAsDataSource(client_to_server ?
                                                                      dtls_server :
                                                                      dtls_client),
                              GG_CAST(&TestSink, GG_DataSink));

    // start the handshake
    GG_DtlsProtocol_StartHandshake(dtls_server);
    GG_DtlsProtocol_StartHandshake(dtls_client);

    // start writing the data
    result = GG_MemoryDataSource_Start(msg1_source);

    // run the timer manually for a while
    for (uint32_t now = 0; now < 100; now++) {
        GG_TimerScheduler_SetTime(timer_scheduler, now);
    }

    // get the status
    GG_DtlsProtocolStatus client_status;
    GG_DtlsProtocolStatus server_status;
    GG_DtlsProtocol_GetStatus(dtls_client, &client_status);
    GG_DtlsProtocol_GetStatus(dtls_server, &server_status);

    switch (test) {
        case SD_TEST_NORMAL:
            // check that we got what was sent
            CHECK_EQUAL(strlen(msg1), TestSink.bytes_received);
            CHECK_EQUAL(strlen(msg1), TestSink.buffers_received);
            MEMCMP_EQUAL(msg1, TestSink.bytes, TestSink.bytes_received);
            break;

        case SD_TEST_WRONG_CLIENT_KEY:
            CHECK_EQUAL(GG_TLS_STATE_ERROR, client_status.state);
            break;

        case SD_TEST_WRONG_SERVER_KEY:
            CHECK_EQUAL(GG_TLS_STATE_ERROR, client_status.state);
            break;

        case SD_TEST_WRONG_CLIENT_KEY_IDENTITY:
            CHECK_EQUAL(GG_TLS_STATE_ERROR, client_status.state);
            break;

        case SD_TEST_WRONG_SERVER_KEY_IDENTITY:
            CHECK_EQUAL(GG_TLS_STATE_ERROR, client_status.state);
            break;

        case SD_TEST_BOGUS_CLIENT_CIPHER_SUITE:
            CHECK_EQUAL(GG_TLS_STATE_ERROR, client_status.state);
            break;

        case SD_TEST_BOGUS_SERVER_CIPHER_SUITE:
            CHECK_EQUAL(GG_TLS_STATE_ERROR, client_status.state);
            break;
    }

    // cleanup
    GG_DataSource_SetDataSink(GG_MemoryDataSource_AsDataSource(msg1_source), NULL);
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetUserSideAsDataSource(
        client_to_server ? dtls_server : dtls_client), NULL);
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetTransportSideAsDataSource(dtls_client), NULL);
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetTransportSideAsDataSource(dtls_server), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(client_to_server_pipe), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(server_to_client_pipe), NULL);
    GG_DtlsProtocol_Destroy(dtls_client);
    GG_DtlsProtocol_Destroy(dtls_server);
    GG_AsyncPipe_Destroy(client_to_server_pipe);
    GG_AsyncPipe_Destroy(server_to_client_pipe);
    GG_MemoryDataSource_Destroy(msg1_source);
    GG_TimerScheduler_Destroy(timer_scheduler);
}

//----------------------------------------------------------------------
TEST(GG_DTLS, Test_Dtls_DEFAULT_CIPHERS) {
    // test with the default cipher suites
    TestSingleDirection(true, SD_TEST_NORMAL, NULL, 0);
    TestSingleDirection(false, SD_TEST_NORMAL, NULL, 0);
}


TEST(GG_DTLS, Test_Dtls_TLS_PSK_WITH_AES_128_CCM_8) {
    // test with TLS_PSK_WITH_AES_128_CCM_8
    uint16_t cipher_suite_1 = GG_TLS_PSK_WITH_AES_128_CCM_8;
    TestSingleDirection(true, SD_TEST_NORMAL, &cipher_suite_1, 1);
    TestSingleDirection(false, SD_TEST_NORMAL, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_TLS_PSK_WITH_AES_128_CCM) {
    // test with TLS_PSK_WITH_AES_128_CCM
    uint16_t cipher_suite_1 = GG_TLS_PSK_WITH_AES_128_CCM;
    TestSingleDirection(true, SD_TEST_NORMAL, &cipher_suite_1, 1);
    TestSingleDirection(false, SD_TEST_NORMAL, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA) {
    // test with TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA
    uint16_t cipher_suite_1 = GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA;
    TestSingleDirection(true, SD_TEST_NORMAL, &cipher_suite_1, 1);
    TestSingleDirection(false, SD_TEST_NORMAL, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_GG_TLS_PSK_WITH_AES_128_CCM_8) {
    // test with TLS_PSK_WITH_AES_128_CCM_8
    uint16_t cipher_suite_1 = GG_TLS_PSK_WITH_AES_128_CCM_8;
    TestSingleDirection(true, SD_TEST_NORMAL, &cipher_suite_1, 1);
    TestSingleDirection(false, SD_TEST_NORMAL, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_WRONG_CLIENT_KEY) {
    uint16_t cipher_suite_1 = GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA;
    TestSingleDirection(false, SD_TEST_WRONG_CLIENT_KEY, &cipher_suite_1, 1);
    TestSingleDirection(true, SD_TEST_WRONG_CLIENT_KEY, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_WRONG_SERVER_KEY) {
    uint16_t cipher_suite_1 = GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA;
    TestSingleDirection(false, SD_TEST_WRONG_SERVER_KEY, &cipher_suite_1, 1);
    TestSingleDirection(true, SD_TEST_WRONG_SERVER_KEY, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_WRONG_CLIENT_KEY_IDENTITY) {
    uint16_t cipher_suite_1 = GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA;
    TestSingleDirection(false, SD_TEST_WRONG_CLIENT_KEY_IDENTITY, &cipher_suite_1, 1);
    TestSingleDirection(true, SD_TEST_WRONG_CLIENT_KEY_IDENTITY, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_WRONG_SERVER_KEY_IDENTITY) {
    uint16_t cipher_suite_1 = GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA;
    TestSingleDirection(false, SD_TEST_WRONG_SERVER_KEY_IDENTITY, &cipher_suite_1, 1);
    TestSingleDirection(true, SD_TEST_WRONG_SERVER_KEY_IDENTITY, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_BOGUS_CLIENT_CIPHER_SUITE) {
    uint16_t cipher_suite_1 = GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA;
    TestSingleDirection(false, SD_TEST_BOGUS_CLIENT_CIPHER_SUITE, &cipher_suite_1, 1);
    TestSingleDirection(true, SD_TEST_BOGUS_CLIENT_CIPHER_SUITE, &cipher_suite_1, 1);
}

TEST(GG_DTLS, Test_Dtls_BOGUS_SERVER_CIPHER_SUITE) {
    uint16_t cipher_suite_1 = GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA;
    TestSingleDirection(false, SD_TEST_BOGUS_SERVER_CIPHER_SUITE, &cipher_suite_1, 1);
    TestSingleDirection(true, SD_TEST_BOGUS_SERVER_CIPHER_SUITE, &cipher_suite_1, 1);
}
