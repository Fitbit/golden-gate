/**
 * @file
 * @brief Object inspection
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2019-02-14
*/

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Inspection
//! Object inspection
//! @{

#if defined(GG_CONFIG_ENABLE_INSPECTION)
#define GG_IF_INSPECTION_ENABLED(x) x
#else
#define GG_IF_INSPECTION_ENABLED(x)
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Hint that indicates to the inspector how a value should be displayed if the inspector
 * needs to display the value in a human-readable form.
 */
typedef enum {
    GG_INSPECTOR_FORMAT_HINT_NONE,    ///< No special formatting hint
    GG_INSPECTOR_FORMAT_HINT_HEX,     ///< The value should be displayed in hexadecimal
    GG_INSPECTOR_FORMAT_HINT_UNSIGNED ///< The value should be displayed as an unsigned integer
} GG_InspectorFormatHint;

/**
 * Options that may be passed when inspecting an object.
 */
typedef struct {
    /**
     * Level of detail requested by the inspector. The higher the level, the more details may be
     * included. There is no object-wide convention for what each level means, each object must
     * define that for itself. Most objects don't offer variable-verbosity inspection.
     */
    unsigned int verbosity;
} GG_InspectionOptions;

//----------------------------------------------------------------------
//! Interface implemented by objects that can be inspected
//!
//! @ingroup Interfaces
//----------------------------------------------------------------------
typedef struct GG_InspectableInterface GG_InspectableInterface;
typedef struct {
    const GG_InspectableInterface* vtable;
} GG_Inspectable;

//----------------------------------------------------------------------
//! Interface implemented by objects that can inspect a GG_Inspectable object.
//!
//! @interface GG_Inspector
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_Inspector) {
    void (*OnObjectStart)(GG_Inspector* self, const char* name);
    void (*OnObjectEnd)(GG_Inspector* self);
    void (*OnArrayStart)(GG_Inspector* self, const char* name);
    void (*OnArrayEnd)(GG_Inspector* self);
    void (*OnInspectable)(GG_Inspector* self, const char* name, GG_Inspectable* inspectable);
    void (*OnString)(GG_Inspector* self, const char* name, const char* value);
    void (*OnBoolean)(GG_Inspector* self, const char* name, bool value);
    void (*OnInteger)(GG_Inspector*          self,
                      const char*            name,
                      int64_t                value,
                      GG_InspectorFormatHint format_hint);
    void (*OnFloat)(GG_Inspector* self, const char* name, double value);
    void (*OnBytes)(GG_Inspector*  self,
                    const char*    name,
                    const uint8_t* data,
                    size_t         data_size);
    void (*OnExtensible)(GG_Inspector* self,
                         const char*   name,
                         uint32_t      data_type,
                         const void*   data,
                         size_t        data_size);
};

void GG_Inspector_OnObjectStart(GG_Inspector* self, const char* name);
void GG_Inspector_OnObjectEnd(GG_Inspector* self);
void GG_Inspector_OnArrayStart(GG_Inspector* self, const char* name);
void GG_Inspector_OnArrayEnd(GG_Inspector* self);
void GG_Inspector_OnInspectable(GG_Inspector* self, const char* name, GG_Inspectable* inspectable);
void GG_Inspector_OnString(GG_Inspector* self, const char* name, const char* value);
void GG_Inspector_OnBoolean(GG_Inspector* self, const char* name, bool value);
void GG_Inspector_OnInteger(GG_Inspector*          self,
                            const char*            name,
                            int64_t                value,
                            GG_InspectorFormatHint format_hint);
void GG_Inspector_OnFloat(GG_Inspector* self, const char* name, double value);
void GG_Inspector_OnBytes(GG_Inspector*  self,
                          const char*    name,
                          const uint8_t* data,
                          size_t         data_size);
void GG_Inspector_OnExtensible(GG_Inspector* self,
                               const char*   name,
                               uint32_t      data_type,
                               const void*   data,
                               size_t        data_size);

//----------------------------------------------------------------------
//! Interface implemented by objects that can be inspected
//!
//! @ingroup Interfaces
//----------------------------------------------------------------------
struct GG_InspectableInterface {
    /**
     * Inspect an object.
     * The inspected object will call back an inspector for each field that may be inspected.
     * The inspector may be called back 0 or more times, depending on the amount of detail the
     * object has. When this call returns, no futher callback will be invoked on the inspector.
     *
     * @param self The object on which this method is invoked.
     * @param inspector The inspector that should be called back during the inspection.
     * @param options. Options for the inspection. May be NULL when no specific option is needed.
     *
     * @return GG_SUCCESS if the call succeeded, or a negative error code.
     */
    GG_Result (*Inspect)(GG_Inspectable* self, GG_Inspector* inspector, const GG_InspectionOptions* options);
};

GG_Result GG_Inspectable_Inspect(GG_Inspectable* self, GG_Inspector* inspector, const GG_InspectionOptions* options);

/**
 * Class that implements the GG_Inspector interface and outputs all inspected
 * values through a logger.
 */
typedef struct GG_LoggingInspector GG_LoggingInspector;

/**
 * Create an inspector that logs all values it receives through callbacks.
 * NOTE: a limitation of the current implementation is that this object
 * cannot be destroyed while it is still indirectly referenced by the logging
 * subsystem, which means that once used, it can only be destroyed after
 * GG_LogManager_Configure has been called to reset any current logger tree.
 *
 * @param logger_name Name of the logger to use when logging.
 * @param logging_level Logging level to use when logging.
 * @param inspector Pointer to where the inspector should be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_LoggingInspector_Create(const char*           logger_name,
                                     unsigned int          logging_level,
                                     GG_LoggingInspector** inspector);

/**
 * Destroy an instance.
 *
 * @param self The object on which this method is invoked.
 */
void GG_LoggingInspector_Destroy(GG_LoggingInspector* self);

/**
 * Obtain the GG_Inspector interface for a logging inspector object.
 *
 * @param self The object on which this method is invoked.
 */
GG_Inspector* GG_LoggingInspector_AsInspector(GG_LoggingInspector* self);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
