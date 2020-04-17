/**
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-02-14
 *
 * @details
 *
 * Object inspection
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdio.h>

#include "gg_inspect.h"
#include "gg_port.h"

#if defined(GG_CONFIG_ENABLE_INSPECTION)

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
void
GG_Inspector_OnObjectStart(GG_Inspector* self, const char* name)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnObjectStart(self, name);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnObjectEnd(GG_Inspector* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnObjectEnd(self);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnArrayStart(GG_Inspector* self, const char* name)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnArrayStart(self, name);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnArrayEnd(GG_Inspector* self)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnArrayEnd(self);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnInspectable(GG_Inspector* self, const char* name, GG_Inspectable* inspectable)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnInspectable(self, name, inspectable);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnString(GG_Inspector* self, const char* name, const char* value)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnString(self, name, value);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnBoolean(GG_Inspector* self, const char* name, bool value)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnBoolean(self, name, value);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnInteger(GG_Inspector*          self,
                       const char*            name,
                       int64_t                value,
                       GG_InspectorFormatHint format_hint)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnInteger(self, name, value, format_hint);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnFloat(GG_Inspector* self, const char* name, double value)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnFloat(self, name, value);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnBytes(GG_Inspector*          self,
                     const char*            name,
                     const uint8_t*         data,
                     size_t                 data_size)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnBytes(self, name, data, data_size);
}

//----------------------------------------------------------------------
void
GG_Inspector_OnExtensible(GG_Inspector*          self,
                          const char*            name,
                          uint32_t               data_type,
                          const void*            data,
                          size_t                 data_size)
{
    GG_ASSERT(self);
    GG_INTERFACE(self)->OnExtensible(self, name, data_type, data, data_size);
}

//----------------------------------------------------------------------
GG_Result
GG_Inspectable_Inspect(GG_Inspectable* self, GG_Inspector* inspector, const GG_InspectionOptions* options)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->Inspect(self, inspector, options);
}

/*----------------------------------------------------------------------
|   GG_LoggingInspector
|
|   NOTE: the GG_Inspector method implementation functions are
|   named in a way that makes them all the same length, so that when
|   the log handlers print the function name, the output is aligned.
+---------------------------------------------------------------------*/
#define GG_LOGGING_INSPECTOR_MAX_PREFIX_LENGTH 32
#define GG_LOGGING_INSPECTOR_MAX_BYTES         256 // max number of bytes we accept to print as hex

struct GG_LoggingInspector {
    GG_IMPLEMENTS(GG_Inspector);

    GG_LoggerReference logger_reference;
    int                logging_level;
    char               prefix[GG_LOGGING_INSPECTOR_MAX_PREFIX_LENGTH + 1];
    size_t             prefix_length;
    char               hex_buffer[2 * GG_LOGGING_INSPECTOR_MAX_BYTES + 1];
};

//----------------------------------------------------------------------
static void
GG_LoggingInspector_Indent(GG_LoggingInspector* self)
{
    // check bounds
    if (self->prefix_length + 2 > GG_LOGGING_INSPECTOR_MAX_PREFIX_LENGTH) {
        return;
    }

    // extend the prefix by two spaces
    self->prefix[self->prefix_length++] = ' ';
    self->prefix[self->prefix_length++] = ' ';
    self->prefix[self->prefix_length] = '\0';
}

//----------------------------------------------------------------------
static void
GG_LoggingInspector_Dedent(GG_LoggingInspector* self)
{
    // check bounds
    if (self->prefix_length < 2) {
        return;
    }

    // truncate the existing prefix
    self->prefix_length -= 2;
    self->prefix[self->prefix_length] = '\0';
}

//----------------------------------------------------------------------
static void
GG_LI_OS(GG_Inspector* _self, const char* name)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s{",
              self->prefix,
              name ? name : "",
              name ? ": " : "");
    GG_LoggingInspector_Indent(self);
}

//----------------------------------------------------------------------
static void
GG_LI_OE(GG_Inspector* _self)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    GG_LoggingInspector_Dedent(self);
    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s}",
              self->prefix);
}

//----------------------------------------------------------------------
static void
GG_LI_AS(GG_Inspector* _self, const char* name)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s[",
              self->prefix,
              name ? name : "",
              name ? ": " : "");
    GG_LoggingInspector_Indent(self);
}

//----------------------------------------------------------------------
static void
GG_LI_AE(GG_Inspector* _self)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    GG_LoggingInspector_Dedent(self);
    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s]",
              self->prefix);
}

//----------------------------------------------------------------------
static void
GG_LI_IS(GG_Inspector* self, const char* name, GG_Inspectable* inspectable)
{
    GG_LI_OS(self, name);
    GG_Inspectable_Inspect(inspectable, self, NULL);
    GG_LI_OE(self);
}

//----------------------------------------------------------------------
static void
GG_LI_ST(GG_Inspector* _self, const char* name, const char* value)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s%s",
              self->prefix,
              name ? name : "",
              name ? ": " : "",
              value);
}

//----------------------------------------------------------------------
static void
GG_LI_BO(GG_Inspector* _self, const char* name, bool value)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s%s",
              self->prefix,
              name ? name : "",
              name ? ": " : "",
              value ? "true" : "false");
}

//----------------------------------------------------------------------
static void
GG_LI_IN(GG_Inspector*          _self,
         const char*            name,
         int64_t                value,
         GG_InspectorFormatHint format_hint)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    char value_string[16];
    switch (format_hint) {
        case GG_INSPECTOR_FORMAT_HINT_NONE:
            snprintf(value_string, sizeof(value_string), "%" PRIi64, value);
            break;

        case GG_INSPECTOR_FORMAT_HINT_UNSIGNED:
            snprintf(value_string, sizeof(value_string), "%" PRIu64, (uint64_t)value);
            break;

        case GG_INSPECTOR_FORMAT_HINT_HEX:
            snprintf(value_string, sizeof(value_string), "0x%" PRIx64, value);
            break;
    }
    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s%s",
              self->prefix,
              name ? name : "",
              name ? ": " : "",
              value_string);
}

//----------------------------------------------------------------------
static void
GG_LI_FL(GG_Inspector* _self, const char* name, double value)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s%f",
              self->prefix,
              name ? name : "",
              name ? ": " : "",
              value);
}

//----------------------------------------------------------------------
static void
GG_LI_BY(GG_Inspector*  _self,
         const char*    name,
         const uint8_t* data,
         size_t         data_size)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    size_t bytes_to_print = GG_MIN(GG_LOGGING_INSPECTOR_MAX_BYTES, data_size);
    if (bytes_to_print) {
        GG_BytesToHex(data, bytes_to_print, self->hex_buffer, true);
    }
    self->hex_buffer[2 * bytes_to_print] = '\0';
    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s[%s]",
              self->prefix,
              name ? name : "",
              name ? ": " : "",
              self->hex_buffer);
}

//----------------------------------------------------------------------
static void
GG_LI_EX(GG_Inspector* _self,
         const char*   name,
         uint32_t      data_type,
         const void*   data,
         size_t        data_size)
{
    GG_LoggingInspector* self = GG_SELF(GG_LoggingInspector, GG_Inspector);

    size_t bytes_to_print = GG_MIN(GG_LOGGING_INSPECTOR_MAX_BYTES, data_size);
    if (bytes_to_print) {
        GG_BytesToHex(data, bytes_to_print, self->hex_buffer, true);
    }
    self->hex_buffer[2 * bytes_to_print] = '\0';
    GG_LOG_LL(self->logger_reference,
              self->logging_level,
              "%s%s%s[t=%08x,%s]",
              self->prefix,
              name ? name : "",
              name ? ": " : "",
              (int)data_type,
              self->hex_buffer);
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LoggingInspector, GG_Inspector) {
    .OnObjectStart = GG_LI_OS,
    .OnObjectEnd   = GG_LI_OE,
    .OnArrayStart  = GG_LI_AS,
    .OnArrayEnd    = GG_LI_AE,
    .OnInspectable = GG_LI_IS,
    .OnString      = GG_LI_ST,
    .OnBoolean     = GG_LI_BO,
    .OnInteger     = GG_LI_IN,
    .OnFloat       = GG_LI_FL,
    .OnBytes       = GG_LI_BY,
    .OnExtensible  = GG_LI_EX
};

//----------------------------------------------------------------------
GG_Result
GG_LoggingInspector_Create(const char*           logger_name,
                           unsigned int          logging_level,
                           GG_LoggingInspector** inspector)
{
    GG_LoggingInspector* self = GG_AllocateZeroMemory(sizeof(GG_LoggingInspector));
    if (!self) {
        return GG_ERROR_OUT_OF_MEMORY;
    }
    *inspector = self;

    // init the logger reference
    self->logger_reference.logger.name = logger_name;
    self->logging_level = (int)logging_level;

    // init the interfaces
    GG_SET_INTERFACE(self, GG_LoggingInspector, GG_Inspector);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_LoggingInspector_Destroy(GG_LoggingInspector* self)
{
    if (self == NULL) return;

    GG_ClearAndFreeObject(self, 1);
}

//----------------------------------------------------------------------
GG_Inspector*
GG_LoggingInspector_AsInspector(GG_LoggingInspector* self)
{
    return GG_CAST(self, GG_Inspector);
}


#endif
