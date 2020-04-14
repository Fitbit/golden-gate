/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_common.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_BITSTREAM)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

typedef struct {
    unsigned int bit_count;
    uint32_t     bits;
} TestBitstreamElement;

static TestBitstreamElement TestBits[] = {
    { 17, 0x4495 },
    {  4, 0xd },
    { 26, 0x38a30a5 },
    { 13, 0x9c7 },
    {  2, 0x0 },
    {  2, 0x2 },
    { 31, 0x75d63644 },
    { 20, 0x4184a },
    { 28, 0x587d96a },
    { 27, 0x260f631 },
    {  4, 0x2 },
    { 19, 0x3fd96 },
    {  2, 0x1 },
    {  5, 0xc },
    {  6, 0x5 },
    {  1, 0x0 },
    { 16, 0x2970 },
    {  4, 0x7 },
    { 10, 0x3b9 },
    { 25, 0x19e175a },
    {  2, 0x2 },
    {  8, 0xb1 },
    { 13, 0x13e6 },
    { 15, 0x1696 },
    { 21, 0xc2667 },
    { 24, 0xcfed4b },
    { 13, 0x1d9f },
    { 19, 0x2582f },
    { 18, 0xc376 },
    { 14, 0x2aee },
    {  5, 0xd },
    { 22, 0x120806 },
    {  4, 0x2 },
    { 27, 0xac7a82 },
    { 30, 0x16286e24 },
    { 26, 0x374c30c },
    { 21, 0x774ad },
    { 21, 0x63c83 },
    {  6, 0x6 },
    { 18, 0xe633 },
    { 24, 0x5f8c8 },
    { 20, 0xada31 },
    {  4, 0x9 },
    { 23, 0x626318 },
    {  1, 0x0 },
    {  3, 0x4 },
    { 25, 0x1da54f8 },
    { 12, 0x791 },
    { 22, 0x26274d },
    { 19, 0x703c7 },
    { 27, 0x3714cbe },
    {  8, 0x2a },
    {  6, 0x34 },
    {  5, 0x3 },
    { 11, 0x5c7 },
    { 24, 0x5206c1 },
    { 27, 0x1586071 },
    { 22, 0x33388c },
    { 17, 0x1f1eb },
    { 14, 0x2b3 },
    { 32, 0x5464666c },
    {  5, 0x17 },
    { 10, 0x206 },
    { 30, 0x1d14d0f6 },
    { 29, 0x79144a2 },
    {  8, 0x89 },
    { 24, 0x8d57da },
    { 15, 0x965 },
    { 11, 0xb8 },
    { 25, 0x38e039 },
    { 28, 0x5bc2c46 },
    { 26, 0x245fa6 },
    { 15, 0x54c3 },
    {  2, 0x3 },
    {  2, 0x3 },
    { 24, 0xe7452e },
    {  6, 0x2f },
    { 22, 0x375543 },
    { 15, 0x760 },
    { 12, 0xc60 },
    {  3, 0x1 },
    { 10, 0x56 },
    { 23, 0x7b7158 },
    { 28, 0xf39321a },
    {  5, 0xb },
    { 14, 0x531 },
    {  6, 0x2d },
    {  7, 0x7 },
    {  4, 0xe },
    { 12, 0x51e },
    {  6, 0x1d },
    {  3, 0x4 },
    { 30, 0x221aebcd },
    { 11, 0x7a3 },
    { 27, 0x2a26d0c },
    {  9, 0x134 },
    { 15, 0x42f8 },
    {  4, 0x4 },
    { 10, 0x2d3 },
    {  9, 0x1b1 }
};

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_WriteThenRead) {
    GG_BitOutputStream bits_o;
    static uint8_t buffer[401];
    GG_BitOutputStream_Init(&bits_o, buffer, sizeof(buffer));

    size_t bit_count = 0;
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(TestBits); i++) {
        GG_BitOutputStream_Write(&bits_o, TestBits[i].bits, TestBits[i].bit_count);

        bit_count += TestBits[i].bit_count;
        LONGS_EQUAL(bit_count, GG_BitOutputStream_GetPosition(&bits_o));
    }
    GG_BitOutputStream_Flush(&bits_o);

    GG_BitInputStream bits_i;
    GG_BitInputStream_Init(&bits_i, buffer, sizeof(buffer));
    bit_count = 0;
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(TestBits); i++) {
        unsigned int value = GG_BitInputStream_Read(&bits_i, TestBits[i].bit_count);
        LONGS_EQUAL(TestBits[i].bits, value);

        bit_count += TestBits[i].bit_count;
        LONGS_EQUAL(bit_count, GG_BitInputStream_GetPosition(&bits_i));
    }
}

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_Peek) {
    GG_BitOutputStream bits_o;
    static uint8_t buffer[401];
    GG_BitOutputStream_Init(&bits_o, buffer, sizeof(buffer));

    for (unsigned int i = 0; i < GG_ARRAY_SIZE(TestBits); i++) {
        GG_BitOutputStream_Write(&bits_o, TestBits[i].bits, TestBits[i].bit_count);
    }
    GG_BitOutputStream_Flush(&bits_o);

    GG_BitInputStream bits_i;
    GG_BitInputStream_Init(&bits_i, buffer, sizeof(buffer));
    for (unsigned int i = 0; i < GG_ARRAY_SIZE(TestBits); i++) {
        unsigned int value1 = GG_BitInputStream_Peek(&bits_i, TestBits[i].bit_count);
        unsigned int value2 = GG_BitInputStream_Read(&bits_i, TestBits[i].bit_count);
        LONGS_EQUAL(TestBits[i].bits, value1);
        LONGS_EQUAL(value1, value2);
    }
}

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_ReadWriteAssymetric) {
    GG_BitOutputStream bits1;
    static uint8_t buffer1[401];
    GG_BitOutputStream_Init(&bits1, buffer1, sizeof(buffer1));

    for (unsigned int i = 0; i < GG_ARRAY_SIZE(TestBits); i++) {
        GG_BitOutputStream_Write(&bits1, TestBits[i].bits, TestBits[i].bit_count);
    }
    GG_BitOutputStream_Flush(&bits1);

    GG_BitInputStream bits2_i;
    GG_BitOutputStream bits2_o;
    static uint8_t buffer2[401];
    GG_BitInputStream_Init(&bits2_i, buffer1, sizeof(buffer1));
    GG_BitOutputStream_Init(&bits2_o, buffer2, sizeof(buffer2));
    for (unsigned int i = 1; i < 33; i++) {
        GG_BitOutputStream_Reset(&bits2_o);
        GG_BitInputStream_Reset(&bits2_i);
        size_t bit_count = 0;
        while (bit_count + i <= 8 * sizeof(buffer1)) {
            unsigned int value = GG_BitInputStream_Read(&bits2_i, i);
            GG_BitOutputStream_Write(&bits2_o, value, i);
            bit_count += i;
        }
        MEMCMP_EQUAL(buffer1, buffer2, (bit_count + 7)/8);
    }
}

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_ByteAlign) {
    GG_BitInputStream bits;
    uint8_t buffer[] = { 0x11, 0xD3, 0xEF };
    GG_BitInputStream_Init(&bits, buffer, sizeof(buffer));
    GG_BitInputStream_Read(&bits, 3);
    GG_BitInputStream_ByteAlign(&bits);
    unsigned int value = GG_BitInputStream_Read(&bits, 4);
    LONGS_EQUAL(0x0D, value);
    GG_BitInputStream_ByteAlign(&bits);
    value = GG_BitInputStream_Read(&bits, 4);
    LONGS_EQUAL(0x0E, value);
}

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_ReadBounds) {
    GG_BitInputStream bits;
    uint8_t buffer[] = { 0x11, 0xD3, 0xEF };
    GG_BitInputStream_Init(&bits, buffer, sizeof(buffer));
    GG_BitInputStream_Read(&bits, 3);
    GG_BitInputStream_ByteAlign(&bits);
    unsigned int value = GG_BitInputStream_Read(&bits, 4);
    LONGS_EQUAL(0x0D, value);
    GG_BitInputStream_ByteAlign(&bits);
    value = GG_BitInputStream_Read(&bits, 4);
    LONGS_EQUAL(0x0E, value);

    // check that zeros are produced if we try to read more than the buffer
    value = GG_BitInputStream_Read(&bits, 8);
    LONGS_EQUAL(0xF0, value);
}

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_WriteBounds) {
    // check that the bits are truncated if we try to write more than the buffer
    GG_BitOutputStream bits;
    uint8_t buffer[3];
    GG_BitOutputStream_Init(&bits, buffer, sizeof(buffer));
    GG_BitOutputStream_Write(&bits, 0x11223344, 32);
    GG_BitOutputStream_Flush(&bits);
    uint8_t expected[] = { 0x11, 0x22, 0x33 };
    MEMCMP_EQUAL(expected, buffer, sizeof(buffer))
}

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_MaxSize) {
    GG_BitInputStream bits_i;
    uint8_t b[] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08 };
    GG_BitInputStream_Init(&bits_i, b, sizeof(b));
    unsigned int v = GG_BitInputStream_Peek(&bits_i, 32);
    LONGS_EQUAL(0x01020304, v);
    v = GG_BitInputStream_Read(&bits_i, 32);
    LONGS_EQUAL(0x01020304, v);
    v = GG_BitInputStream_Peek(&bits_i, 32);
    LONGS_EQUAL(0x05060708, v);
    v = GG_BitInputStream_Read(&bits_i, 32);
    LONGS_EQUAL(0x05060708, v);

    uint8_t c[8];
    GG_BitOutputStream bits_o;
    GG_BitOutputStream_Init(&bits_o, c, sizeof(c));
    GG_BitOutputStream_Write(&bits_o, 0x01020304, 32);
    GG_BitOutputStream_Write(&bits_o, 0x05060708, 32);
    GG_BitOutputStream_Flush(&bits_o);
    MEMCMP_EQUAL(b, c, sizeof(b));
}

//----------------------------------------------------------------------
TEST(GG_BITSTREAM, Test_Flush) {
    uint8_t b[4] = {0, 0, 0, 0};
    GG_BitOutputStream bits;
    GG_BitOutputStream_Init(&bits, b, sizeof(b));
    GG_BitOutputStream_Write(&bits, 1, 1);
    LONGS_EQUAL(1, GG_BitOutputStream_GetPosition(&bits));
    GG_BitOutputStream_Flush(&bits);
    LONGS_EQUAL(8, GG_BitOutputStream_GetPosition(&bits));
    LONGS_EQUAL(0x80, b[0]);
}
