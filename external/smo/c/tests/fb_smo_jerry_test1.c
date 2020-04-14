/**
*
* @file: fb_smo_jerry_test1.c
*
* @copyright
* Copyright 2017 by Fitbit, Inc., all rights reserved.
*
* @author Gilles Boccon-Gibod
*
* @date 2016-11-05
*
* @details
*
* Simple Multipurpose Objects - JerryScript Tests 1
*
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "jerryscript.h"
#include "jerryscript-port-default.h"
#include "fb_smo_jerryscript.h"

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
/* use this so we can put a breakpoint */
static void print_error(unsigned int line) {
    fprintf(stderr, "ERROR line %d\n", line);
}
#define CHECK(x) do {          \
    if (!(x)) {                \
        print_error(__LINE__); \
        return 1;              \
    }                          \
} while(0)

#define CHECK_E(x) do {        \
    if (!(x)) {                \
        print_error(__LINE__); \
        exit(1);               \
    }                          \
} while(0)

/*----------------------------------------------------------------------
|   InjectCbor
+---------------------------------------------------------------------*/
static int
InjectCbor(const uint8_t* cbor, unsigned int cbor_size)
{
    int           result;
    jerry_value_t obj;

    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor, cbor_size, &obj);
    if (result != FB_SMO_SUCCESS) {
        return result;
    }

    {
        jerry_value_t global = jerry_get_global_object();
        jerry_value_t obj_name = jerry_create_string((const jerry_char_t*)"cbor");
        jerry_set_property(global, obj_name, obj);
        jerry_release_value(obj);
        jerry_release_value(obj_name);
        jerry_release_value(global);
    }

    return 0;
}

/*----------------------------------------------------------------------
|   MakeRandomCbor
+---------------------------------------------------------------------*/
uint8_t cbor_test1[] = { 12 };
static int
MakeRandomCbor(uint8_t** cbor, unsigned int* cbor_size)
{

    *cbor = cbor_test1;
    *cbor_size = 1;

    return 0;
}

/*----------------------------------------------------------------------
|   BasicTypesTest
+---------------------------------------------------------------------*/
static int
BasicTypesTest(void)
{
    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);

    jerry_value_t obj;

    const uint8_t cbor_zero[5] = { 0x00 }; // 0
    int result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_zero, sizeof(cbor_zero), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((uint32_t)jerry_get_number_value(obj) == 0);
    jerry_release_value(obj);

    const uint8_t cbor_tiny_positive_int[5] = { 0x01 }; // 1
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_tiny_positive_int, sizeof(cbor_tiny_positive_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((uint32_t)jerry_get_number_value(obj) == 1);
    jerry_release_value(obj);

    const uint8_t cbor_tiny_negative_int[5] = { 0x20 }; // -1
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_tiny_negative_int, sizeof(cbor_tiny_negative_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((int32_t)jerry_get_number_value(obj) == -1);
    jerry_release_value(obj);

    const uint8_t cbor_small_positive_int[5] = { 0x18, 0x55 }; // 85
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_small_positive_int, sizeof(cbor_small_positive_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((uint32_t)jerry_get_number_value(obj) == 85);
    jerry_release_value(obj);

    const uint8_t cbor_small_negative_int[5] = { 0x38, 0x54 }; // -85
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_small_negative_int, sizeof(cbor_small_negative_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((int32_t)jerry_get_number_value(obj) == -85);
    jerry_release_value(obj);

    const uint8_t cbor_medium_positive_int[5] = { 0x19, 0x55, 0x66 }; // 21862
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_medium_positive_int, sizeof(cbor_medium_positive_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((uint32_t)jerry_get_number_value(obj) == 21862);
    jerry_release_value(obj);

    const uint8_t cbor_medium_negative_int[5] = { 0x39, 0x55, 0x65 }; // -21862
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_medium_negative_int, sizeof(cbor_medium_negative_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((int32_t)jerry_get_number_value(obj) == -21862);
    jerry_release_value(obj);

    const uint8_t cbor_positive_int[5] = { 0x1a, 0x00, 0x12, 0xd6, 0x87 }; // 1234567
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_positive_int, sizeof(cbor_positive_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((uint32_t)jerry_get_number_value(obj) == 1234567);
    jerry_release_value(obj);

    const uint8_t cbor_negative_int[5] = { 0x3a, 0x00, 0x12, 0xd6, 0x86 }; // -1234567
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_negative_int, sizeof(cbor_negative_int), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK((int32_t)jerry_get_number_value(obj) == -1234567);
    jerry_release_value(obj);

    const uint8_t cbor_float[9] = { 0xfb, 0x3f, 0xf3, 0xc0, 0xc9, 0x53, 0x9b, 0x88, 0x87 }; // 1.234567
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_float, sizeof(cbor_float), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_number(obj));
    CHECK(jerry_get_number_value(obj) == 1.234567);
    jerry_release_value(obj);

    const uint8_t cbor_false[1] = { 0xf4 }; // false
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_false, sizeof(cbor_false), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_boolean(obj));
    CHECK(jerry_get_boolean_value(obj) == false);
    jerry_release_value(obj);

    const uint8_t cbor_true[1] = { 0xf5 }; // true
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_true, sizeof(cbor_true), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_boolean(obj));
    CHECK(jerry_get_boolean_value(obj) == true);
    jerry_release_value(obj);

    const uint8_t cbor_null[1] = { 0xf6 }; // null
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_null, sizeof(cbor_null), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_null(obj));
    jerry_release_value(obj);

    const uint8_t cbor_undefined[1] = { 0xf7 }; // 'undefined'
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_undefined, sizeof(cbor_undefined), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_undefined(obj));
    jerry_release_value(obj);

    const uint8_t cbor_string[10] = { 0x69, 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x2c, 0x20, 0xcf, 0x80 }; // "Hello, π"
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_string, sizeof(cbor_string), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_string(obj));
    uint8_t str_buffer[10] = {};
    jerry_string_to_utf8_char_buffer(obj, str_buffer, sizeof(str_buffer));
    CHECK(!strcmp((const char*)str_buffer, "Hello, π"));
    jerry_release_value(obj);

    const uint8_t cbor_bytes[4] = { 0x43, 0x01, 0x02, 0x03 }; // 3 bytes 0x00 0x01 0x02
    uint8_t cbor_bytes_check[4];
    unsigned int cbor_bytes_check_size = sizeof(cbor_bytes_check);
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_bytes, sizeof(cbor_bytes), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_object(obj));
    result = Fb_Smo_Serialize_CBOR_FromJerry(obj, cbor_bytes_check, &cbor_bytes_check_size, 1, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_bytes_check_size == 0);
    CHECK(memcmp(cbor_bytes_check, cbor_bytes, sizeof(cbor_bytes)) == 0);
    jerry_release_value(obj);

    const uint8_t cbor_array[4] = { 0x83, 0x01, 0x02, 0x03 }; // [1,2,3]
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_array, sizeof(cbor_array), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_array(obj));
    CHECK(jerry_get_array_length(obj) == 3);
    jerry_value_t key0 = jerry_create_string((const jerry_char_t*)"0");
    jerry_value_t j0 = jerry_get_property(obj, key0);
    jerry_release_value(key0);
    CHECK(jerry_value_is_number(j0));
    CHECK(jerry_get_number_value(j0) == 1);
    jerry_release_value(j0);
    jerry_value_t key1 = jerry_create_string((const jerry_char_t*)"1");
    jerry_value_t j1 = jerry_get_property(obj, key1);
    jerry_release_value(key1);
    CHECK(jerry_value_is_number(j1));
    CHECK(jerry_get_number_value(j1) == 2);
    jerry_release_value(j1);
    jerry_value_t key2 = jerry_create_string((const jerry_char_t*)"2");
    jerry_value_t j2 = jerry_get_property(obj, key2);
    jerry_release_value(key2);
    CHECK(jerry_value_is_number(j2));
    CHECK(jerry_get_number_value(j2) == 3);
    jerry_release_value(j2);
    jerry_release_value(obj);

    const uint8_t cbor_obj[7] = { 0xa2, 0x61, 0x61, 0x01, 0x61, 0x62, 0x02 }; // {a:1, b:2}
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_obj, sizeof(cbor_obj), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_object(obj));
    jerry_value_t keys = jerry_get_object_keys(obj);
    CHECK(jerry_get_array_length(keys) == 2);
    key0 = jerry_get_property_by_index(keys, 0);
    jerry_value_t val0 = jerry_get_property(obj, key0);
    jerry_release_value(key0);
    CHECK(jerry_get_number_value(val0) == 1);
    jerry_release_value(val0);
    key1 = jerry_get_property_by_index(keys, 1);
    jerry_value_t val1 = jerry_get_property(obj, key1);
    jerry_release_value(key1);
    CHECK(jerry_get_number_value(val1) == 2);
    jerry_release_value(val1);
    jerry_release_value(keys);
    jerry_release_value(obj);

    const uint8_t cbor_obj2[7] = { 0xa1, 0x62, 0xcf, 0x80, 0x62, 0xcf, 0x80 }; // {π, "π"}
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_obj2, sizeof(cbor_obj2), &obj);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(jerry_value_is_object(obj));
    keys = jerry_get_object_keys(obj);
    CHECK(jerry_get_array_length(keys) == 1);
    key0 = jerry_get_property_by_index(keys, 0);
    val0 = jerry_get_property(obj, key0);
    jerry_release_value(key0);
    CHECK(jerry_value_is_string(val0));
    jerry_release_value(val0);
    jerry_release_value(keys);
    jerry_release_value(obj);

    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   FailuresTest
+---------------------------------------------------------------------*/
static int
FailuresTest()
{
    jerry_init(JERRY_INIT_EMPTY);

    const uint8_t cbor_obj[7] = { 0xa2, 0x01, 0x01, 0x02, 0x02 }; // invalid object with integer keys {1:1, 1:2}
    jerry_value_t obj;
    int result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor_obj, sizeof(cbor_obj), &obj);
    CHECK(result == FB_SMO_ERROR_INVALID_FORMAT);

#if 0 // JerryScript seems to exit on out of memory conditions!
    uint8_t longstring_cbor[] = {
      0x78, 0x33, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a,
      0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
      0x77, 0x78, 0x79, 0x7a, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
      0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x54, 0x55,
      0x56, 0x57, 0x58, 0x59, 0x5a
    };
    unsigned int i;
    for (i=0; i<10000; i++) {
        result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, longstring_cbor, sizeof(longstring_cbor), &obj);
    }
#endif

    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   SimpleParseTest
+---------------------------------------------------------------------*/
static int
SimpleParseTest(void)
{
    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    jerry_value_t ret_value = jerry_create_undefined ();

    const char* source = "JSON.stringify(cbor);";
    ret_value = jerry_parse((const jerry_char_t*)source, strlen(source), false);

    uint8_t* cbor = NULL;
    unsigned int cbor_size = 0;
    int result = MakeRandomCbor(&cbor, &cbor_size);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(!jerry_value_is_error(ret_value));

    InjectCbor(cbor, cbor_size);

    jerry_value_t func_val = ret_value;
    ret_value = jerry_run(func_val);
    jerry_release_value(func_val);

    CHECK(!jerry_value_is_error(ret_value));

    jerry_release_value (ret_value);
    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   SimpleSerializeTest
+---------------------------------------------------------------------*/
static int
SimpleSerializeTest(void)
{
    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    jerry_value_t ret_value = jerry_create_undefined ();

    const char* source = "({object1: {a:1, b:2}, array1: [1, 2, 'hello', 1.2345], float1: 1.2345, string1: 'someString', number1: 12345, bool1: false, bool2: true, nullv: null, undef: undefined, ninf: -Infinity, pinf: Infinity, nan: NaN});";
    ret_value = jerry_parse((const jerry_char_t*)source, strlen(source), false);

    CHECK(!jerry_value_is_error(ret_value));

    jerry_value_t func_val = ret_value;
    ret_value = jerry_run(func_val);
    jerry_release_value(func_val);

    CHECK(!jerry_value_is_error(ret_value));

    uint8_t* cbor = NULL;
    unsigned int cbor_size = 0;
    unsigned int cbor_size_copy = 0;
    int result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size != 0);
    cbor = (uint8_t*)malloc(cbor_size);
    cbor_size_copy = cbor_size;
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, cbor, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 0);
    cbor_size = cbor_size_copy;

    jerry_value_t obj_out;
    result = Fb_Smo_Deserialize_CBOR_ToJerry(NULL, cbor, cbor_size, &obj_out, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(!jerry_value_is_undefined(obj_out));

    jerry_value_t keys = jerry_get_object_keys(obj_out);
    uint32_t key_count = jerry_get_array_length(keys);
    jerry_release_value(keys);
    CHECK(key_count == 12);

    jerry_value_t string1_prop = jerry_create_string((const jerry_char_t*)"string1");
    CHECK(!jerry_value_is_error(string1_prop));
    jerry_value_t string1 = jerry_get_property(obj_out, string1_prop);
    jerry_release_value(string1_prop);
    CHECK(jerry_value_is_string(string1));
    jerry_char_t buf[50] = {};
    jerry_string_to_utf8_char_buffer(string1, buf, sizeof(buf));
    // Use strncmp because `jerry_string_to_utf8_char_buffer` doesn't NULL terminate
    CHECK(!strncmp("someString", (const char*)buf, strlen("someString")));
    jerry_release_value(string1);

    jerry_release_value(obj_out);

    free(cbor);

    jerry_release_value(ret_value);
    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   ArrayBufferTest
+---------------------------------------------------------------------*/
static int
ArrayBufferTest(void)
{
    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    jerry_value_t ret_value = jerry_create_undefined ();

    const char* source = "var a = new ArrayBuffer(3); var b = new Uint8Array(a); b[0] = 1; b[1] = 2; b[2] = 3; a";
    ret_value = jerry_parse((const jerry_char_t*)source, strlen(source), false);

    CHECK(!jerry_value_is_error(ret_value));

    jerry_value_t func_val = ret_value;
    ret_value = jerry_run(func_val);
    jerry_release_value(func_val);

    CHECK(!jerry_value_is_error(ret_value));

    uint8_t cbor[4] = {0};
    unsigned int cbor_size = 0;
    int result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 4);
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, cbor, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 0);
    CHECK(cbor[0] == 0x43);
    CHECK(cbor[1] == 0x01);
    CHECK(cbor[2] == 0x02);
    CHECK(cbor[3] == 0x03);

    jerry_release_value(ret_value);
    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   TypedArrayTest
+---------------------------------------------------------------------*/
static int
TypedArrayTest(void)
{
    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    jerry_value_t ret_value = jerry_create_undefined ();

    const char* source = "var a = new ArrayBuffer(4); var b = new Uint8Array(a); b[0] = 99; b[1] = 1; b[2] = 2; b[3] = 3; new Uint8Array(a, 1, 3)";
    ret_value = jerry_parse((const jerry_char_t*)source, strlen(source), false);

    CHECK(!jerry_value_is_error(ret_value));

    jerry_value_t func_val = ret_value;
    ret_value = jerry_run(func_val);
    jerry_release_value(func_val);

    CHECK(!jerry_value_is_error(ret_value));

    uint8_t cbor[4] = {0};
    unsigned int cbor_size = 0;
    int result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 4);
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, cbor, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 0);
    CHECK(cbor[0] == 0x43);
    CHECK(cbor[1] == 0x01);
    CHECK(cbor[2] == 0x02);
    CHECK(cbor[3] == 0x03);

    jerry_release_value(ret_value);
    jerry_cleanup();

    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    ret_value = jerry_create_undefined ();

    const char* source2 = "var a = new Uint16Array(1); a[0] = 0x1234; a";
    ret_value = jerry_parse((const jerry_char_t*)source2, strlen(source2), false);

    CHECK(!jerry_value_is_error(ret_value));

    func_val = ret_value;
    ret_value = jerry_run(func_val);
    jerry_release_value(func_val);

    CHECK(!jerry_value_is_error(ret_value));

    uint8_t cbor2[3] = {0};
    cbor_size = 0;
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 16);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 3);
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, cbor2, &cbor_size, 16);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 0);
    CHECK(cbor2[0] == 0x42);
    if (cbor2[1] == 0x12) {
        // big endian
        CHECK(cbor2[1] == 0x12);
        CHECK(cbor2[2] == 0x34);
    } else {
        // little endian
        CHECK(cbor2[1] == 0x34);
        CHECK(cbor2[2] == 0x12);
    }

    jerry_release_value(ret_value);
    jerry_cleanup();

    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    ret_value = jerry_create_undefined ();

    const char* source3 = "var a = new Uint32Array(1); a[0] = 0x12345678; a";
    ret_value = jerry_parse((const jerry_char_t*)source3, strlen(source3), false);

    CHECK(!jerry_value_is_error(ret_value));

    func_val = ret_value;
    ret_value = jerry_run(func_val);
    jerry_release_value(func_val);

    CHECK(!jerry_value_is_error(ret_value));
    uint8_t cbor3[5] = {0};
    cbor_size = 0;
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 5);
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, cbor3, &cbor_size, 16, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(cbor_size == 0);
    CHECK(cbor3[0] == 0x44);
    if (cbor3[1] == 0x12) {
        // big endian
        CHECK(cbor3[1] == 0x12);
        CHECK(cbor3[2] == 0x34);
        CHECK(cbor3[3] == 0x56);
        CHECK(cbor3[4] == 0x78);
    } else {
        // little endian
        CHECK(cbor3[1] == 0x78);
        CHECK(cbor3[2] == 0x56);
        CHECK(cbor3[3] == 0x34);
        CHECK(cbor3[4] == 0x12);
    }

    jerry_release_value(ret_value);
    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   ParseDepthTest
+---------------------------------------------------------------------*/
static int
ParseDepthTest(void)
{
    /* init JerryScript */
    jerry_init(JERRY_INIT_EMPTY);
    jerry_value_t ret_value = jerry_create_undefined ();

    const char* source = "({a:{a:{a:{a:{a:{a:{a:{a:{}}}}}}}}});";
    ret_value = jerry_parse((const jerry_char_t*)source, strlen(source), false);

    CHECK(!jerry_value_is_error(ret_value));

    jerry_value_t func_val = ret_value;
    ret_value = jerry_run(func_val);
    jerry_release_value(func_val);

    CHECK(!jerry_value_is_error(ret_value));

    unsigned int cbor_size = 0;
    int result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 0, NULL, NULL);
    CHECK(result == FB_SMO_ERROR_OVERFLOW);

    cbor_size = 0;
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 1, NULL, NULL);
    CHECK(result == FB_SMO_ERROR_OVERFLOW);

    cbor_size = 0;
    result = Fb_Smo_Serialize_CBOR_FromJerry(ret_value, NULL, &cbor_size, 9, NULL, NULL);
    CHECK(result == FB_SMO_SUCCESS);

    jerry_release_value(ret_value);
    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   EncoderTest
+---------------------------------------------------------------------*/

static bool CounterEncoder(void* context, jerry_value_t obj, uint8_t** serialized, unsigned int* available, int* result) {
    int* counter = (int*)context;
    (&counter)++;
    return false;
}

static int
EncoderTest(void)
{
    jerry_init(JERRY_INIT_EMPTY);
    jerry_value_t jerry_number = jerry_create_number(3);

    unsigned int cbor_size = 0;
    int counter = 0;
    int result = Fb_Smo_Serialize_CBOR_FromJerry(jerry_number, NULL, &cbor_size, 16, CounterEncoder, &counter)
    CHECK(counter == 1);

    jerry_release_value(jerry_number);
    jerry_cleanup();

    return 0;
}

/*----------------------------------------------------------------------
|   main
+---------------------------------------------------------------------*/
int
main(int argc, char** argv)
{
    int result;

    /* fixed seed */
    srand(0);

    /* basic tests */
    result = SimpleParseTest();
    if (result) return 1;

    result = ArrayBufferTest();
    if (result) return 1;

    result = TypedArrayTest();
    if (result) return 1;

    result = BasicTypesTest();
    if (result) return 1;

    result = SimpleSerializeTest();
    if (result) return 1;

    result = ParseDepthTest();
    if (result) return 1;

    result = FailuresTest();
    if (result) return 1;

    result = EncoderTest();
    if (result) return 1;

    return 0;
}
