/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_ring_buffer.h"
#include "xp/common/gg_port.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_RING_BUFFER)
{
  void setup(void) {
  }

  void teardown(void) {
  }
};

#define BUFFER_SIZE 17

static uint32_t trivial_rand() {
    static uint32_t val = 1;
    val = (val * 16807) % 0xFFFFFFFF;

    return val;
}

static void
ReadChunk(GG_RingBuffer* buffer)
{
    static unsigned int total_read = 0;
    unsigned int        i;
    unsigned char       bytes[BUFFER_SIZE];
    unsigned int        chunk = trivial_rand() % BUFFER_SIZE;
    unsigned int        can_read = (unsigned int)GG_RingBuffer_GetAvailable(buffer);
    if (chunk > can_read) chunk = can_read;
    if (chunk == 0) return;

    /* peek at the chunk */
    for (unsigned int offset = 0; offset < chunk; offset++) {
        size_t bytes_read = GG_RingBuffer_Peek(buffer, bytes, offset, chunk-offset);
        CHECK_EQUAL(chunk-offset, bytes_read);

        /* check values */
        for (i = 0; i < chunk-offset; i++) {
            unsigned int index = total_read + i + offset;
            unsigned char expected = index & 0xFF;
            CHECK_EQUAL(expected, bytes[i]);
        }
    }

    /* read a chunk */
    size_t bytes_read = GG_RingBuffer_Read(buffer, bytes, chunk);
    CHECK_EQUAL(chunk, bytes_read);

    /* check values */
    for (i = 0; i < chunk; i++) {
        unsigned int index = total_read + i;
        unsigned char expected = index & 0xFF;
        CHECK_EQUAL(expected, bytes[i]);
    }
    total_read += chunk;
}

static void
WriteChunk(GG_RingBuffer* buffer)
{
    static unsigned int total_written = 0;
    unsigned char       bytes[BUFFER_SIZE];
    unsigned int        i;
    unsigned int        chunk = trivial_rand() % BUFFER_SIZE;
    unsigned int        can_write = (unsigned int)GG_RingBuffer_GetSpace(buffer);
    if (chunk > can_write) chunk = can_write;
    if (chunk == 0) return;

    /* generate buffer */
    for (i = 0; i < chunk; i++) {
        unsigned int index = total_written + i;
        bytes[i] = index & 0xFF;
    }

    /* write chunk */
    size_t bytes_written = GG_RingBuffer_Write(buffer, bytes, chunk);
    CHECK_EQUAL(chunk, bytes_written);
    total_written += chunk;
}

TEST(GG_RING_BUFFER, Test_RingBuffer_1) {
    GG_RingBuffer ring;
    uint8_t buffer[BUFFER_SIZE];

    GG_RingBuffer_Init(&ring, buffer, sizeof(buffer));

    /* test a few basic functions */
    GG_RingBuffer_Write(&ring, (const uint8_t*)"ab", 2);
    *ring.in = 'c';
    GG_RingBuffer_MoveIn(&ring, 1);
    CHECK_EQUAL(BUFFER_SIZE-3-1, GG_RingBuffer_GetSpace(&ring));
    CHECK_EQUAL(3, GG_RingBuffer_GetAvailable(&ring));
    CHECK_EQUAL('c', GG_RingBuffer_PeekByte(&ring, 2));
    CHECK_EQUAL('a', GG_RingBuffer_ReadByte(&ring));
    CHECK_EQUAL('b', GG_RingBuffer_ReadByte(&ring));
    CHECK_EQUAL('c', GG_RingBuffer_ReadByte(&ring));
    CHECK_EQUAL(0, GG_RingBuffer_GetAvailable(&ring));
    CHECK_EQUAL(BUFFER_SIZE-1, GG_RingBuffer_GetSpace(&ring));
    CHECK_EQUAL(BUFFER_SIZE-3, GG_RingBuffer_GetContiguousSpace(&ring));

    GG_RingBuffer_Reset(&ring);
    for (unsigned int i = 0; i < 1000000; i++) {
        WriteChunk(&ring);
        ReadChunk(&ring);
    }
}
