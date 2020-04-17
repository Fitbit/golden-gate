// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_io.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_buffer.h"
#include "xp/common/gg_memory.h"
#include "xp/coap/gg_coap_message.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_MESSAGE_MIN_SIZE 4
#define GG_COAP_MESSAGE_TKL(byte) ((byte & 0xF0) >> 4)

/*----------------------------------------------------------------------
|   tests
+---------------------------------------------------------------------*/
TEST_GROUP(GG_COAP) {
    void setup(void) {
    }

    void teardown(void) {
    }
};

// message 1
static uint8_t coap_message_1[] = {
    0x44, 0x01, 0x9a, 0x1d, 0xc7, 0x47, 0xdb, 0xbb, 0xb6, 0x66, 0x6f, 0x6f,
    0x62, 0x61, 0x72
};
static unsigned int coap_message_1_size = 15;

static uint8_t coap_message_1_copy[15];

// message 2
static uint8_t coap_message_2[] = {
    0x44, 0x02, 0x50, 0x49, 0x0a, 0x25, 0x5c, 0x97, 0xb6, 0x61, 0x62, 0x63,
    0x64, 0x65, 0x66, 0xff, 0x68, 0x65, 0x6c, 0x6c, 0x6f
};
static unsigned int coap_message_2_size = 21;

static uint8_t coap_message_2_copy[21];

TEST(GG_COAP, Test_BasicMessageParsing) {
    GG_StaticBuffer message_buffer;
    GG_Buffer* buffer = NULL;
    GG_CoapMessage* coap_message = NULL;
    GG_Result result;

    // parse and check message 1
    GG_StaticBuffer_Init(&message_buffer, coap_message_1, coap_message_1_size);
    result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&message_buffer), &coap_message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(coap_message != NULL);
    CHECK_EQUAL(GG_COAP_MESSAGE_TYPE_CON, GG_CoapMessage_GetType(coap_message));
    CHECK_EQUAL(GG_COAP_METHOD_GET, GG_CoapMessage_GetCode(coap_message));
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t token_size = GG_CoapMessage_GetToken(coap_message, token);
    CHECK_EQUAL(4, token_size);
    CHECK(token[0] == 0xc7 && token[1] == 0x47 && token[2] == 0xdb && token[3] == 0xbb);
    CHECK_EQUAL(0x9a1d, GG_CoapMessage_GetMessageId(coap_message));
    CHECK_EQUAL(0, GG_CoapMessage_GetPayloadSize(coap_message));
    result = GG_CoapMessage_ToDatagram(coap_message, &buffer);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(buffer != NULL);
    CHECK_EQUAL(GG_Buffer_GetDataSize(buffer), GG_Buffer_GetDataSize(GG_StaticBuffer_AsBuffer(&message_buffer)));
    MEMCMP_EQUAL(GG_Buffer_GetData(buffer),
                 GG_Buffer_GetData(GG_StaticBuffer_AsBuffer(&message_buffer)),
                 GG_Buffer_GetDataSize(buffer));
    GG_Buffer_Release(GG_StaticBuffer_AsBuffer(&message_buffer));
    GG_CoapMessage_Destroy(coap_message);

    // parse and check message 2
    GG_StaticBuffer_Init(&message_buffer, coap_message_2, coap_message_2_size);
    result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&message_buffer), &coap_message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(coap_message != NULL);
    CHECK_EQUAL(GG_COAP_MESSAGE_TYPE_CON, GG_CoapMessage_GetType(coap_message));
    CHECK_EQUAL(GG_COAP_METHOD_POST, GG_CoapMessage_GetCode(coap_message));
    GG_Buffer_Release(GG_StaticBuffer_AsBuffer(&message_buffer));
    GG_CoapMessage_Destroy(coap_message);
}

TEST(GG_COAP, Test_CheckInvalidMessageFormat) {
    GG_StaticBuffer message_buffer;
    GG_CoapMessage* coap_message = NULL;
    GG_Result result;

    // test with truncated coap message
    for (unsigned int len = 0; len < coap_message_1_size; len++) {
        GG_StaticBuffer_Init(&message_buffer, coap_message_1_copy, len);
        result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&message_buffer), &coap_message);
        if (len == (unsigned int)(GG_COAP_MESSAGE_TKL(coap_message_1_copy[0]) + GG_COAP_MESSAGE_MIN_SIZE)) {
            CHECK_EQUAL(GG_SUCCESS, result);
        } else {
            CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);
        }

        GG_CoapMessage_Destroy(coap_message);
        coap_message_1_copy[len] = coap_message_1[len];
    }

    // set invalid version
    coap_message_1_copy[0] = coap_message_1_copy[0] & 0x3F;
    GG_StaticBuffer_Init(&message_buffer, coap_message_1_copy, coap_message_1_size);
    result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&message_buffer), &coap_message);
    CHECK_EQUAL(GG_ERROR_COAP_UNSUPPORTED_VERSION, result);
    GG_CoapMessage_Destroy(coap_message);
    coap_message_1_copy[0] = coap_message_1[0];

    // set invalid token length
    coap_message_1_copy[0] = (coap_message_1_copy[0] & 0xF0) | 0x0F;
    GG_StaticBuffer_Init(&message_buffer, coap_message_1_copy, coap_message_1_size);
    result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&message_buffer), &coap_message);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);
    GG_CoapMessage_Destroy(coap_message);
    coap_message_1_copy[0] = coap_message_1[0];

    // set invalid option delta
    coap_message_1_copy[8] = 0xF0;
    GG_StaticBuffer_Init(&message_buffer, coap_message_1_copy, coap_message_1_size);
    result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&message_buffer), &coap_message);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);
    GG_CoapMessage_Destroy(coap_message);
    coap_message_1_copy[8] = coap_message_1[8];

    // set a marker followed by a zero-length payload
    memcpy(coap_message_2_copy, coap_message_2, coap_message_2_size);
    GG_StaticBuffer_Init(&message_buffer, coap_message_2_copy, 16);
    result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&message_buffer), &coap_message);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);
    GG_CoapMessage_Destroy(coap_message);
}

TEST(GG_COAP, Test_OptionsParsing) {
    GG_CoapMessage* message = NULL;
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                             GG_COAP_MESSAGE_TYPE_CON,
                                             NULL, 0, 0, NULL, 0, NULL, 0,
                                             &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(message != NULL);
    GG_CoapMessageOptionIterator iterator;
    GG_CoapMessage_InitOptionIterator(message, 0, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_NONE, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_EMPTY, iterator.option.type);
    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_NONE, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_EMPTY, iterator.option.type);
    GG_CoapMessage_Destroy(message);

    uint8_t etag[2] = { 0xa0, 0xa1 };
    uint8_t extended_error[GG_COAP_EXTENDED_ERROR_OPTION_SIZE] = {0};

    GG_CoapMessageOptionParam options[] = {
        GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(ETAG, etag, 2),
        GG_COAP_MESSAGE_OPTION_PARAM_EMPTY(IF_NONE_MATCH),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 8765),
        GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE_N(GG_COAP_MESSAGE_OPTION_IF_MATCH, NULL, 0),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_HOST, "blabla"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING_L(LOCATION_PATH, "x", 1),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_PATH, "y"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_PATH, "z"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foobar"),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(CONTENT_FORMAT, 12),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(MAX_AGE, 67),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_QUERY, "f=5"),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(ACCEPT, 100),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_QUERY, "g=7"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(PROXY_URI, "foo.bar.com/proxy"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING_N(GG_COAP_MESSAGE_OPTION_PROXY_SCHEME, "coaps"),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(SIZE1, 0),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(SIZE2, 300),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(START_OFFSET, 100),
        GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(EXTENDED_ERROR, extended_error, GG_COAP_EXTENDED_ERROR_OPTION_SIZE),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK1, 10000),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(BLOCK2, 0xE8781234),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT_N(250, 0),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT_N(500, 1),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT_N(6500, 0x12345678),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT_N(65535, 0x123456),
        GG_COAP_MESSAGE_OPTION_PARAM_EMPTY_N(10000)};

    uint8_t token[2] = { 0x12, 0x34 };
    result = GG_CoapMessage_Create(GG_COAP_METHOD_PUT,
                                   GG_COAP_MESSAGE_TYPE_NON,
                                   &options[0], 26, 1234, token, 2, NULL, 0,
                                   &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(message != NULL);

    GG_CoapMessage_InitOptionIterator(message, 0, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_IF_MATCH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, iterator.option.type);
    CHECK_EQUAL(0, iterator.option.value.opaque.size);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_URI_HOST, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(6, iterator.option.value.string.length);
    STRNCMP_EQUAL("blabla", iterator.option.value.string.chars, 6);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_ETAG, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, iterator.option.type);
    CHECK_EQUAL(2, iterator.option.value.opaque.size);
    MEMCMP_EQUAL(iterator.option.value.opaque.bytes, etag, 2);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_IF_NONE_MATCH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_EMPTY, iterator.option.type);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_URI_PORT, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(8765, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_LOCATION_PATH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(1, iterator.option.value.string.length);
    STRNCMP_EQUAL("x", iterator.option.value.string.chars, 1);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_LOCATION_PATH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(1, iterator.option.value.string.length);
    STRNCMP_EQUAL("y", iterator.option.value.string.chars, 1);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_LOCATION_PATH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(1, iterator.option.value.string.length);
    STRNCMP_EQUAL("z", iterator.option.value.string.chars, 1);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_URI_PATH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(6, iterator.option.value.string.length);
    STRNCMP_EQUAL("foobar", iterator.option.value.string.chars, 6);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_CONTENT_FORMAT, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(12, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_MAX_AGE, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(67, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_URI_QUERY, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(3, iterator.option.value.string.length);
    STRNCMP_EQUAL("f=5", iterator.option.value.string.chars, 3);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_ACCEPT, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(100, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_LOCATION_QUERY, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(3, iterator.option.value.string.length);
    STRNCMP_EQUAL("g=7", iterator.option.value.string.chars, 3);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_BLOCK2, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(0xE8781234, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_BLOCK1, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(10000, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_SIZE2, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(300, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_PROXY_URI, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(17, iterator.option.value.string.length);
    STRNCMP_EQUAL("foo.bar.com/proxy", iterator.option.value.string.chars, 17);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_PROXY_SCHEME, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    CHECK_EQUAL(5, iterator.option.value.string.length);
    STRNCMP_EQUAL("coaps", iterator.option.value.string.chars, 5);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_SIZE1, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(0, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(250, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, iterator.option.type);
    CHECK_EQUAL(0, iterator.option.value.opaque.size);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(500, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, iterator.option.type);
    CHECK_EQUAL(1, iterator.option.value.opaque.size);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_START_OFFSET, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    CHECK_EQUAL(100, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_EXTENDED_ERROR, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, iterator.option.type);
    CHECK_EQUAL(GG_COAP_EXTENDED_ERROR_OPTION_SIZE, iterator.option.value.opaque.size);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(6500, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, iterator.option.type);
    CHECK_EQUAL(4, iterator.option.value.opaque.size);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(65535, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_OPAQUE, iterator.option.type);
    LONGS_EQUAL(3, iterator.option.value.opaque.size);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_NONE, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_EMPTY, iterator.option.type);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_NONE, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_EMPTY, iterator.option.type);

    // serialize and deserialize the message
    GG_Buffer* datagram = NULL;
    result = GG_CoapMessage_ToDatagram(message, &datagram);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_CoapMessage* message2 = NULL;
    result = GG_CoapMessage_CreateFromDatagram(datagram, &message2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_Buffer_Release(datagram);
    GG_CoapMessage_Destroy(message2);

    GG_CoapMessage_Destroy(message);
}

TEST(GG_COAP, Test_OptionsChaining) {
    GG_CoapMessageOptionParam options1[3] = {
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 8765),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_HOST, "blabla"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_PATH, "x")
    };
    CHECK_TRUE(options1[0].next == NULL);
    CHECK_TRUE(options1[1].next == NULL);
    CHECK_TRUE(options1[2].next == NULL);

    // those won't be used
    GG_CoapMessageOptionParam options2[2] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_PATH, "a"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_PATH, "b")
    };
    (void)options2;

    GG_CoapMessageOptionParam options3[2] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_PATH, "y"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(LOCATION_PATH, "z")
    };

    // chain options 1 and 3 together
    options1[2].next = &options3[0];

    GG_CoapMessage* message;
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_GET, GG_COAP_MESSAGE_TYPE_CON,
                                             options1, 5, 0, NULL, 0, NULL, 0, &message);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that we have all our options
    GG_CoapMessageOptionIterator iterator;
    GG_CoapMessage_InitOptionIterator(message, 0, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_URI_HOST, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    MEMCMP_EQUAL("blabla", iterator.option.value.string.chars, iterator.option.value.string.length);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_URI_PORT, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_UINT, iterator.option.type);
    LONGS_EQUAL(8765, iterator.option.value.uint);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_LOCATION_PATH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    MEMCMP_EQUAL("x", iterator.option.value.string.chars, iterator.option.value.string.length);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_LOCATION_PATH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    MEMCMP_EQUAL("y", iterator.option.value.string.chars, iterator.option.value.string.length);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_LOCATION_PATH, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_STRING, iterator.option.type);
    MEMCMP_EQUAL("z", iterator.option.value.string.chars, iterator.option.value.string.length);

    GG_CoapMessage_StepOptionIterator(message, &iterator);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_NONE, iterator.option.number);
    CHECK_EQUAL(GG_COAP_MESSAGE_OPTION_TYPE_EMPTY, iterator.option.type);

    GG_CoapMessage_Destroy(message);
}

TEST(GG_COAP, Test_BasicMessageCreation) {
    GG_CoapMessage* message = NULL;
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                             GG_COAP_MESSAGE_TYPE_CON,
                                             NULL,
                                             0,
                                             0,
                                             NULL,
                                             0,
                                             NULL,
                                             0,
                                             &message);

    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(message != NULL);
    CHECK_EQUAL(GG_COAP_METHOD_GET, GG_CoapMessage_GetCode(message));
    CHECK_EQUAL(GG_COAP_MESSAGE_TYPE_CON, GG_CoapMessage_GetType(message));
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    CHECK_EQUAL(0, GG_CoapMessage_GetToken(message, token));
    CHECK_EQUAL(0, GG_CoapMessage_GetPayloadSize(message));
    GG_CoapMessage_Destroy(message);

    uint8_t token_in[8] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, 0x11, 0x22};
    uint8_t payload_in[8] = {0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88};
    GG_CoapMessageOptionParam options[3] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "hello"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "world"),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 7891)
    };
    result = GG_CoapMessage_Create(GG_COAP_METHOD_PUT,
                                   GG_COAP_MESSAGE_TYPE_NON,
                                   &options[0],
                                   3,
                                   1234,
                                   token_in,
                                   2,
                                   payload_in,
                                   8,
                                   &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK(message != NULL);
    CHECK_EQUAL(GG_COAP_METHOD_PUT, GG_CoapMessage_GetCode(message));
    CHECK_EQUAL(GG_COAP_MESSAGE_TYPE_NON, GG_CoapMessage_GetType(message));
    CHECK_EQUAL(2, GG_CoapMessage_GetToken(message, token));
    MEMCMP_EQUAL(token_in, token, 2);
    CHECK_EQUAL(8, GG_CoapMessage_GetPayloadSize(message));
    MEMCMP_EQUAL(payload_in, GG_CoapMessage_GetPayload(message), 8);

    // serialize and deserialize the message
    GG_Buffer* datagram = NULL;
    result = GG_CoapMessage_ToDatagram(message, &datagram);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_CoapMessage* message2 = NULL;
    result = GG_CoapMessage_CreateFromDatagram(datagram, &message2);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_Buffer_Release(datagram);
    GG_CoapMessage_Destroy(message2);

    GG_CoapMessage_Destroy(message);
}

TEST(GG_COAP, Test_UsePayload) {
    GG_CoapMessage* message;
    uint8_t payload[4] = { 1, 2, 3, 4 };
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_PUT,
                                             GG_COAP_MESSAGE_TYPE_NON,
                                             NULL,
                                             0,
                                             0,
                                             NULL,
                                             0,
                                             payload,
                                             sizeof(payload),
                                             &message);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_Buffer* datagram;
    result = GG_CoapMessage_ToDatagram(message, &datagram);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(sizeof(payload), GG_CoapMessage_GetPayloadSize(message));
    uint8_t* message_payload = GG_CoapMessage_UsePayload(message);
    CHECK_TRUE(message_payload != NULL);

    // check that a message created from a read-only buffer can't be written to
    GG_CoapMessage* message2;
    GG_StaticBuffer static_datagram;
    GG_StaticBuffer_Init(&static_datagram, GG_Buffer_GetData(datagram), GG_Buffer_GetDataSize(datagram));
    result = GG_CoapMessage_CreateFromDatagram(GG_StaticBuffer_AsBuffer(&static_datagram), &message2);
    LONGS_EQUAL(GG_SUCCESS, result);
    message_payload = GG_CoapMessage_UsePayload(message2);
    CHECK_TRUE(message_payload == NULL);

    GG_Buffer_Release(datagram);
    GG_CoapMessage_Destroy(message);
    GG_CoapMessage_Destroy(message2);
}

TEST(GG_COAP, Test_ShortMessages) {
    GG_CoapMessage* message = NULL;
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                             GG_COAP_MESSAGE_TYPE_NON,
                                             NULL, 0,
                                             0,
                                             NULL, 0,
                                             NULL, 0,
                                             &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_Buffer* datagram = NULL;
    result = GG_CoapMessage_ToDatagram(message, &datagram);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, GG_Buffer_GetDataSize(datagram));
    GG_CoapMessage_Destroy(message);

    // re-parse the datagram
    result = GG_CoapMessage_CreateFromDatagram(datagram, &message);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(GG_COAP_METHOD_GET, GG_CoapMessage_GetCode(message));
    CHECK_EQUAL(GG_COAP_MESSAGE_TYPE_NON, GG_CoapMessage_GetType(message));
    uint8_t token[GG_COAP_MESSGAGE_MAX_TOKEN_LENGTH];
    size_t  token_size  = GG_CoapMessage_GetToken(message, token);
    CHECK_EQUAL(0, token_size);
    GG_CoapMessage_Destroy(message);

    // test that a datagram with an empty payload ending with a 0xFF marker is invalid
    GG_DynamicBuffer* bogus = NULL;
    result = GG_DynamicBuffer_Create(5, &bogus);
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_DynamicBuffer_SetData(bogus, GG_Buffer_GetData(datagram), 4);
    GG_DynamicBuffer_SetDataSize(bogus, 5);
    GG_DynamicBuffer_UseData(bogus)[4] = 0xFF;
    result = GG_CoapMessage_CreateFromDatagram(GG_DynamicBuffer_AsBuffer(bogus), &message);
    GG_Buffer_Release(datagram);
    GG_DynamicBuffer_Release(bogus);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);
}

TEST(GG_COAP, Test_OptionParamsOrdering) {
    uint8_t etag[3] = {1, 2, 3};
    GG_CoapMessageOptionParam options[5] = {
        GG_COAP_MESSAGE_OPTION_PARAM_EMPTY(IF_NONE_MATCH),
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 5683),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "hello"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING_L(URI_PATH, "bye bye", 7),
        GG_COAP_MESSAGE_OPTION_PARAM_OPAQUE(ETAG, etag, sizeof(etag))
    };

    GG_CoapMessageOptionParam options2[2] = {
        GG_COAP_MESSAGE_OPTION_PARAM_UINT(URI_PORT, 5684),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foobar")
    };

    // chain options
    options[4].next = &options2[0];

    // create a CoAP message. This will internally order the options, but should leave
    // the original non-sorted linkage intact.
    uint8_t token[1] = { 0 };
    GG_CoapMessage* message = NULL;
    GG_Result result = GG_CoapMessage_Create(GG_COAP_METHOD_GET,
                                             GG_COAP_MESSAGE_TYPE_CON,
                                             options,
                                             7,
                                             0,
                                             token,
                                             sizeof(token),
                                             NULL,
                                             0,
                                             &message);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_Buffer* datagram = NULL;
    result = GG_CoapMessage_ToDatagram(message, &datagram);
    LONGS_EQUAL(GG_SUCCESS, result);

    POINTERS_EQUAL(NULL, options[0].next);
    POINTERS_EQUAL(NULL, options[1].next);
    POINTERS_EQUAL(NULL, options[2].next);
    POINTERS_EQUAL(NULL, options[3].next);
    POINTERS_EQUAL(&options2[0], options[4].next);
    POINTERS_EQUAL(NULL, options2[0].next);
    POINTERS_EQUAL(NULL, options2[1].next);

    GG_CoapMessage_Destroy(message);
}
