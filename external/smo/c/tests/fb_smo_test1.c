/**
*
* @file: fb_smo_test1.c
*
* @copyright
* Copyright 2016 - 2017 by Fitbit, Inc., all rights reserved.
*
* @author Gilles Boccon-Gibod
*
* @date 2016-11-05
*
* @details
*
* Simple Message Object Model - Tests 1
*
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "fb_smo.h"
#include "fb_smo_serialization.h"
#include "fb_smo_utils.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define FB_SMO_TEST1_ITERATIONS 10000

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
char RandomString[32];
uint8_t RandomBytes[32];
unsigned int RandomBytesSize;

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
|   test smo factory
+---------------------------------------------------------------------*/
static void* test_malloc(Fb_SmoAllocator* _self, size_t size);
static void  test_free(Fb_SmoAllocator* _self, void* memory);

typedef struct {
    Fb_SmoAllocator base;
    size_t          total_allocated;
} TestSmoAllocator;

TestSmoAllocator GlobalTestSmoAllocator = {
    { test_malloc, test_free }, 0
};

static void*
test_malloc(Fb_SmoAllocator* _self, size_t size)
{
    TestSmoAllocator* self = (TestSmoAllocator*)_self;
    size_t* block = malloc(size+sizeof(size_t));
    *block = size;
    self->total_allocated += size;
    return (void*)(block+1);
}

static void
test_free(Fb_SmoAllocator* _self, void* memory)
{
    TestSmoAllocator* self = (TestSmoAllocator*)_self;
    size_t* block = ((size_t*)memory)-1;

    CHECK_E(self->total_allocated >= *block);

    self->total_allocated -= *block;

    memset(block, 0xCC, *block);
    free(block);
}

/*----------------------------------------------------------------------
|   MakeRandomString
+---------------------------------------------------------------------*/
static void
MakeRandomString(void) {
    unsigned int i;
    unsigned int length = rand()%32;
    for (i=0; i<length; i++) {
        RandomString[i] = 'a'+ (rand()%26);
    }
    RandomString[length] = '\0';
}

/*----------------------------------------------------------------------
|   MakeRandomBytes
+---------------------------------------------------------------------*/
static void
MakeRandomBytes(void) {
    unsigned int i;
    RandomBytesSize = rand()%33;
    for (i=0; i<RandomBytesSize; i++) {
        RandomBytes[i] = (uint8_t)(rand()%256);
    }
}

/*----------------------------------------------------------------------
|   CreateRandomTree
+---------------------------------------------------------------------*/
static int
CreateRandomTree(Fb_SmoAllocator* factory, unsigned int iterations, Fb_Smo** tree)
{
    Fb_Smo* smo = NULL;
    Fb_Smo* container = Fb_Smo_CreateArray(factory);
    unsigned int i;

    for (i=0; i<iterations; i++) {
        int r = rand() % 7;
        switch (r) {
            case 0:
                smo = Fb_Smo_CreateObject(factory);
                break;

            case 1:
                smo = Fb_Smo_CreateArray(factory);
                break;

            case 2:
                MakeRandomString();
                smo = Fb_Smo_CreateString(factory, RandomString, 0);
                break;

            case 3:
                MakeRandomBytes();
                smo = Fb_Smo_CreateBytes(factory, RandomBytes, RandomBytesSize);
                break;

            case 4:
                {
                    int64_t value = rand();
                    if (rand() % 4) {
                        value <<= (rand()%33);
                    }
                    smo = Fb_Smo_CreateInteger(factory, rand());
                }
                break;

            case 5:
                smo = Fb_Smo_CreateFloat(factory, 1.0/(1+(rand()%100)));
                break;

            case 6:
                smo = Fb_Smo_CreateSymbol(factory, (Fb_SmoSymbol)(rand()%4));
                break;

            default:
                /* should never happen */
                exit(1);
        }

        CHECK(smo != NULL);

        /* add this to the current container */
        if (Fb_Smo_GetType(container) == FB_SMO_TYPE_OBJECT) {
            /* pick a name */
            MakeRandomString();
            Fb_Smo_AddChild(container, RandomString, 0, smo);
        } else {
            Fb_Smo_AddChild(container, NULL, 0, smo);
        }

        /* if this is a container, it becomes the new container with probability 1/10 */
        if (Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT || Fb_Smo_GetType(smo) == FB_SMO_TYPE_ARRAY) {
            if ((rand()%10) == 0) {
                container = smo;
            }
        }

        /* go back up to the parent with probability 1/10 */
        if ((rand()%10) == 0) {
            Fb_Smo* parent = Fb_Smo_GetParent(container);
            if (parent) {
                container = parent;
            }
        }
    }

    while (Fb_Smo_GetParent(container)) {
        container = Fb_Smo_GetParent(container);
    }
    *tree = container;
    return 0;
}

/*----------------------------------------------------------------------
|   test simple object creation
+---------------------------------------------------------------------*/
static int
SimpleObjectCreationTest(void)
{
    Fb_Smo* smo;
    Fb_SmoAllocator* factory = &GlobalTestSmoAllocator.base;
    uint8_t workspace[64];
    unsigned int i;
    unsigned int bytes_size;
    const uint8_t* bytes;

    for (i=0; i<2; i++) {
        if (i==0) factory = NULL;

        /* objects */
        smo = Fb_Smo_CreateObject(factory);
        CHECK(smo);
        CHECK(Fb_Smo_GetChildrenCount(smo) == 0);
        Fb_Smo_Destroy(smo);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* arrays */
        smo = Fb_Smo_CreateArray(factory);
        CHECK(smo);
        CHECK(Fb_Smo_GetChildrenCount(smo) == 0);
        Fb_Smo_Destroy(smo);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* strings */
        smo = Fb_Smo_CreateString(factory, "", 0);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsString(smo));
        CHECK(Fb_Smo_GetValueAsString(smo)[0] == 0);
        Fb_Smo_Destroy(smo);
        smo = Fb_Smo_CreateString(factory, NULL, 0);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsString(smo));
        CHECK(Fb_Smo_GetValueAsString(smo)[0] == 0);
        Fb_Smo_Destroy(smo);
        smo = Fb_Smo_CreateString(factory, "some_string", 0);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsString(smo));
        CHECK(!strcmp(Fb_Smo_GetValueAsString(smo), "some_string"));
        Fb_Smo_Destroy(smo);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* bytes */
        workspace[0] = 1;
        workspace[1] = 2;
        smo = Fb_Smo_CreateBytes(factory, NULL, 0);
        bytes = Fb_Smo_GetValueAsBytes(smo, &bytes_size);
        CHECK(bytes == NULL);
        CHECK(bytes_size == 0);
        Fb_Smo_Destroy(smo);
        smo = Fb_Smo_CreateBytes(factory, workspace, 2);
        CHECK(smo);
        bytes = Fb_Smo_GetValueAsBytes(smo, &bytes_size);
        CHECK(bytes != NULL);
        CHECK(bytes_size == 2);
        CHECK(bytes[0] == 1);
        CHECK(bytes[1] == 2);
        Fb_Smo_Destroy(smo);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* integers */
        smo = Fb_Smo_CreateInteger(factory, 89);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsInteger(smo) == 89);
        Fb_Smo_Destroy(smo);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* floats */
        smo = Fb_Smo_CreateFloat(factory, 1.2345);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsFloat(smo) == 1.2345);
        Fb_Smo_Destroy(smo);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* symbols */
        smo = Fb_Smo_CreateSymbol(factory, FB_SMO_SYMBOL_NULL);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_NULL);
        Fb_Smo_Destroy(smo);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);
    }

    return 0;
}

/*----------------------------------------------------------------------
|   type coercion test
+---------------------------------------------------------------------*/
static int
CoercionTest(void)
{
    Fb_Smo* smo;

    /* integer to float */
    smo = Fb_Smo_CreateInteger(NULL, 89);
    CHECK(smo);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 89);
    CHECK(Fb_Smo_GetValueAsFloat(smo) == (double)89);
    Fb_Smo_Destroy(smo);

    /* float to integer */
    smo = Fb_Smo_CreateFloat(NULL, 1.2345);
    CHECK(smo);
    CHECK(Fb_Smo_GetValueAsFloat(smo) == 1.2345);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == (int64_t)1.2345);
    Fb_Smo_Destroy(smo);

    return 0;
}

/*----------------------------------------------------------------------
|   TestSimpleBlockAllocator
+---------------------------------------------------------------------*/
static int
TestSimpleBlockAllocator()
{
    Fb_SmoSimpleBlockAllocator allocator;
    uint8_t blocks[5*sizeof(void*)*2];
    uint8_t deeply_nested[] = {4<<5 | 2, 4<<5 | 2, 4<<5 | 2, 4<<5 | 2, 4<<5 | 1, 1, 2, 3, 4, 5};

    Fb_Smo* root = NULL;
    Fb_SmoSimpleBlockAllocator_Init(&allocator, blocks, sizeof(blocks));
    int result = Fb_Smo_Deserialize(NULL, &allocator.base, FB_SMO_SERIALIZATION_FORMAT_CBOR, deeply_nested, sizeof(deeply_nested), &root);
    CHECK(result == FB_SMO_ERROR_OUT_OF_MEMORY);

    return 0;
}

/*----------------------------------------------------------------------
|   TestOutOfMemory
+---------------------------------------------------------------------*/
static int
TestOutOfMemory()
{
    Fb_SmoSimpleBlockAllocator block_allocator;
    uint8_t blocks[10*sizeof(void*)];
    uint8_t cbor[] = {0xa3, 0x61, 0x61, 0x01, 0x61, 0x63, 0x03, 0x61, 0x62, 0x02};
    int result;

    Fb_Smo* root = NULL;
    Fb_SmoSimpleBlockAllocator_Init(&block_allocator, blocks, sizeof(blocks));
    result = Fb_Smo_Deserialize(&block_allocator.base, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor, sizeof(cbor), &root);
    CHECK(result == FB_SMO_ERROR_OUT_OF_MEMORY);
    CHECK(root == NULL);

    {
        TestSmoAllocator heap_allocator = {
            { test_malloc, test_free }, 0
        };
        size_t object_mem_needed = 0;
        size_t parser_mem_needed = 0;
        unsigned int i;
        unsigned char misc_cbor[] = {
          0xac, 0x68, 0x73, 0x6d, 0x61, 0x6c, 0x6c, 0x49, 0x6e, 0x74, 0x18, 0xc8,
          0x66, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0xa2, 0x61, 0x61, 0x01, 0x61,
          0x62, 0x02, 0x65, 0x66, 0x61, 0x6c, 0x73, 0x65, 0xf4, 0x69, 0x6d, 0x65,
          0x64, 0x69, 0x75, 0x6d, 0x49, 0x6e, 0x74, 0x1a, 0x00, 0x01, 0x38, 0x80,
          0x65, 0x66, 0x6c, 0x6f, 0x61, 0x74, 0xfb, 0x3f, 0xf3, 0xc0, 0x83, 0x12,
          0x6e, 0x97, 0x8d, 0x6c, 0x6d, 0x65, 0x64, 0x69, 0x75, 0x6d, 0x53, 0x74,
          0x72, 0x69, 0x6e, 0x67, 0x78, 0x45, 0x6d, 0x65, 0x64, 0x69, 0x75, 0x6d,
          0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31, 0x20, 0x6d, 0x65, 0x64, 0x69,
          0x75, 0x6d, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31, 0x20, 0x6d, 0x65,
          0x64, 0x69, 0x75, 0x6d, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x31, 0x20,
          0x6d, 0x65, 0x64, 0x69, 0x75, 0x6d, 0x53, 0x74, 0x72, 0x69, 0x6e, 0x67,
          0x31, 0x20, 0x6d, 0x65, 0x64, 0x69, 0x75, 0x6d, 0x53, 0x74, 0x72, 0x69,
          0x6e, 0x67, 0x31, 0x6a, 0x74, 0x69, 0x6e, 0x79, 0x53, 0x74, 0x72, 0x69,
          0x6e, 0x67, 0x6a, 0x74, 0x69, 0x6e, 0x79, 0x53, 0x74, 0x72, 0x69, 0x6e,
          0x67, 0x67, 0x74, 0x69, 0x6e, 0x79, 0x49, 0x6e, 0x74, 0x0c, 0x68, 0x6c,
          0x61, 0x72, 0x67, 0x65, 0x49, 0x6e, 0x74, 0x1b, 0x00, 0x00, 0x00, 0x12,
          0xa0, 0x5f, 0x20, 0x00, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x86, 0x01,
          0x02, 0x03, 0x61, 0x31, 0x61, 0x32, 0x61, 0x33, 0x64, 0x6e, 0x75, 0x6c,
          0x6c, 0xf6, 0x64, 0x74, 0x72, 0x75, 0x65, 0xf5
        };

        result = Fb_Smo_Deserialize(&heap_allocator.base, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, misc_cbor, sizeof(misc_cbor), &root);
        CHECK(result == FB_SMO_SUCCESS);
        object_mem_needed = heap_allocator.total_allocated;
        Fb_Smo_Destroy(root);
        root = NULL;

        result = Fb_Smo_Deserialize(NULL, &heap_allocator.base, FB_SMO_SERIALIZATION_FORMAT_CBOR, misc_cbor, sizeof(misc_cbor), &root);
        CHECK(result == FB_SMO_SUCCESS);
        parser_mem_needed = heap_allocator.total_allocated;
        Fb_Smo_Destroy(root);
        root = NULL;

        for (i=1; i<object_mem_needed; i++) {
            uint8_t* mem_block = (uint8_t*)malloc(i);
            Fb_SmoGrowOnlyAllocator grow_allocator;
            Fb_SmoGrowOnlyAllocator_Init(&grow_allocator, mem_block, i);
            result = Fb_Smo_Deserialize(&grow_allocator.base, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, misc_cbor, sizeof(misc_cbor), &root);
            CHECK(result == FB_SMO_ERROR_OUT_OF_MEMORY);
            free(mem_block);
        }

        for (i=1; i<parser_mem_needed; i++) {
            uint8_t* mem_block = (uint8_t*)malloc(i);
            Fb_SmoGrowOnlyAllocator grow_allocator;
            Fb_SmoGrowOnlyAllocator_Init(&grow_allocator, mem_block, i);
            result = Fb_Smo_Deserialize(NULL, &grow_allocator.base, FB_SMO_SERIALIZATION_FORMAT_CBOR, misc_cbor, sizeof(misc_cbor), &root);
            CHECK(result == FB_SMO_ERROR_OUT_OF_MEMORY);
            free(mem_block);
        }
    }

    return 0;
}

/*----------------------------------------------------------------------
|   TestGrowOnlyAllocator
+---------------------------------------------------------------------*/
static int
TestGrowOnlyAllocator()
{
    Fb_SmoGrowOnlyAllocator allocator;
    uint8_t memory[100];
    Fb_Smo* smo = NULL;

    Fb_SmoGrowOnlyAllocator_Init(&allocator, memory, sizeof(memory));
    smo = Fb_Smo_CreateInteger(&allocator.base, 123);
    CHECK(smo != NULL);
    Fb_Smo_Destroy(smo);
    smo = Fb_Smo_CreateString(&allocator.base, "kshkjshkjhkjhkjhkjhsdkjhfskjdhfsdfsdfskshkjshkjhkjhkjhkjhsdkjhfskjdhfsdfsdfskshkjshkjhkjhkjhkjhsdkjhfskjdhfsdfsdfskshkjshkjhkjhkjhkjhsdkjhfskjdhfsdfsdfs", 0);
    CHECK(smo == NULL);

    return 0;
}

/*----------------------------------------------------------------------
|   container test
+---------------------------------------------------------------------*/
static int
ContainerTest(void)
{
    Fb_SmoAllocator* factory = &GlobalTestSmoAllocator.base;
    Fb_Smo* object_1;
    Fb_Smo* array_1;
    Fb_Smo* string_1;
    Fb_Smo* integer_1;

    object_1 = Fb_Smo_CreateObject(factory);
    CHECK(Fb_Smo_GetChildrenCount(object_1) == 0);
    string_1 = Fb_Smo_CreateString(factory, "string1", 0);
    Fb_Smo_AddChild(object_1, "field", 0, string_1);
    CHECK(Fb_Smo_GetChildrenCount(object_1) == 1);
    CHECK(!strcmp(Fb_Smo_GetName(string_1), "field"));
    Fb_Smo_Destroy(string_1);
    CHECK(Fb_Smo_GetChildrenCount(object_1) == 0);
    string_1 = Fb_Smo_CreateString(factory, "string1", 0);
    Fb_Smo_AddChild(object_1, "field", 0, string_1);
    Fb_Smo_Destroy(object_1);

    object_1  = Fb_Smo_CreateObject(factory);
    string_1  = Fb_Smo_CreateString(factory, "string1", 0);
    integer_1 = Fb_Smo_CreateInteger(factory, 89);
    Fb_Smo_AddChild(object_1, "field1", 0, string_1);
    Fb_Smo_AddChild(object_1, "field2", 0, integer_1);
    CHECK(Fb_Smo_GetChildrenCount(object_1) == 2);
    Fb_Smo_Destroy(object_1);

    array_1 = Fb_Smo_CreateArray(factory);
    CHECK(Fb_Smo_GetChildrenCount(array_1) == 0);
    string_1 = Fb_Smo_CreateString(factory, "string1", 0);
    CHECK(Fb_Smo_AddChild(array_1, "field", 0, string_1) == FB_SMO_ERROR_INVALID_PARAMETERS);
    CHECK(Fb_Smo_AddChild(array_1, "", 0, string_1) == FB_SMO_ERROR_INVALID_PARAMETERS);
    CHECK(Fb_Smo_AddChild(array_1, NULL, 0, string_1) == FB_SMO_SUCCESS);
    CHECK(Fb_Smo_AddChild(array_1, NULL, 0, string_1) == FB_SMO_ERROR_INVALID_PARAMETERS);
    CHECK(Fb_Smo_GetChildrenCount(array_1) == 1);
    CHECK(Fb_Smo_GetName(string_1) == NULL);
    Fb_Smo_Destroy(string_1);
    CHECK(Fb_Smo_GetChildrenCount(array_1) == 0);
    string_1 = Fb_Smo_CreateString(factory, "string1", 0);
    Fb_Smo_AddChild(array_1, NULL, 0, string_1);
    Fb_Smo_Destroy(array_1);

    array_1   = Fb_Smo_CreateObject(factory);
    string_1  = Fb_Smo_CreateString(factory, "string1", 0);
    integer_1 = Fb_Smo_CreateInteger(factory, 89);
    Fb_Smo_AddChild(array_1, NULL, 0, string_1);
    Fb_Smo_AddChild(array_1, NULL, 0, integer_1);
    CHECK(Fb_Smo_GetChildrenCount(array_1) == 2);
    Fb_Smo_Destroy(array_1);

    return 0;
}

/*----------------------------------------------------------------------
|   PathTest
+---------------------------------------------------------------------*/
static int
PathTest(void)
{
    Fb_Smo* root;
    Fb_Smo* smo;
    Fb_Smo* child;
    Fb_Smo* found;

    root = Fb_Smo_CreateArray(NULL);
    smo = root;
    child = Fb_Smo_CreateInteger(NULL, 1);
    Fb_Smo_AddChild(smo, NULL, 0, child);
    child = Fb_Smo_CreateInteger(NULL, 2);
    Fb_Smo_AddChild(smo, NULL, 0, child);
    child = Fb_Smo_CreateInteger(NULL, 3);
    Fb_Smo_AddChild(smo, NULL, 0, child);
    child = Fb_Smo_CreateObject(NULL);
    Fb_Smo_AddChild(smo, NULL, 0, child);
    smo = child;
    child = Fb_Smo_CreateInteger(NULL, 5);
    Fb_Smo_AddChild(smo, "int1", 0, child);
    child = Fb_Smo_CreateInteger(NULL, 6);
    Fb_Smo_AddChild(smo, "int2", 0, child);
    child = Fb_Smo_CreateInteger(NULL, 7);
    Fb_Smo_AddChild(smo, "int3", 0, child);

    found = Fb_Smo_GetDescendantByPath(root, "foo");
    CHECK(found == NULL);
    found = Fb_Smo_GetDescendantByPath(root, "[0]");
    CHECK(found != NULL);
    CHECK(Fb_Smo_GetType(found) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(found) == 1);
    found = Fb_Smo_GetDescendantByPath(root, "[3]");
    CHECK(found != NULL);
    CHECK(Fb_Smo_GetType(found) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(found) == 3);
    found = Fb_Smo_GetDescendantByPath(root, "[4]");
    CHECK(found == NULL);
    found = Fb_Smo_GetDescendantByPath(root, "[3].int3");
    CHECK(found != NULL);
    CHECK(Fb_Smo_GetType(found) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(found) == 7);

    Fb_Smo_Destroy(root);

    root = Fb_Smo_CreateObject(NULL);
    smo = root;
    child = Fb_Smo_CreateArray(NULL);
    Fb_Smo_AddChild(smo, "foo", 0, child);
    smo = child;
    child = Fb_Smo_CreateObject(NULL);
    Fb_Smo_AddChild(smo, NULL, 0, child);
    smo = child;
    child = Fb_Smo_CreateInteger(NULL, 6);
    Fb_Smo_AddChild(smo, "bar", 0, child);
    found = Fb_Smo_GetDescendantByPath(root, "foo[0].bar");
    CHECK(found != NULL);
    CHECK(Fb_Smo_GetType(found) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(found) == 6);

    Fb_Smo_Destroy(root);

    root = Fb_Smo_CreateObject(NULL);
    smo = root;
    child = Fb_Smo_CreateObject(NULL);
    Fb_Smo_AddChild(smo, "foo", 0, child);
    smo = child;
    child = Fb_Smo_CreateInteger(NULL, 7);
    Fb_Smo_AddChild(smo, "bar", 0, child);
    found = Fb_Smo_GetDescendantByPath(root, "foo.bar");
    CHECK(found != NULL);
    CHECK(Fb_Smo_GetType(found) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(found) == 7);

    Fb_Smo_Destroy(root);

    return 0;
}

/*----------------------------------------------------------------------
|   VaragsTest
+---------------------------------------------------------------------*/
static int
VarargsTest(void)
{
    Fb_Smo* smo;
    Fb_Smo* child;
    uint8_t bytes[] = {1,2,3,4};
    unsigned int bytes_size;
    const uint8_t* bytes_value;

    smo = Fb_Smo_Create(NULL, "i", (int)1234);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 1234);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "I", (int64_t)123456789876543);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == (int64_t)123456789876543);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "f", 1.2345);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_FLOAT);
    CHECK(Fb_Smo_GetValueAsFloat(smo) == 1.2345);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "s", "hello");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_STRING);
    CHECK(strcmp(Fb_Smo_GetValueAsString(smo), "hello") == 0);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "#", FB_SMO_SYMBOL_TRUE);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_TRUE);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "#", FB_SMO_SYMBOL_FALSE);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_FALSE);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "#", FB_SMO_SYMBOL_NULL);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_NULL);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "#", FB_SMO_SYMBOL_UNDEFINED);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_UNDEFINED);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "b", bytes, (int)sizeof(bytes));
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_BYTES);
    bytes_value = Fb_Smo_GetValueAsBytes(smo, &bytes_size);
    CHECK(bytes_value != NULL);
    CHECK(bytes_size = sizeof(bytes));
    CHECK(memcmp(bytes_value, bytes, bytes_size) == 0);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "N");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_NULL);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "T");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_TRUE);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "F");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_FALSE);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "U");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_SYMBOL);
    CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_UNDEFINED);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "[]");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_ARRAY);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 0);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "[");
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "]");
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "{}");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 0);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "{");
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "}");
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "[iii]", 1, 2, 3);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_ARRAY);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 3);
    child = Fb_Smo_GetFirstChild(smo);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 1);
    child = Fb_Smo_GetNext(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 2);
    child = Fb_Smo_GetNext(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 3);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "[[[[[i]]]]]", 1234);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_ARRAY);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 1);
    child = Fb_Smo_GetFirstChild(smo);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 1);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 1);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 1);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 1);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 1234);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "[i[i[i[i[i]]]]]", 1,2,3,4,1234);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_ARRAY);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 2);
    child = Fb_Smo_GetFirstChild(smo);
    CHECK(child != NULL);
    child = Fb_Smo_GetNext(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 2);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    child = Fb_Smo_GetNext(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 2);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    child = Fb_Smo_GetNext(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 2);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    child = Fb_Smo_GetNext(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 1);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 1234);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "[[[[[i]i]i]i]i]", 1234, 1,2,3,4);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_ARRAY);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 2);
    child = Fb_Smo_GetFirstChild(smo);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 2);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 2);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 2);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetChildrenCount(child) == 1);
    child = Fb_Smo_GetFirstChild(child);
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 1234);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "{a=i}", 7);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 1);
    child = Fb_Smo_GetDescendantByPath(smo, "a");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(strcmp(Fb_Smo_GetName(child), "a") == 0);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 7);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "{=i}", "a", 7);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 1);
    child = Fb_Smo_GetDescendantByPath(smo, "a");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(strcmp(Fb_Smo_GetName(child), "a") == 0);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 7);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "{=i=T}", "a", 7, "b");
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 2);
    child = Fb_Smo_GetDescendantByPath(smo, "a");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(strcmp(Fb_Smo_GetName(child), "a") == 0);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 7);
    child = Fb_Smo_GetDescendantByPath(smo, "b");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_SYMBOL);
    CHECK(strcmp(Fb_Smo_GetName(child), "b") == 0);
    CHECK(Fb_Smo_GetValueAsSymbol(child) == FB_SMO_SYMBOL_TRUE);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "{a=ib=T}", 7);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 2);
    child = Fb_Smo_GetDescendantByPath(smo, "a");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_INTEGER);
    CHECK(strcmp(Fb_Smo_GetName(child), "a") == 0);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 7);
    child = Fb_Smo_GetDescendantByPath(smo, "b");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_SYMBOL);
    CHECK(strcmp(Fb_Smo_GetName(child), "b") == 0);
    CHECK(Fb_Smo_GetValueAsSymbol(child) == FB_SMO_SYMBOL_TRUE);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "{a=ifoo=[is[i{=s}]b]bar={}fox=[{}]}", 13, 14, "hello", 15, "blabla", "coucou", bytes, sizeof(bytes));
    CHECK(smo != NULL);
    child = Fb_Smo_GetDescendantByPath(smo, "a");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 13);
    child = Fb_Smo_GetDescendantByPath(smo, "foo");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_ARRAY);
    child = Fb_Smo_GetDescendantByPath(smo, "foo[1]");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_STRING);
    CHECK(strcmp(Fb_Smo_GetValueAsString(child), "hello") == 0);
    child = Fb_Smo_GetDescendantByPath(smo, "foo[2][1].blabla");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_STRING);
    CHECK(strcmp(Fb_Smo_GetValueAsString(child), "coucou") == 0);
    child = Fb_Smo_GetDescendantByPath(smo, "fox");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_ARRAY);
    child = Fb_Smo_GetDescendantByPath(smo, "fox[0]");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(child) == 0);
    Fb_Smo_Destroy(smo);

    smo = Fb_Smo_Create(NULL, "{=i}", "", 14);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetChildrenCount(smo) == 1);
    child = Fb_Smo_GetChildByName(smo, "");
    CHECK(child != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(child) == 14);
    Fb_Smo_Destroy(smo);


    smo = Fb_Smo_Create(NULL, "");
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "ii", 1,2);
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "{i}", 1,2);
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "i{", 1,2);
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "i}", 1,2);
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "i[", 1,2);
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "i]", 1,2);
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "[i}", 1);
    CHECK(smo == NULL);

    smo = Fb_Smo_Create(NULL, "{foo=i]", 1);
    CHECK(smo == NULL);

    return 0;
}

/*----------------------------------------------------------------------
|   RandomTreeTest
+---------------------------------------------------------------------*/
static int
RandomTreeTest(unsigned int build_iterations, unsigned int destroy_iterations, unsigned int with_serialization)
{
    Fb_Smo* tree = NULL;
    Fb_SmoAllocator* factory = &GlobalTestSmoAllocator.base;
    unsigned int r;
    int result;

    result = CreateRandomTree(factory, build_iterations, &tree);
    if (result) return result;

    /* destroy the whole tree */
    Fb_Smo_Destroy(tree);
    CHECK(GlobalTestSmoAllocator.total_allocated == 0);

    /* destroy random nodes in the tree */
    for (r=0; r<destroy_iterations; r++) {
        result = CreateRandomTree(factory, build_iterations, &tree);
        if (result) return result;

        /* test with serialization and deserialization */
        if (with_serialization) {
            unsigned int size1 = 0;
            unsigned int size2 = 0;
            uint8_t*     serialized1;
            uint8_t*     serialized2;
            Fb_Smo*      deserialized;
            result = Fb_Smo_Serialize(tree, FB_SMO_SERIALIZATION_FORMAT_CBOR, NULL, &size1);
            CHECK(result == FB_SMO_SUCCESS);
            serialized1 = (uint8_t*)malloc(size1);
            CHECK(serialized1 != NULL);
            result = Fb_Smo_Serialize(tree, FB_SMO_SERIALIZATION_FORMAT_CBOR, serialized1, &size1);
            CHECK(result == FB_SMO_SUCCESS);
            result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, serialized1, size1, &deserialized);
            CHECK(result == FB_SMO_SUCCESS);
            CHECK(deserialized != NULL);
            result = Fb_Smo_Serialize(deserialized, FB_SMO_SERIALIZATION_FORMAT_CBOR, NULL, &size2);
            CHECK(result == FB_SMO_SUCCESS);
            CHECK(size1 == size2);
            serialized2 = (uint8_t*)malloc(size2);
            CHECK(serialized2 != NULL);
            result = Fb_Smo_Serialize(deserialized, FB_SMO_SERIALIZATION_FORMAT_CBOR, serialized2, &size2);
            CHECK(result == FB_SMO_SUCCESS);
            CHECK(memcmp(serialized1, serialized2, size1) == 0);

            Fb_Smo_Destroy(deserialized);
            free(serialized1);
            free(serialized2);
        }

        while (Fb_Smo_GetFirstChild(tree)) {
            unsigned int steps = (unsigned int)(rand()%10);
            unsigned int i;
            Fb_Smo* smo = tree;

            /*printf("%d\n", (int)GlobalTestSmoAllocator.total_allocated);*/

            for (i=0; i<steps; i++) {
                if (Fb_Smo_GetType(smo) == FB_SMO_TYPE_ARRAY || Fb_Smo_GetType(smo) == FB_SMO_TYPE_OBJECT) {
                    if (Fb_Smo_GetFirstChild(smo)) {
                        smo = Fb_Smo_GetFirstChild(smo);
                        CHECK(smo);
                    }
                } else {
                    if (Fb_Smo_GetNext(smo)) {
                        smo = Fb_Smo_GetNext(smo);
                        CHECK(smo);
                    }
                }
            }
            if (smo != tree) Fb_Smo_Destroy(smo);
        }

        Fb_Smo_Destroy(tree);
        CHECK(GlobalTestSmoAllocator.total_allocated == 0);
    }

    return 0;
}

/*----------------------------------------------------------------------
|   test data
+---------------------------------------------------------------------*/
static uint8_t cbor_data_01[] = {(0<<5) | 17};
static uint8_t cbor_data_02[] = {(0<<5) | 24, 0x0A};
static uint8_t cbor_data_03[] = {(0<<5) | 24, 0xFF};
static uint8_t cbor_data_04[] = {(0<<5) | 25, 0x01, 0x02};
static uint8_t cbor_data_05[] = {(0<<5) | 25, 0xFF, 0xFF};
static uint8_t cbor_data_06[] = {(0<<5) | 26, 0x01, 0x02, 0x03, 0x04};
static uint8_t cbor_data_07[] = {(0<<5) | 26, 0x7F, 0xFF, 0xFF, 0xFF};
static uint8_t cbor_data_08[] = {(0<<5) | 26, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t cbor_data_09[] = {(0<<5) | 26, 0x80, 0x00, 0x00, 0x00};
static uint8_t cbor_data_0A[] = {(0<<5) | 27, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
static uint8_t cbor_data_0B[] = {(0<<5) | 27, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t cbor_data_0C[] = {(0<<5) | 27, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t cbor_data_0D[] = {(0<<5) | 27, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static uint8_t cbor_data_11[] = {(1<<5) | 17};
static uint8_t cbor_data_12[] = {(1<<5) | 24, 0x0A};
static uint8_t cbor_data_13[] = {(1<<5) | 24, 0xFF};
static uint8_t cbor_data_14[] = {(1<<5) | 25, 0x01, 0x02};
static uint8_t cbor_data_15[] = {(1<<5) | 25, 0xFF, 0xFF};
static uint8_t cbor_data_16[] = {(1<<5) | 26, 0x01, 0x02, 0x03, 0x04};
static uint8_t cbor_data_17[] = {(1<<5) | 26, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t cbor_data_18[] = {(1<<5) | 27, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t cbor_data_19[] = {(1<<5) | 27, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
static uint8_t cbor_data_1A[] = {(1<<5) | 27, 0x7F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t cbor_data_1B[] = {(1<<5) | 27, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/*----------------------------------------------------------------------
|   CborSerializationTest
+---------------------------------------------------------------------*/
static int
CborSerializationTest()
{
    unsigned int size;
    uint8_t scratchpad[32];
    Fb_Smo* smo;
    uint8_t* serialized;
    int result;

    smo = Fb_Smo_CreateInteger(NULL, 17);
    CHECK(smo != NULL);
    size = 0;
    result = Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, scratchpad, &size);
    CHECK(result = FB_SMO_ERROR_NOT_ENOUGH_SPACE);
    result = Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, NULL, &size);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(size == (unsigned int)sizeof(cbor_data_01));
    size += 1;
    serialized = (uint8_t*)malloc(size);
    result = Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, serialized, &size);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(size == (unsigned int)sizeof(cbor_data_01));
    CHECK(memcmp(serialized, cbor_data_01, size) == 0);
    Fb_Smo_Destroy(smo);
    free(serialized);

    smo = Fb_Smo_CreateInteger(NULL, 0xFF);
    CHECK(smo != NULL);
    size = 0;
    result = Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, scratchpad, &size);
    CHECK(result = FB_SMO_ERROR_NOT_ENOUGH_SPACE);
    result = Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, NULL, &size);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(size == (unsigned int)sizeof(cbor_data_03));
    size += 1;
    serialized = (uint8_t*)malloc(size);
    result = Fb_Smo_Serialize(smo, FB_SMO_SERIALIZATION_FORMAT_CBOR, serialized, &size);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(size == (unsigned int)sizeof(cbor_data_03));
    CHECK(memcmp(serialized, cbor_data_03, size) == 0);
    Fb_Smo_Destroy(smo);
    free(serialized);

    return 0;
}

/*----------------------------------------------------------------------
|   CborDeserializationTest
+---------------------------------------------------------------------*/
static int
CborDeserializationTest()
{
    Fb_Smo* smo;
    int     result;

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_01, sizeof(cbor_data_01), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 17);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_02, sizeof(cbor_data_02), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0x0A);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_03, sizeof(cbor_data_03), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0xFF);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_04, sizeof(cbor_data_04), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0x0102);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_05, sizeof(cbor_data_05), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0xFFFF);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_06, sizeof(cbor_data_06), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0x01020304);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_07, sizeof(cbor_data_07), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 2147483647);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_08, sizeof(cbor_data_08), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0xFFFFFFFF);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_09, sizeof(cbor_data_09), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0x80000000);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_0A, sizeof(cbor_data_0A), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0x0102030405060708LL);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_0B, sizeof(cbor_data_0B), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == 0x7FFFFFFFFFFFFFFFLL);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_0C, sizeof(cbor_data_0C), &smo);
    CHECK(result == FB_SMO_ERROR_OVERFLOW);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_0D, sizeof(cbor_data_0D), &smo);
    CHECK(result == FB_SMO_ERROR_OVERFLOW);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_11, sizeof(cbor_data_11), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -18);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_12, sizeof(cbor_data_12), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -11);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_13, sizeof(cbor_data_13), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -256);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_14, sizeof(cbor_data_14), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -259);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_15, sizeof(cbor_data_15), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -65536);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_16, sizeof(cbor_data_16), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -16909061);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_17, sizeof(cbor_data_17), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -(int64_t)0xFFFFFFFF-1);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_18, sizeof(cbor_data_18), &smo);
    CHECK(result == FB_SMO_ERROR_OVERFLOW);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_19, sizeof(cbor_data_19), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -(int64_t)0x0102030405060709);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_1A, sizeof(cbor_data_1A), &smo);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(smo != NULL);
    CHECK(Fb_Smo_GetValueAsInteger(smo) == -(int64_t)0x7FFFFFFFFFFFFFFF-1);
    Fb_Smo_Destroy(smo);

    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, cbor_data_1B, sizeof(cbor_data_1B), &smo);
    CHECK(result == FB_SMO_ERROR_OVERFLOW);

    return 0;
}

/*----------------------------------------------------------------------
|   test object set value
+---------------------------------------------------------------------*/
static int
SetObjectValueTest(void)
{
    Fb_Smo* smo;
    Fb_Smo* child;
    Fb_SmoAllocator* factory = &GlobalTestSmoAllocator.base;
    uint8_t workspace[64];
    unsigned int i;
    unsigned int bytes_size;
    const uint8_t* bytes;

    for (i=0; i<2; i++) {
        if (i==1) factory = NULL;

        /* strings */
        smo = Fb_Smo_CreateString(factory, "Longer string to short string", 0);
        smo = Fb_Smo_SetValueAsString(smo, "Short string", 12);
        CHECK(smo);
        CHECK(!strcmp(Fb_Smo_GetValueAsString(smo), "Short string"));
        Fb_Smo_Destroy(smo);

        smo = Fb_Smo_CreateString(factory, "String", 0);
        smo = Fb_Smo_SetValueAsString(smo, "Test string", 0);
        CHECK(smo);
        CHECK(!strcmp(Fb_Smo_GetValueAsString(smo), "Test string"));
        Fb_Smo_Destroy(smo);
        smo = Fb_Smo_CreateString(factory, "", 0);
        smo = Fb_Smo_SetValueAsString(smo, "Test string", 11);
        CHECK(smo);
        CHECK(!strcmp(Fb_Smo_GetValueAsString(smo), "Test string"));
        Fb_Smo_Destroy(smo);

        smo = Fb_Smo_CreateString(factory, "", 0);
        smo = Fb_Smo_SetValueAsString(smo, "Test string", 4);
        CHECK(smo);
        CHECK(!strcmp(Fb_Smo_GetValueAsString(smo), "Test"));
        Fb_Smo_Destroy(smo);

        smo = Fb_Smo_Create(NULL, "{foo=[is]}", 14, "hello");
        CHECK(smo != NULL);
        child = Fb_Smo_GetDescendantByPath(smo, "foo[1]");
        CHECK(Fb_Smo_GetType(child) == FB_SMO_TYPE_STRING);
        child = Fb_Smo_SetValueAsString(child, "Welcome", 7);
        CHECK(strcmp(Fb_Smo_GetValueAsString(child), "Welcome") == 0);
        CHECK(Fb_Smo_GetParent(child) == Fb_Smo_GetDescendantByPath(smo, "foo"));
        CHECK(Fb_Smo_GetNext(child) == Fb_Smo_GetDescendantByPath(smo, "foo[2]"));
        CHECK(Fb_Smo_GetFirstChild(child) == NULL);
        Fb_Smo_Destroy(smo);

        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* bytes */
        workspace[0] = 1;
        workspace[1] = 2;
        workspace[2] = 3;
        workspace[3] = 4;
        workspace[4] = 5;
        workspace[5] = 6;
        smo = Fb_Smo_CreateBytes(factory, NULL, 0);
        smo = Fb_Smo_SetValueAsBytes(smo, workspace, 5);
        bytes = Fb_Smo_GetValueAsBytes(smo, &bytes_size);
        CHECK(bytes != NULL);
        CHECK(bytes_size == 5);
        CHECK(bytes[0] == 1);
        CHECK(bytes[1] == 2);
        CHECK(bytes[2] == 3);
        CHECK(bytes[3] == 4);
        CHECK(bytes[4] == 5);
        Fb_Smo_Destroy(smo);

        workspace[0] = 1;
        workspace[1] = 2;
        workspace[2] = 3;
        workspace[3] = 4;
        workspace[4] = 5;
        workspace[5] = 6;
        smo = Fb_Smo_CreateBytes(factory, workspace, 2);
        workspace[0] = 255;
        workspace[1] = 254;
        workspace[2] = 253;
        workspace[3] = 252;
        workspace[4] = 251;
        smo = Fb_Smo_SetValueAsBytes(smo, workspace, 5);
        bytes = Fb_Smo_GetValueAsBytes(smo, &bytes_size);
        CHECK(bytes != NULL);
        CHECK(bytes_size == 5);
        CHECK(bytes[0] == 255);
        CHECK(bytes[1] == 254);
        CHECK(bytes[2] == 253);
        CHECK(bytes[3] == 252);
        CHECK(bytes[4] == 251);
        Fb_Smo_Destroy(smo);

        smo = Fb_Smo_CreateBytes(factory, workspace, 6);
        workspace[0] = 255;
        workspace[1] = 254;
        smo = Fb_Smo_SetValueAsBytes(smo, workspace, 2);
        bytes = Fb_Smo_GetValueAsBytes(smo, &bytes_size);
        CHECK(bytes != NULL);
        CHECK(bytes_size == 2);
        CHECK(bytes[0] == 255);
        CHECK(bytes[1] == 254);
        Fb_Smo_Destroy(smo);

        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* integers */
        smo = Fb_Smo_CreateInteger(factory, 89);
        smo = Fb_Smo_SetValueAsInteger(smo, 312);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsInteger(smo) == 312);
        Fb_Smo_Destroy(smo);

        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* floats */
        smo = Fb_Smo_CreateFloat(factory, 1.2345);
        smo = Fb_Smo_SetValueAsFloat(smo, 3.4567);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsFloat(smo) == 3.4567);
        Fb_Smo_Destroy(smo);

        CHECK(GlobalTestSmoAllocator.total_allocated == 0);

        /* symbols */
        smo = Fb_Smo_CreateSymbol(factory, FB_SMO_SYMBOL_FALSE);
        smo = Fb_Smo_SetValueAsSymbol(smo, FB_SMO_SYMBOL_TRUE);
        CHECK(smo);
        CHECK(Fb_Smo_GetValueAsSymbol(smo) == FB_SMO_SYMBOL_TRUE);
        Fb_Smo_Destroy(smo);

        CHECK(GlobalTestSmoAllocator.total_allocated == 0);
    }

    return 0;
}

/*----------------------------------------------------------------------
|   subtree serialization
+---------------------------------------------------------------------*/
static int
SubtreeSerializationTest()
{
    // create a tree with {"p0": 9999, "params": { "x": 1234, "y": 5678}}
    Fb_Smo* o1 = Fb_Smo_CreateObject(NULL);
    CHECK(o1 != NULL);
    Fb_Smo* o2 = Fb_Smo_CreateObject(NULL);
    CHECK(o2 != NULL);
    Fb_Smo* o3 = Fb_Smo_CreateInteger(NULL, 1234);
    CHECK(o3 != NULL);
    Fb_Smo* o4 = Fb_Smo_CreateInteger(NULL, 5678);
    CHECK(o4 != NULL);
    Fb_Smo* o5 = Fb_Smo_CreateInteger(NULL, 9999);
    CHECK(o5 != NULL);
    int result = Fb_Smo_AddChild(o2, "x", 0, o3);
    CHECK(result == FB_SMO_SUCCESS);
    result = Fb_Smo_AddChild(o2, "y", 0, o4);
    CHECK(result == FB_SMO_SUCCESS);
    result = Fb_Smo_AddChild(o1, "p0", 0, o5);
    CHECK(result == FB_SMO_SUCCESS);
    result = Fb_Smo_AddChild(o1, "params", 0, o2);
    CHECK(result == FB_SMO_SUCCESS);

    // serialize the "params" subtree
    uint8_t buffer[128];
    unsigned int buffer_size = sizeof(buffer);
    Fb_Smo* params = Fb_Smo_GetChildByName(o1, "params");
    CHECK(params != NULL);
    result = Fb_Smo_Serialize(params, FB_SMO_SERIALIZATION_FORMAT_CBOR, buffer, &buffer_size);
    CHECK(result == FB_SMO_SUCCESS);

    // deserialize the buffer and check that we have "only" the "params" object
    Fb_Smo* d1 = NULL;
    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, buffer, buffer_size, &d1);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(d1 != NULL);
    CHECK(Fb_Smo_GetType(d1) == FB_SMO_TYPE_OBJECT);
    CHECK(Fb_Smo_GetNext(d1) == NULL);
    Fb_Smo_Destroy(d1);

    // serialize the "p0" subtree
    buffer_size = sizeof(buffer);
    Fb_Smo* p0 = Fb_Smo_GetChildByName(o1, "p0");
    CHECK(params != NULL);
    result = Fb_Smo_Serialize(p0, FB_SMO_SERIALIZATION_FORMAT_CBOR, buffer, &buffer_size);
    CHECK(result == FB_SMO_SUCCESS);

    // deserialize the buffer and check that we have "only" the "p0" object
    result = Fb_Smo_Deserialize(NULL, NULL, FB_SMO_SERIALIZATION_FORMAT_CBOR, buffer, buffer_size, &d1);
    CHECK(result == FB_SMO_SUCCESS);
    CHECK(d1 != NULL);
    CHECK(Fb_Smo_GetType(d1) == FB_SMO_TYPE_INTEGER);
    CHECK(Fb_Smo_GetNext(d1) == NULL);
    Fb_Smo_Destroy(d1);

    Fb_Smo_Destroy(o1);

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
    result = SimpleObjectCreationTest();
    if (result) return 1;

    result = CoercionTest();
    if (result) return 1;

    result = TestSimpleBlockAllocator();
    if (result) return 1;

    result = TestGrowOnlyAllocator();
    if (result) return 1;

    result = TestOutOfMemory();
    if (result) return 1;

    result = ContainerTest();
    if (result) return 1;

    result = PathTest();
    if (result) return 1;

    result = VarargsTest();
    if (result) return 1;

    /* random trees */
    result = RandomTreeTest(10000, 100, 0);
    if (result) return 1;

    /* random trees with serialization */
    result = RandomTreeTest(10000, 100, 1);
    if (result) return 1;

    /* CBOR serialization test */
    result = CborSerializationTest();
    if (result) return 1;

    /* CBOR deserialization test */
    result = CborDeserializationTest();
    if (result) return 1;

    /* Set object value test */
    result = SetObjectValueTest();
    if (result) return 1;

    /* subtree serialization */
    result = SubtreeSerializationTest();
    if (result) return 1;

    return 0;
}
