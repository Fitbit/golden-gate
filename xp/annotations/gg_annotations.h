/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-07-06
 *
 * @details
 *
 * Type mappings and macros for loggable objects.
 */

#pragma once

#ifdef GG_CONFIG_ENABLE_ANNOTATIONS
/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include "xp/common/gg_logging.h"
#include "debug_annotations.pb.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define GG_LOG_ANNOTATION_HANDLER_NAME  "AnnotationHandler"

#define GG_ANNOTATIONS_SITE_LOG_OBJECT_TYPE_ID  GG_4CC('s', 'i', 't', 'e')

/**
 * GG_LogObject object sub-class for 'site' debug annotations.
 * This sub-class is used when the `type` field of the base class
 * is set to GG_ANNOTATIONS_SITE_LOG_OBJECT_TYPE_ID.
 */
typedef struct {
    GG_LogObject     base;
    site_log_Message top_level;
} GG_SiteDebugAnnotationLogObject;

/**
 * Construct a GG_LogObject struct given a subtype name and an initializer for the subtype
 */
#define GG_LOG_MAKE_SITE_OBJECT(_object_type, ...)               \
    ((GG_SiteDebugAnnotationLogObject) {                         \
        .base = {                                                \
            .type = GG_ANNOTATIONS_SITE_LOG_OBJECT_TYPE_ID       \
        },                                                       \
        .top_level =  {                                          \
            .which_type = site_log_Message_##_object_type##_tag, \
            .type._object_type = __VA_ARGS__                     \
        }                                                        \
    })

#define GG_LOG_MAKE_TOPLEVEL_OBJECT(_object_type, ...) GG_LOG_MAKE_SITE_OBJECT(_object_type, __VA_ARGS__)
#define GG_LOG_TYPEOF_TOPLEVEL_OBJECT GG_SiteDebugAnnotationLogObject

/**
 * Construct a GG_LogObject struct given a GoldenGate sub-subtype.
 * This is used by the GG_LOG_O_XXX macros, so as to not have to explicitly reference the 'golden_gate' field
 */
#define GG_LOG_MAKE_GG_OBJECT(_object_type, ...)                   \
    GG_LOG_MAKE_SITE_OBJECT(golden_gate, (site_log_GoldenGate) {   \
        .which_details = site_log_GoldenGate_##_object_type##_tag, \
        .details._object_type = __VA_ARGS__                        \
    })
#define GG_LOG_TYPEOF_GG_OBJECT GG_SiteDebugAnnotationLogObject

/**
 * Helper macros for creating GG Annotations
 */
#define GG_ANNOT_ERR(x)     site_log_GoldenGate_ErrorDetails_##x

#define GG_LOG_COMMS_ERROR(err)                                         \
    GG_LOG_O_SEVERE(comms_error, {.has_error_details = true,            \
                                  .error_details = GG_ANNOT_ERR(err),   \
                                  .has_error_code = false})

#define GG_LOG_COMMS_ERROR_CODE(err, code)                              \
    GG_LOG_O_SEVERE(comms_error, {.has_error_details = true,            \
                                  .error_details = GG_ANNOT_ERR(err),   \
                                  .has_error_code = true,               \
                                  .error_code = code})

// Ensure the this matches the length defined in GoldenGate.proto
#define GG_LOG_COMMS_ERROR_EXTRA_CONTEXT_MAX_SIZE (50)

#define GG_LOG_COMMS_ERROR_STRING(err, context)                                              \
    site_log_GoldenGate_CommsError object = (site_log_GoldenGate_CommsError) {               \
        .has_error_details = true,                                                           \
        .error_details = GG_ANNOT_ERR(err),                                                  \
        .has_error_extra_context = true,                                                     \
    };                                                                                       \
    snprintf(object.error_extra_context, sizeof(object.error_extra_context), "%s", context); \
    GG_LOG_O_SEVERE(comms_error, object);                                                    \

#define GG_ANNOT_EVENT(x) site_log_GoldenGate_EventDetails_##x

#define GG_LOG_COMMS_EVENT(event)                                          \
    GG_LOG_O_SEVERE(comms_event, {.has_event_details = true,               \
                                  .event_details = GG_ANNOT_EVENT(event)})

#if defined(__cplusplus)
}
#endif

#else

#define GG_LOG_COMMS_ERROR(err)
#define GG_LOG_COMMS_ERROR_CODE(err, code)
#define GG_LOG_COMMS_ERROR_EXTRA_CONTEXT_MAX_SIZE (0)
#define GG_LOG_COMMS_ERROR_STRING(err, extra_context)
#define GG_LOG_COMMS_EVENT(event)

#endif // GG_CONFIG_ENABLE_ANNOTATIONS
