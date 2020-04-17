// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_buffer.h"
#include "xp/common/gg_port.h"

TEST_GROUP(GG_BUFFER)
{
  void setup(void) {
  }

  void teardown(void) {
  }
};

TEST(GG_BUFFER, Test_StaticBuffer_1) {
    GG_StaticBuffer static_buffer;
    uint8_t data[] = "Hello world";

    GG_StaticBuffer_Init(&static_buffer, data, sizeof(data));

    /* test a few basic functions */
    GG_Buffer *buffer = GG_StaticBuffer_AsBuffer(&static_buffer);
    size_t length = GG_Buffer_GetDataSize(buffer);
    CHECK_EQUAL(sizeof(data), length);

    const uint8_t* new_buffer_ptr = GG_Buffer_GetData(buffer);
    STRCMP_EQUAL((const char *)data, (const char *)new_buffer_ptr);
}

TEST(GG_BUFFER, Test_DynamicBuffer_SetData) {
    GG_DynamicBuffer *buf1 = NULL;
    GG_DynamicBuffer *buf2 = NULL;
    uint8_t data[] = "hello world";

    CHECK(GG_DynamicBuffer_Equals(buf1, GG_DynamicBuffer_AsBuffer(buf2)));

    CHECK_EQUAL(GG_SUCCESS, GG_DynamicBuffer_Create(0, &buf1));
    CHECK_EQUAL(0, GG_DynamicBuffer_GetBufferSize(buf1));
    CHECK(!GG_DynamicBuffer_Equals(buf1, GG_DynamicBuffer_AsBuffer(buf2)));

    // Set data
    CHECK_EQUAL(GG_SUCCESS, GG_DynamicBuffer_SetData(buf1, data, sizeof(data)));
    CHECK_EQUAL(sizeof(data), GG_DynamicBuffer_GetDataSize(buf1));

    // Clone buffer
    CHECK_EQUAL(GG_SUCCESS, GG_DynamicBuffer_Clone(buf1, &buf2));
    CHECK_EQUAL(sizeof(data), GG_DynamicBuffer_GetDataSize(buf2));
    CHECK(GG_DynamicBuffer_Equals(buf1, GG_DynamicBuffer_AsBuffer(buf2)));

    GG_DynamicBuffer_Release(buf1);
    GG_DynamicBuffer_Release(buf2);
}

TEST(GG_BUFFER, Test_DynamicBuffer_SetBuffer) {
    GG_DynamicBuffer *buf;
    uint8_t *extern_buf = NULL;
    uint8_t *extern_data = NULL;

    CHECK_EQUAL(GG_SUCCESS, GG_DynamicBuffer_Create(0, &buf));
    CHECK_EQUAL(0, GG_DynamicBuffer_GetBufferSize(buf));

    CHECK_EQUAL(GG_SUCCESS, GG_DynamicBuffer_SetBufferSize(buf, 0));
    CHECK_EQUAL(0, GG_DynamicBuffer_GetBufferSize(buf));

    // set to external buffer from local buffer
    CHECK_EQUAL(GG_SUCCESS, GG_DynamicBuffer_SetBuffer(buf, extern_buf, 0));
    CHECK_EQUAL(0, GG_DynamicBuffer_GetBufferSize(buf));

    CHECK_EQUAL(GG_ERROR_NOT_SUPPORTED, GG_DynamicBuffer_SetBufferSize(buf, 8));
    CHECK_EQUAL(0, GG_DynamicBuffer_GetBufferSize(buf));

    CHECK_EQUAL(GG_ERROR_OUT_OF_RESOURCES, GG_DynamicBuffer_SetData(buf, extern_data, 8));
    CHECK_EQUAL(0, GG_DynamicBuffer_GetDataSize(buf));

    CHECK_EQUAL(GG_ERROR_NOT_SUPPORTED, GG_DynamicBuffer_SetDataSize(buf, 8));
    CHECK_EQUAL(0, GG_DynamicBuffer_GetDataSize(buf));

    GG_DynamicBuffer_Release(buf);
}
