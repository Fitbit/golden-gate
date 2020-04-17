// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_common.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_INSPECT)
{
  void setup(void) {
  }

  void teardown(void) {
  }
};

typedef struct {
    GG_IMPLEMENTS(GG_Inspector);

    char*    string_name;
    char*    string_value;
    char*    boolean_name;
    bool     boolean_value;
    char*    integer_name;
    int64_t  integer_value;
    char*    float_name;
    double   float_value;
    char*    bytes_name;
    uint8_t* bytes;
    size_t   bytes_size;
    char*    ext_name;
    uint32_t ext_type;
    void*    ext_data;
    size_t   ext_data_size;
    char*    array_name;
    int64_t  array_values[3];
    size_t   array_value_count;
    char*    object_name;
    char*    object_property_name;
    char*    object_property_value;

    bool     in_object;
    bool     in_array;
} TestInspector;

static void
TestInspector_OnObjectStart(GG_Inspector* _self, const char* name)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    if (self->in_object) return;
    self->in_object = true;
    if (name && !self->object_name) {
        self->object_name = strdup(name);
    }
}

static void
TestInspector_OnObjectEnd(GG_Inspector* _self)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    self->in_object = false;
}

static void
TestInspector_OnArrayStart(GG_Inspector* _self, const char* name)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    if (self->in_array) return;
    self->in_array = true;
    if (name && !self->array_name) {
        self->array_name = strdup(name);
    }
    self->array_value_count = 0;
}

static void
TestInspector_OnArrayEnd(GG_Inspector* _self)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    self->in_array = false;
}

static void
TestInspector_OnInspectable(GG_Inspector* self, const char* name, GG_Inspectable* inspectable)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(name);
    GG_COMPILER_UNUSED(inspectable);
}

static void
TestInspector_OnString(GG_Inspector* _self, const char* name, const char* value)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);

    if (self->in_object) {
        if (name && !self->object_property_name) {
            self->object_property_name = strdup(name);
        }
        if (value && !self->object_property_value) {
            self->object_property_value = strdup(value);
        }
    } else {
        if (name && !self->string_name) {
            self->string_name = strdup(name);
        }
        if (value && !self->string_value) {
            self->string_value = strdup(value);
        }
    }
}

static void
TestInspector_OnBoolean(GG_Inspector* _self, const char* name, bool value)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    if (name && !self->float_name) {
        self->boolean_name = strdup(name);
    }
    self->boolean_value = value;
}

static void
TestInspector_OnInteger(GG_Inspector*          _self,
                        const char*            name,
                        int64_t                value,
                        GG_InspectorFormatHint format_hint)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    GG_COMPILER_UNUSED(format_hint);

    if (self->in_array) {
        if (self->array_value_count < GG_ARRAY_SIZE(self->array_values)) {
            self->array_values[self->array_value_count++] = value;
        }
    } else {
        if (name && !self->integer_name) {
            self->integer_name = strdup(name);
        }
        self->integer_value = value;
    }
}

static void
TestInspector_OnFloat(GG_Inspector* _self, const char* name, double value)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    if (name && !self->float_name) {
        self->float_name = strdup(name);
    }
    self->float_value = value;
}

static void
TestInspector_OnBytes(GG_Inspector*  _self,
                      const char*    name,
                      const uint8_t* data,
                      size_t         data_size)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    if (name && !self->bytes_name) {
        self->bytes_name = strdup(name);
    }
    if (data && data_size && !self->bytes) {
        self->bytes_size = data_size;
        self->bytes = (uint8_t*)malloc(data_size);
        memcpy(self->bytes, data, data_size);
    }
}

static void
TestInspector_OnExtensible(GG_Inspector* _self,
                           const char*   name,
                           uint32_t      data_type,
                           const void*   data,
                           size_t        data_size)
{
    TestInspector* self = GG_SELF(TestInspector, GG_Inspector);
    if (data_type != 0x01020304) return;
    if (name && !self->ext_name) {
        self->ext_name = strdup(name);
    }
    if (data && data_size && !self->ext_data) {
        self->ext_data_size = data_size;
        self->ext_data = malloc(data_size);
        memcpy(self->ext_data, data, data_size);
    }
}

GG_IMPLEMENT_INTERFACE(TestInspector, GG_Inspector) {
    TestInspector_OnObjectStart,
    TestInspector_OnObjectEnd,
    TestInspector_OnArrayStart,
    TestInspector_OnArrayEnd,
    TestInspector_OnInspectable,
    TestInspector_OnString,
    TestInspector_OnBoolean,
    TestInspector_OnInteger,
    TestInspector_OnFloat,
    TestInspector_OnBytes,
    TestInspector_OnExtensible
};

static void
TestInspector_Init(TestInspector* self)
{
    memset(self, 0, sizeof(*self));
    GG_SET_INTERFACE(self, TestInspector, GG_Inspector);
}

static void
TestInspector_Deinit(TestInspector* self)
{
    if (self->string_name) free(self->string_name);
    if (self->string_value) free(self->string_value);
    if (self->boolean_name) free(self->boolean_name);
    if (self->integer_name) free(self->integer_name);
    if (self->float_name) free(self->float_name);
    if (self->bytes_name) free(self->bytes_name);
    if (self->bytes) free(self->bytes);
    if (self->ext_name) free(self->ext_data);
    if (self->object_name) free(self->object_name);
    if (self->object_property_name) free(self->object_property_name);
    if (self->object_property_value) free(self->object_property_value);
    if (self->array_name) free(self->array_name);
}

typedef struct {
    GG_IMPLEMENTS(GG_Inspectable);
} TestInspectable;

static uint8_t bytes[3] = { 1, 2, 3 };
static uint8_t ext_data[3] = { 4, 5, 6 };

static GG_Result
TestInspectable_Inspect(GG_Inspectable* self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(options);

    GG_Inspector_OnString(inspector, "foo_string", "bar");
    GG_Inspector_OnBoolean(inspector, "foo_boolean", true);
    GG_Inspector_OnInteger(inspector, "foo_integer", 12345, GG_INSPECTOR_FORMAT_HINT_NONE);
    GG_Inspector_OnFloat(inspector, "foo_float", 1.2345);
    GG_Inspector_OnBytes(inspector, "foo_bytes", bytes, sizeof(bytes));
    GG_Inspector_OnExtensible(inspector, "foo_ext", 0x01020304, ext_data, sizeof(ext_data));
    GG_Inspector_OnObjectStart(inspector, "foo_object");
    GG_Inspector_OnString(inspector, "foo_object_bar", "foo.bar");
    GG_Inspector_OnObjectEnd(inspector);
    GG_Inspector_OnArrayStart(inspector, "foo_array");
    GG_Inspector_OnInteger(inspector, NULL, 1, GG_INSPECTOR_FORMAT_HINT_NONE);
    GG_Inspector_OnInteger(inspector, NULL, 2, GG_INSPECTOR_FORMAT_HINT_NONE);
    GG_Inspector_OnInteger(inspector, NULL, 3, GG_INSPECTOR_FORMAT_HINT_NONE);
    GG_Inspector_OnArrayEnd(inspector);


    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(TestInspectable, GG_Inspectable) {
    .Inspect = TestInspectable_Inspect
};

static void
TestInspectable_Init(TestInspectable* self)
{
    memset(self, 0, sizeof(*self));
    GG_SET_INTERFACE(self, TestInspectable, GG_Inspectable);
}

static void
TestInspectable_Deinit(TestInspectable* self)
{
    GG_COMPILER_UNUSED(self);
}

TEST(GG_INSPECT, Test_BasicInspection) {
    TestInspectable inspectable;
    TestInspectable_Init(&inspectable);

    TestInspector inspector;
    TestInspector_Init(&inspector);

    GG_InspectionOptions options = {
        .verbosity = 3
    };
    GG_Result result = GG_Inspectable_Inspect(GG_CAST(&inspectable, GG_Inspectable),
                                              GG_CAST(&inspector, GG_Inspector),
                                              &options);
    LONGS_EQUAL(GG_SUCCESS, result);

    CHECK_FALSE(inspector.in_object);
    CHECK_FALSE(inspector.in_array);
    STRCMP_EQUAL("foo_string", inspector.string_name);
    STRCMP_EQUAL("bar", inspector.string_value);
    STRCMP_EQUAL("foo_boolean", inspector.boolean_name);
    LONGS_EQUAL(true, inspector.boolean_value);
    STRCMP_EQUAL("foo_integer", inspector.integer_name);
    LONGS_EQUAL(12345, inspector.integer_value);
    STRCMP_EQUAL("foo_float", inspector.float_name);
    DOUBLES_EQUAL(1.2345, inspector.float_value, 0.001);
    STRCMP_EQUAL("foo_bytes", inspector.bytes_name);
    LONGS_EQUAL(sizeof(bytes), inspector.bytes_size);
    MEMCMP_EQUAL(bytes, inspector.bytes, inspector.bytes_size);
    STRCMP_EQUAL("foo_ext", inspector.ext_name);
    LONGS_EQUAL(sizeof(ext_data), inspector.ext_data_size);
    MEMCMP_EQUAL(ext_data, inspector.ext_data, inspector.ext_data_size);
    STRCMP_EQUAL("foo_object", inspector.object_name);
    STRCMP_EQUAL("foo_object_bar", inspector.object_property_name);
    STRCMP_EQUAL("foo.bar", inspector.object_property_value);
    STRCMP_EQUAL("foo_array", inspector.array_name);
    LONGS_EQUAL(3, inspector.array_value_count);
    LONGS_EQUAL(1, inspector.array_values[0]);
    LONGS_EQUAL(2, inspector.array_values[1]);
    LONGS_EQUAL(3, inspector.array_values[2]);

    TestInspector_Deinit(&inspector);
    TestInspectable_Deinit(&inspectable);
}
