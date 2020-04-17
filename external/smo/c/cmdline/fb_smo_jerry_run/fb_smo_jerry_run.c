/**
*
* @file: fb_smo_jerry_run.c
*
* @copyright
* Copyright 2017-2020 Fitbit, Inc
* SPDX-License-Identifier: Apache-2.0
*
* @author Gilles Boccon-Gibod
*
* @date 2017-03-20
*
* @details
*
* JerryScript runner with CBOR functionality
*
*/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "jerryscript.h"
#include "jerryscript-port.h"
#include "fb_smo_jerryscript.h"

typedef struct {
    Fb_SmoJerryDataSource base;
    FILE*        file;
    uint8_t*     buffer;
    unsigned int buffer_offset;
    unsigned int buffer_available;
    unsigned int buffer_size;
    unsigned int buffer_increment;
} FileDataSource;

static uint8_t *
read_file(const char *file_name, size_t *out_size_p)
{
    FILE *file = fopen (file_name, "r");
    if (file == NULL) {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to open file: %s\n", file_name);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    *out_size_p = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t* buffer = (uint8_t*)malloc(*out_size_p);
    if (buffer == NULL) {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to allocate memory\n");
        fclose (file);
        return NULL;
    }

    size_t bytes_read = fread (buffer, 1u, *out_size_p, file);
    if (!bytes_read) {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to read file: %s\n", file_name);
        fclose (file);
        return NULL;
    }

    fclose (file);

    return buffer;
}

static unsigned int
FileDataSource_get_more(Fb_SmoJerryDataSource* _self, const uint8_t** buffer, unsigned int* bytes_available) {
    FileDataSource* self = (FileDataSource*)_self;
    unsigned int bytes_to_read;
    size_t bytes_read;

    // default values
    *buffer = self->buffer+self->buffer_offset;
    *bytes_available = self->buffer_available;

    if (self->buffer_offset) {
        // shift the buffer back
        if (self->buffer_available) {
            memmove(self->buffer, self->buffer+self->buffer_offset, self->buffer_available);
        }
        self->buffer_offset = 0;
        *buffer = self->buffer;
    } else if (self->buffer_available == self->buffer_size) {
        // grow the buffer
        unsigned int new_buffer_size = self->buffer_size + self->buffer_increment;
        self->buffer = realloc(self->buffer, new_buffer_size);
        if (self->buffer == NULL) {
            return FB_SMO_ERROR_OUT_OF_MEMORY;
        }
        self->buffer_size = new_buffer_size;
        *buffer = self->buffer;
    }

    // read more
    bytes_to_read = self->buffer_size-self->buffer_available;
    bytes_read = fread(self->buffer+self->buffer_available, 1, bytes_to_read, self->file);
    // ignore errors here, this is just an example
    self->buffer_available += bytes_read;
    *bytes_available = self->buffer_available;

    return (unsigned int)bytes_read;
}

static int
FileDataSource_advance(Fb_SmoJerryDataSource* _self, unsigned int bytes_used)
{
    FileDataSource* self = (FileDataSource*)_self;

    if (bytes_used <= self->buffer_available) {
        self->buffer_offset    += bytes_used;
        self->buffer_available -= bytes_used;

        return 0;
    } else {
        // something's wrong, reset
        self->buffer_offset    = 0;
        self->buffer_available = 0;

        return -1;
    }
}

static int
FileDataSource_init(FileDataSource* self, const char* filename, unsigned int buffer_increment)
{
    memset(self, 0, sizeof(*self));

    self->buffer_increment = buffer_increment;
    self->file = fopen(filename, "r");
    if (self->file == NULL) {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Error: failed to open file: %s\n", filename);
        return -1;
    }

    self->base.get_more = FileDataSource_get_more;
    self->base.advance  = FileDataSource_advance;
    return 0;
}

static void
FileDataSource_deinit(FileDataSource* self)
{
    if (self->buffer) {
        free(self->buffer);
    }
    fclose(self->file);
}

static void
parse_cbor(const char* filename)
{
    jerry_value_t  obj;
    FileDataSource source;
    int            result;

    result = FileDataSource_init(&source, filename, 256);
    if (result != FB_SMO_SUCCESS) {
        jerry_port_log(JERRY_LOG_LEVEL_ERROR, "Error: FileDataSource_init returned %d\n", result);
        return;
    }

    result = Fb_Smo_Deserialize_CBOR_ToJerryFromSource(NULL, &source.base, &obj);
    if (result != FB_SMO_SUCCESS) {
        jerry_port_log(JERRY_LOG_LEVEL_ERROR, "Error: Fb_Smo_Deserialize_CBOR_ToJerryFromSource returned %d\n", result);
        return;
    }

    {
        jerry_value_t global = jerry_get_global_object();
        jerry_value_t obj_name = jerry_create_string((const jerry_char_t*)"loadedFromFile");
        jerry_set_property(global, obj_name, obj);
        jerry_release_value(obj);
        jerry_release_value(obj_name);
        jerry_release_value(global);
    }

    FileDataSource_deinit(&source);
}

static void
print_jerry_string(jerry_value_t str_obj)
{
    jerry_length_t utf8_size = jerry_get_utf8_string_size(str_obj);
    if (utf8_size) {
        jerry_char_t* utf8_buffer = (jerry_char_t*)malloc(utf8_size+1);
        if (utf8_buffer) {
            utf8_buffer[utf8_size] = 0;
            jerry_string_to_utf8_char_buffer(str_obj, utf8_buffer, utf8_size);
            printf("\"%s\"", utf8_buffer);
            free(utf8_buffer);
        }
    }
}

static void dump_cbor(jerry_value_t obj);

static bool
cbor_foreach(const jerry_value_t property_name,
             const jerry_value_t property_value,
             void*               self)
{
    printf("name=");
    print_jerry_string(property_name);
    printf(", value=");
    dump_cbor(property_value);
    printf("\n");

    return true;
}

static void
dump_cbor(jerry_value_t obj)
{
    bool result;

    if (jerry_value_is_object(obj)) {
        if (jerry_value_is_array(obj)) {
            uint32_t array_length = jerry_get_array_length(obj);
            uint32_t i;
            printf("[\n");
            for (i=0; i<array_length; i++) {
                jerry_value_t item = jerry_get_property_by_index(obj, i);
                if (i > 0) {
                    printf(", ");
                }
                dump_cbor(item);
                jerry_release_value(item);
            }
            printf("]\n");
        } else {
            printf("{\n");
            result = jerry_foreach_object_property(obj, cbor_foreach, NULL);
            printf("}\n");
        }
    } else if (jerry_value_is_null(obj)) {
        printf("(null)");
    } else if (jerry_value_is_boolean(obj)) {
        bool value = jerry_get_boolean_value(obj);
        printf("%s\n", value?"true":"false");
    } else if (jerry_value_is_undefined(obj)) {
        printf("(undefined)");
    } else if (jerry_value_is_number(obj)) {
        double value = jerry_get_number_value(obj);
        int64_t int_value = (int64_t)value;
        if ((double)int_value == value) {
            printf("%lld", int_value);
        } else {
            printf("%f", value);
        }
    } else if (jerry_value_is_string(obj)) {
        print_jerry_string(obj);
    } else {
        printf("(unknown)");
    }
}

static void
print_help (char *name)
{
  printf("Usage: %s [CBOR-FILE] [JS-FILE]\n", name);
}

int
main (int argc,
      char **argv)
{
    const char* cbor_file_name;
    const char* js_file_name;
    size_t      js_file_size = 0;

    if (argc != 3)
    {
        print_help (argv[0]);
        return 0;
    }

    cbor_file_name = argv[1];
    js_file_name = argv[2];

    // load the JS file
    jerry_char_t *js_source = read_file(js_file_name, &js_file_size);
    if (js_source == NULL) {
        jerry_port_log(JERRY_LOG_LEVEL_ERROR, "Error: failed to load CBOR file: %s\n", cbor_file_name);
        return 1;
    }

    // execute the JS
    jerry_init (JERRY_INIT_EMPTY);
    jerry_value_t ret_value = jerry_create_undefined ();

    ret_value = jerry_parse (js_source, js_file_size, false);

    if (!jerry_value_is_error (ret_value)) {
        parse_cbor(cbor_file_name);

        jerry_value_t func_val = ret_value;
        ret_value = jerry_run (func_val);
        jerry_release_value (func_val);
    }

    int ret_code = 0;

    if (jerry_value_is_error (ret_value)) {
        jerry_port_log (JERRY_LOG_LEVEL_ERROR, "Unhandled exception: Script Error!\n");
        ret_code = 1;
    }

    dump_cbor(ret_value);

    jerry_release_value (ret_value);
    jerry_cleanup ();

    free(js_source);

    return ret_code;
}
