/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_utils.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_crc32.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_UTILS)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

//----------------------------------------------------------------------
TEST(GG_UTILS, Test_Base64_Decode) {
    uint8_t buffer[64];
    size_t buffer_size = 0;
    GG_Result result = GG_Base64_Decode("0", 1, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    result = GG_Base64_Decode("a===", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    result = GG_Base64_Decode("!!!!", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    result = GG_Base64_Decode("aaaa=a==", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    buffer_size = 1;
    result = GG_Base64_Decode("+a==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);

    buffer_size = 1;
    result = GG_Base64_Decode("/a==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);

    buffer_size = 1;
    result = GG_Base64_Decode("+a==", 0, buffer, &buffer_size, true);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    buffer_size = 1;
    result = GG_Base64_Decode("/a==", 0, buffer, &buffer_size, true);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    buffer_size = 1;
    result = GG_Base64_Decode("-a==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    buffer_size = 1;
    result = GG_Base64_Decode("_a==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_INVALID_FORMAT, result);

    buffer_size = 1;
    result = GG_Base64_Decode("-a==", 0, buffer, &buffer_size, true);
    CHECK_EQUAL(GG_SUCCESS, result);

    buffer_size = 1;
    result = GG_Base64_Decode("_a==", 0, buffer, &buffer_size, true);
    CHECK_EQUAL(GG_SUCCESS, result);

    buffer_size = 0;
    result = GG_Base64_Decode("", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);

    buffer_size = 0;
    result = GG_Base64_Decode("Zg==", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(1, buffer_size);
    result = GG_Base64_Decode("Zg==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("f", buffer, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Decode("Zm8=\n", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(2, buffer_size);
    result = GG_Base64_Decode("Zm8=\n", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("fo", buffer, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Decode("Zm9v", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(3, buffer_size);
    result = GG_Base64_Decode("Zm9v", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("foo", buffer, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Decode("Zm9vYg", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(4, buffer_size);
    result = GG_Base64_Decode("Zm9vYg==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("foob", buffer, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Decode("Zm9vYmE==", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(5, buffer_size);
    result = GG_Base64_Decode("Zm9vYmE==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("fooba", buffer, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Decode("Zm9vYmFy", 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(6, buffer_size);
    result = GG_Base64_Decode("Zm9vYmFy", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("foobar", buffer, buffer_size);

    buffer_size = 6;
    result = GG_Base64_Decode("\nZm9vYmFy", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("foobar", buffer, buffer_size);

    buffer_size = 6;
    result = GG_Base64_Decode("Zm9vYmFy\n", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("foobar", buffer, buffer_size);

    buffer_size = 6;
    result = GG_Base64_Decode("Zm\n9v\r\nYm\rFy\n", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("foobar", buffer, buffer_size);

    buffer_size = 1;
    result = GG_Base64_Decode("+/==", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("\xfb", buffer, buffer_size);

    buffer_size = 1;
    result = GG_Base64_Decode("-_==", 0, buffer, &buffer_size, true);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("\xfb", buffer, buffer_size);
}

TEST(GG_UTILS, Test_Base64_Encode) {
    size_t buffer_size;
    char buffer[64];
    uint8_t data[] = {0, 1, 2, 3, 4, 5, 6, 7};
    GG_Result result;

    buffer_size = 0;
    result = GG_Base64_Encode(data, 0, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(0, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Encode(data, 1, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(4, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Encode(data, 2, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(4, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Encode(data, 3, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(4, buffer_size);

    buffer_size = 0;
    result = GG_Base64_Encode(data, 4, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(8, buffer_size);

    // RFC tests

    // BASE64("") = ""
    buffer_size = 0;
    result = GG_Base64_Encode((const uint8_t*)"", 0, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(0, buffer_size);

    // BASE64("f") = "Zg=="
    buffer_size = 0;
    result = GG_Base64_Encode((const uint8_t*)"f", 1, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(4, buffer_size);
    result = GG_Base64_Encode((const uint8_t*)"f", 1, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, buffer_size);
    STRNCMP_EQUAL("Zg==", buffer, buffer_size);

    // BASE64("fo") = "Zm8="
    buffer_size = 0;
    result = GG_Base64_Encode((const uint8_t*)"fo", 2, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(4, buffer_size);
    result = GG_Base64_Encode((const uint8_t*)"fo", 2, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, buffer_size);
    STRNCMP_EQUAL("Zm8=", buffer, buffer_size);

    // BASE64("foo") = "Zm9v"
    buffer_size = 0;
    result = GG_Base64_Encode((const uint8_t*)"foo", 3, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(4, buffer_size);
    result = GG_Base64_Encode((const uint8_t*)"foo", 3, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, buffer_size);
    STRNCMP_EQUAL("Zm9v", buffer, buffer_size);

    // BASE64("foob") = "Zm9vYg=="
    buffer_size = 0;
    result = GG_Base64_Encode((const uint8_t*)"foob", 4, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(8, buffer_size);
    result = GG_Base64_Encode((const uint8_t*)"foob", 4, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(8, buffer_size);
    STRNCMP_EQUAL("Zm9vYg==", buffer, buffer_size);

    // BASE64("fooba") = "Zm9vYmE="
    buffer_size = 0;
    result = GG_Base64_Encode((const uint8_t*)"fooba", 5, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(8, buffer_size);
    result = GG_Base64_Encode((const uint8_t*)"fooba", 5, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(8, buffer_size);
    STRNCMP_EQUAL("Zm9vYmE=", buffer, buffer_size);

    // BASE64("foobar") = "Zm9vYmFy"
    buffer_size = 0;
    result = GG_Base64_Encode((const uint8_t*)"foobar", 6, NULL, &buffer_size, false);
    CHECK_EQUAL(GG_ERROR_NOT_ENOUGH_SPACE, result);
    CHECK_EQUAL(8, buffer_size);
    result = GG_Base64_Encode((const uint8_t*)"foobar", 6, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(8, buffer_size);
    STRNCMP_EQUAL("Zm9vYmFy", buffer, buffer_size);

    // Safe URL remapping
    buffer_size = 4;
    result = GG_Base64_Encode((const uint8_t*)"\xfb", 1, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, buffer_size);
    STRNCMP_EQUAL("+w==", buffer, buffer_size);

    buffer_size = 4;
    result = GG_Base64_Encode((const uint8_t*)"\xfb", 1, buffer, &buffer_size, true);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, buffer_size);
    STRNCMP_EQUAL("-w==", buffer, buffer_size);

    buffer_size = 4;
    result = GG_Base64_Encode((const uint8_t*)"\xff\xec", 2, buffer, &buffer_size, false);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, buffer_size);
    STRNCMP_EQUAL("/+w=", buffer, buffer_size);

    buffer_size = 4;
    result = GG_Base64_Encode((const uint8_t*)"\xff\xec", 2, buffer, &buffer_size, true);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(4, buffer_size);
    STRNCMP_EQUAL("_-w=", buffer, buffer_size);
}

//----------------------------------------------------------------------
TEST(GG_UTILS, Test_NibbleToHex) {
    char result;
    result = GG_NibbleToHex(3, false);
    CHECK_EQUAL('3', result);

    result = GG_NibbleToHex(3, true);
    CHECK_EQUAL('3', result);

    result = GG_NibbleToHex(10, false);
    CHECK_EQUAL('a', result);

    result = GG_NibbleToHex(10, true);
    CHECK_EQUAL('A', result);
}

TEST(GG_UTILS, Test_HexToNibble) {
    int result;
    result = GG_HexToNibble('A');
    CHECK_EQUAL(10, result);

    result = GG_HexToNibble('a');
    CHECK_EQUAL(10, result);

    result = GG_HexToNibble('4');
    CHECK_EQUAL(4, result);

    result = GG_HexToNibble('-');
    CHECK_EQUAL(-1, result);
}

TEST(GG_UTILS, Test_ByteToHex) {
    char buffer[4] = {0};
    GG_ByteToHex(0xab, buffer, false);
    STRNCMP_EQUAL("ab", buffer, 2);
    MEMCMP_EQUAL("ab\0\0", buffer, 4); // Overflow

    GG_ByteToHex(0xab, buffer, true);
    STRNCMP_EQUAL("AB", buffer, 2);

    GG_ByteToHex(0x3f, buffer, true);
    STRNCMP_EQUAL("3F", buffer, 2);
}

TEST(GG_UTILS, Test_HexToByte) {
    GG_Result result;
    uint8_t byte;
    result = GG_HexToByte("AB", &byte);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(0xab, byte);

    result = GG_HexToByte("ab", &byte);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(0xab, byte);

    result = GG_HexToByte("3f", &byte);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(0x3f, byte);

    result = GG_HexToByte("03", &byte);
    CHECK_EQUAL(GG_SUCCESS, result);
    CHECK_EQUAL(0x03, byte);

    result = GG_HexToByte("3", &byte);
    CHECK_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_HexToByte("-3", &byte);
    CHECK_EQUAL(GG_ERROR_INVALID_SYNTAX, result);
}

TEST(GG_UTILS, Test_BytesToHex) {
    char hex[8] = {0};
    GG_BytesToHex((const uint8_t*)"\xab\x03\x3f", 3, hex, false);
    STRNCMP_EQUAL("ab033f", hex, 6);
    MEMCMP_EQUAL("ab033f\0\0", hex, 8);

    GG_BytesToHex((const uint8_t*)"\xab\x03\x3f", 3, hex, true);
    STRNCMP_EQUAL("AB033F", hex, 6);

    GG_BytesToHex((const uint8_t*)"\xab", 1, hex, true);
    STRNCMP_EQUAL("AB", hex, 2);
}

TEST(GG_UTILS, Test_BytesToHexString) {
    GG_String hex = GG_String_Create("");
    GG_BytesToHexString((const uint8_t*)"\xab\x03\x3f", 3, &hex, false);
    STRNCMP_EQUAL("ab033f", GG_CSTR(hex), 6);
    MEMCMP_EQUAL("ab033f\0", GG_CSTR(hex), 7);

    GG_BytesToHexString((const uint8_t*)"\xab\x03\x3f", 3, &hex, true);
    STRNCMP_EQUAL("AB033F", GG_CSTR(hex), 6);
    MEMCMP_EQUAL("AB033F\0", GG_CSTR(hex), 7);

    GG_BytesToHexString((const uint8_t*)"\xab", 1, &hex, true);
    STRNCMP_EQUAL("AB", GG_CSTR(hex), 2);
    MEMCMP_EQUAL("AB\0", GG_CSTR(hex), 3);
    GG_String_Destruct(&hex);
}

TEST(GG_UTILS, Test_HexToBytes) {
    GG_Result result;
    uint8_t bytes[4] = {0};
    // Null terminated string
    result = GG_HexToBytes("ab033f", 0, bytes);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("\xab\x03\x3f", bytes, 3);
    MEMCMP_EQUAL("\xab\x03\x3f\0", bytes, 3);

    result = GG_HexToBytes("ab033f---", 6, bytes);
    CHECK_EQUAL(GG_SUCCESS, result);
    MEMCMP_EQUAL("\xab\x03\x3f", bytes, 3);

    result = GG_HexToBytes("ab033", 0, bytes);
    CHECK_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);
}

TEST(GG_UTILS, Test_BytesFromInt16Be) {
    uint8_t buffer[4] = {0};
    GG_BytesFromInt16Be(buffer, 0xabcd);
    MEMCMP_EQUAL("\xab\xcd\0\0", buffer, 4);
}

TEST(GG_UTILS, Test_BytesFromInt16Le) {
    uint8_t buffer[4] = {0};
    GG_BytesFromInt16Le(buffer, 0xabcd);
    MEMCMP_EQUAL("\xcd\xab\0\0", buffer, 4);
}

TEST(GG_UTILS, Test_BytesFromInt32Be) {
    uint8_t buffer[6] = {0};
    GG_BytesFromInt32Be(buffer, 0xabcd1234);
    MEMCMP_EQUAL("\xab\xcd\x12\x34\0\0", buffer, 6);
}

TEST(GG_UTILS, Test_BytesFromInt32Le) {
    uint8_t buffer[6] = {0};
    GG_BytesFromInt32Le(buffer, 0xabcd1234);
    MEMCMP_EQUAL("\x34\x12\xcd\xab\0\0", buffer, 6);
}

TEST(GG_UTILS, Test_BytesFromInt64Be) {
    uint8_t buffer[9] = {0};
    GG_BytesFromInt64Be(buffer, 0x0123456789abcdef);
    MEMCMP_EQUAL("\x01\x23\x45\x67\x89\xab\xcd\xef\0", buffer, 9);
}

TEST(GG_UTILS, Test_BytesFromInt64Le) {
    uint8_t buffer[9] = {0};
    GG_BytesFromInt64Le(buffer, 0x0123456789abcdef);
    MEMCMP_EQUAL("\xef\xcd\xab\x89\x67\x45\x23\x01\0", buffer, 9);
}

TEST(GG_UTILS, Test_BytesToInt16Be) {
    uint16_t result;
    result = GG_BytesToInt16Be((const uint8_t *)"\xab\xcd");
    CHECK_EQUAL(0xabcd, result);
}

TEST(GG_UTILS, Test_BytesToInt16Le) {
    uint16_t result;
    result = GG_BytesToInt16Le((const uint8_t *)"\xab\xcd");
    CHECK_EQUAL(0xcdab, result);
}

TEST(GG_UTILS, Test_BytesToInt32Be) {
    uint32_t result;
    result = GG_BytesToInt32Be((const uint8_t *)"\xab\xcd\xef\x01");
    CHECK_EQUAL(0xabcdef01, result);
}

TEST(GG_UTILS, Test_BytesToInt32Le) {
    uint32_t result;
    result = GG_BytesToInt32Le((const uint8_t *)"\xab\xcd\xef\01");
    CHECK_EQUAL(0x01efcdab, result);
}

TEST(GG_UTILS, Test_BytesToInt64Be) {
    uint64_t result;
    result = GG_BytesToInt64Be((const uint8_t *)"\xab\xcd\xef\x01\x23\x45\x67\x89");
    CHECK_EQUAL(0xabcdef0123456789, result);
}

TEST(GG_UTILS, Test_BytesToInt64Le) {
    uint64_t result;
    result = GG_BytesToInt64Le((const uint8_t *)"\xab\xcd\xef\x01\x23\x45\x67\x89");
    CHECK_EQUAL(0x8967452301efcdab, result);
}

//----------------------------------------------------------------------
typedef struct {
    int field1;
    int field2;
} Callback1State;

static void callback1_handler(void* state) {
    Callback1State* self = (Callback1State*)state;
    self->field1 = 1234;
    self->field2 = 5678;
}

TEST(GG_UTILS, Test_SimpleCallbacks) {
    GG_SimpleCallback callback1;
    Callback1State    callback1_state = {0, 0};
    GG_GenericCallbackHandler* h = GG_SimpleCallback_Init(&callback1, callback1_handler, &callback1_state);
    CHECK_FALSE(h == NULL);
    GG_GenericCallbackHandler_OnCallback(h);
    CHECK_EQUAL(1234, callback1_state.field1);
    CHECK_EQUAL(5678, callback1_state.field2);
}

//----------------------------------------------------------------------
TEST(GG_UTILS, Test_ProtobufVarint) {
    size_t size = GG_ProtobufVarintSize((uint64_t)-1);
    LONGS_EQUAL(10, size);

    uint8_t buffer[10];
    for (unsigned int i = 0; i < 10; i++) {
        uint64_t value = 0;
        for (unsigned int j = 0; j< 64; j++) {
            value = (value * 3) + i;
            size = GG_ProtobufVarintSize(value);
            CHECK_TRUE(size <= 10);
            size_t encoded_size = GG_EncodeProtobufVarint(value, buffer);
            LONGS_EQUAL(size, encoded_size);
            uint64_t decoded = 0;
            size_t bytes_consumed = GG_DecodeProtobufVarint(buffer, 10, &decoded);
            LONGS_EQUAL(encoded_size, bytes_consumed);
            LONGLONGS_EQUAL(value, decoded);
            int64_t s_value = GG_ProtobufSignedFromZigZag(value);
            uint64_t u_value = GG_ProtobufSignedToZigZag(s_value);
            LONGLONGS_EQUAL(u_value, value);
        }
    }

    uint64_t u_value = GG_ProtobufSignedToZigZag(0);
    int64_t s_value = GG_ProtobufSignedFromZigZag(u_value);
    LONGLONGS_EQUAL(0, s_value);
    LONGLONGS_EQUAL(0, u_value);
    u_value = GG_ProtobufSignedToZigZag(-1);
    s_value = GG_ProtobufSignedFromZigZag(u_value);
    LONGLONGS_EQUAL(-1, s_value);
    LONGLONGS_EQUAL(1, u_value);
    u_value = GG_ProtobufSignedToZigZag(1);
    s_value = GG_ProtobufSignedFromZigZag(u_value);
    LONGLONGS_EQUAL(1, s_value);
    LONGLONGS_EQUAL(2, u_value);
    u_value = GG_ProtobufSignedToZigZag(-2);
    s_value = GG_ProtobufSignedFromZigZag(u_value);
    LONGLONGS_EQUAL(-2, s_value);
    LONGLONGS_EQUAL(3, u_value);
    u_value = GG_ProtobufSignedToZigZag(2147483647);
    s_value = GG_ProtobufSignedFromZigZag(u_value);
    LONGLONGS_EQUAL(2147483647, s_value);
    LONGLONGS_EQUAL(4294967294, u_value);
    u_value = GG_ProtobufSignedToZigZag(-2147483648);
    s_value = GG_ProtobufSignedFromZigZag(u_value);
    LONGLONGS_EQUAL((int64_t)-2147483648, s_value);
    LONGLONGS_EQUAL((uint64_t)4294967295, u_value);

    uint8_t foo[] = { 0x81 };
    size_t bytes_consumed = GG_DecodeProtobufVarint(foo, sizeof(foo), &u_value);
    LONGS_EQUAL(0, bytes_consumed);

    // try decoding without asking for the value
    GG_EncodeProtobufVarint(1234, buffer);
    bytes_consumed = GG_DecodeProtobufVarint(buffer, sizeof(buffer), NULL);
    LONGS_EQUAL(bytes_consumed, 2);
}

//----------------------------------------------------------------------
TEST(GG_UTILS, Test_CRC32) {
    uint8_t input[10] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99
    };
    uint32_t value = GG_Crc32(0x12345678, input, sizeof(input));
    LONGS_EQUAL(0x3eb92e38, value);
}
