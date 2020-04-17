/**
 * @file
 * @brief Logging
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

#include "gg_port.h"
#include "gg_types.h"
#include "gg_strings.h"
#include "gg_lists.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//! @addtogroup Logging
//! Logging subsystem
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef enum {
    GG_LOG_MESSAGE_TYPE_STRING,
    GG_LOG_MESSAGE_TYPE_OBJECT
} GG_LogMessageType;

typedef struct {
    const char*       logger_name;
    int               level;
    GG_LogMessageType message_type;
    const void*       message;
    GG_Timestamp      timestamp;
    const char*       source_file;
    unsigned int      source_line;
    const char*       source_function;
} GG_LogRecord;

/**
 * Generic top-level superclass for all loggable objects.
 * This class is supposed to be sub-classed by other modules that can define
 * concrete objects with loggable fields. Each sub-class is identified by a unique
 * 32-bit type ID, typically a 4-character code. Type IDs must be unique, but the
 * uniqueness isn't managed by this header file.
 *
 * Sub-classes must start with this type as their first field, and that field
 * must be named `base` in order for the macros to work.
 * Ex:
 * typedef struct {
 *     GG_LogObject base;
 *     int field1;
 *     int field2;
 * } MyLogObjectSubclass;
 */
typedef struct {
    uint32_t type; ///< Object type ID (usually a constant constructed with #GG_4CC)
    // subclasses add their own fields here
} GG_LogObject;

typedef struct GG_Logger GG_Logger;

struct GG_Logger {
    GG_LinkedListNode list_node;
    const char*       name;
    int               level;
    bool              level_is_inherited;
    bool              forward_to_parent;
    GG_Logger*        parent;
    GG_LinkedList     handlers;
    bool              auto_release; // `true` when heap-allocated
};

/**
 * Struct used to keep local references to loggers that can be initialized on
 * demand, and linked.
 * NOTE: the `name` field of the logger object must stay unmodified even if the
 * reference is reset, as it is used to re-initialize the reference after the reset.
 */
typedef struct {
    GG_Logger         logger;      // referenced logger object
    bool              initialized; // set to `true` if the logger object has been initialized
    GG_LinkedListNode list_node;   // used to link logger references in a list
} GG_LoggerReference;

//----------------------------------------------------------------------
//! Interface implemented by objects that handle log records.
//!
//! @interface GG_LogHandler
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_LogHandler) {
    /**
     * Handle a log record.
     *
     * @param self The object on which this method is called.
     * @param record The log record to handle.
     */
    void (*Log)(GG_LogHandler* self, const GG_LogRecord* record);

    /**
     * Destroy the object.
     *
     * @param self The object on which this method is called.
     */
    void (*Destroy)(GG_LogHandler* self);
};

//! @var GG_LogHandler::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_LogHandlerInterface
//! Virtual function table for the GG_LogHandler interface

//! @relates GG_LogHandler
//! @copydoc GG_LogHandlerInterface::Log
void GG_LogHandler_Log(GG_LogHandler* self, const GG_LogRecord* record);

//! @relates GG_LogHandler
//! @copydoc GG_LogHandlerInterface::Destroy
void GG_LogHandler_Destroy(GG_LogHandler* handler);

/**
 * Factory function for log handlers.
 *
 * @param handler_name The name of the handler type to create.
 * @param logger_name The name of the logger for which the returned handler will be attached to.
 * @param handler Pointer to a variable in which the newly created handler will be returned.
 *
 * @return #GG_SUCCESS if the handler could be created, or an error code.
 */
typedef GG_Result (*GG_LogHandlerFactory)(const char*     handler_name,
                                          const char*     logger_name,
                                          GG_LogHandler** handler);

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_LOG_LEVEL_FATAL   700
#define GG_LOG_LEVEL_SEVERE  600
#define GG_LOG_LEVEL_WARNING 500
#define GG_LOG_LEVEL_INFO    400
#define GG_LOG_LEVEL_FINE    300
#define GG_LOG_LEVEL_FINER   200
#define GG_LOG_LEVEL_FINEST  100

#define GG_LOG_LEVEL_OFF     32767
#define GG_LOG_LEVEL_ALL     0

#define GG_LOG_PLATFORM_HANDLER_NAME    "PlatformHandler"
#define GG_LOG_NULL_HANDLER_NAME        "NullHandler"
#define GG_LOG_CONSOLE_HANDLER_NAME     "ConsoleHandler"
#define GG_LOG_FILE_HANDLER_NAME        "FileHandler"

#define GG_LOG_FORMAT_FILTER_NO_SOURCE        0x01
#define GG_LOG_FORMAT_FILTER_NO_TIMESTAMP     0x02
#define GG_LOG_FORMAT_FILTER_NO_FUNCTION_NAME 0x04
#define GG_LOG_FORMAT_FILTER_NO_LEVEL_NAME    0x08
#define GG_LOG_FORMAT_FILTER_NO_LOGGER_NAME   0x10

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
#if defined(GG_CONFIG_LOGGING_ENABLE_FILENAME)
#define GG_LOG_FILE_MACRO GG_BASE_FILE_NAME
#else
#define GG_LOG_FILE_MACRO ""
#endif

#if defined(GG_CONFIG_LOGGING_MIN_LEVEL)
#define GG_LOG_STATIC_LEVEL_GUARD(_level, _statement) \
do {                                                  \
    if ((_level) >= GG_CONFIG_LOGGING_MIN_LEVEL) {    \
        _statement;                                   \
    }                                                 \
} while (0)
#else
#define GG_LOG_STATIC_LEVEL_GUARD(_level, _statement) _statement
#endif

#if defined(GG_CONFIG_ENABLE_LOGGING)

/*
 * _name must reference a constant string, as the logger won't make a copy
 * of it and will just store _name as a pointer
 */
#define GG_DEFINE_LOGGER(_logger, _name)              \
static GG_LoggerReference _logger = {                 \
    .logger = {                                       \
        .list_node = GG_LINKED_LIST_NODE_INITIALIZER, \
        .name = (_name),                              \
        .level = 0,                                   \
        .level_is_inherited = false,                  \
        .forward_to_parent = false,                   \
        .parent = NULL,                               \
        .handlers = { NULL, NULL },                   \
        .auto_release = false                         \
    },                                                \
    .initialized = false,                             \
    .list_node = GG_LINKED_LIST_NODE_INITIALIZER      \
};

#define GG_LOG_STRING(_logger, _level, _argsx)                     \
GG_LOG_STATIC_LEVEL_GUARD(_level,                                  \
do {                                                               \
    if ((_level) >= (_logger).logger.level) {                      \
        GG_LoggerReference_LogString _argsx;                       \
    }                                                              \
} while (0)                                                        \
)

#define GG_LOG_OBJECT(_logger, _level, _argsx)                     \
GG_LOG_STATIC_LEVEL_GUARD(_level,                                  \
do {                                                               \
    if ((_level) >= (_logger).logger.level) {                      \
        GG_LoggerReference_LogObject _argsx;                       \
    }                                                              \
} while (0)                                                        \
)

#define GG_CHECK_LL(_logger, _level, _result)                           \
GG_LOG_STATIC_LEVEL_GUARD(_level,                                       \
do {                                                                    \
    GG_Result _x = (_result);                                           \
    if (_x != GG_SUCCESS) {                                             \
        GG_LOG_STRING((_logger), (_level), (&(_logger), (_level),       \
        GG_LOG_FILE_MACRO, __LINE__, (GG_LocalFunctionName),            \
        "GG_CHECK failed, result=%d [%s]", _x, #_result));              \
        return _x;                                                      \
    }                                                                   \
} while (0)                                                             \
)

#define GG_CHECK_LABEL_LL(_logger, _level, _result, _label)             \
GG_LOG_STATIC_LEVEL_GUARD(_level,                                       \
do {                                                                    \
    GG_Result _x = (_result);                                           \
    if (_x != GG_SUCCESS) {                                             \
        GG_LOG_STRING((_logger), (_level), (&(_logger), (_level),       \
        GG_LOG_FILE_MACRO, __LINE__, (GG_LocalFunctionName),            \
        "GG_CHECK failed, result=%d [%s]", _x, #_result));              \
        goto _label;                                                    \
    }                                                                   \
} while (0)                                                             \
)

#define GG_LOG_OTD_LL(_logger, _level, _object_domain, _object_type, ...)                                      \
GG_LOG_STATIC_LEVEL_GUARD(_level,                                                                              \
do {                                                                                                           \
    /* because of a bug in GCC & Clang where passing a compound literal directly as a function argument        \
       doesn't reclaim the stack space used to store the struct after the function call, we need               \
       to create a named temporay variable for the struct and then pass a pointer to it. */                    \
    GG_LOG_TYPEOF_##_object_domain##_OBJECT _object =                                                          \
    GG_LOG_MAKE_##_object_domain##_OBJECT(_object_type, __VA_ARGS__);                                          \
    GG_LOG_OBJECT((_logger),                                                                                   \
                  (_level),                                                                                    \
                  (&(_logger), (_level), GG_LOG_FILE_MACRO, __LINE__, (GG_LocalFunctionName), &_object.base)); \
} while (0)                                                                                                    \
)

#define GG_LOG_LL(_logger, _level, ...) \
GG_LOG_STRING((_logger), (_level), (&(_logger), (_level), \
GG_LOG_FILE_MACRO, __LINE__, (GG_LocalFunctionName), __VA_ARGS__))

#else /* GG_CONFIG_ENABLE_LOGGING */
#define GG_DEFINE_LOGGER(_logger, _name)
#define GG_LOG_STRING(_logger, _level, _argsx)
#define GG_CHECK_LL(_logger, _level, _result) GG_CHECK(_result)
#define GG_CHECK_LABEL_LL(_logger, _level, _result, _label) GG_CHECK_LABEL((_result), _label)

#if defined(GG_CONFIG_ENABLE_MINIMAL_LOGGING)

/* The platform should implement GG_MinimalLog_LogString when this option is
 * used.
 */
void GG_MinimalLog_LogString(int level, const char *fmt, ...);

#define GG_LOG_LL(_logger, _level, ...)                 \
  GG_MinimalLog_LogString((_level), __VA_ARGS__)        \

/* The platform should implement GG_MinimalLogger_LogObject when this option is
 * used.
 */
void GG_MinimalLogger_LogObject(GG_LogObject* object);

#define GG_LOG_OTD_LL(_logger, _level, _object_domain, _object_type, ...)                                      \
do {                                                                                                           \
    /* because of a bug in GCC & Clang where passing a compound literal directly as a function argument        \
       doesn't reclaim the stack space used to store the struct after the function call, we need               \
       to create a named temporay variable for the struct and then pass a pointer to it. */                    \
    GG_LOG_TYPEOF_##_object_domain##_OBJECT _object =                                                          \
    GG_LOG_MAKE_##_object_domain##_OBJECT(_object_type, __VA_ARGS__);                                          \
    GG_MinimalLogger_LogObject(&_object.base);                                                              \
} while (0)                                                                                                    \

#endif /* GG_CONFIG_ENABLE_MINIMAL_LOGGING */

#ifndef GG_LOG_OBJECT
#define GG_LOG_OBJECT(_logger, _level, _argsx)
#endif

#ifndef GG_LOG_LL
#define GG_LOG_LL(_logger, _level, ...)
#endif

#ifndef GG_LOG_OTD_LL
#define GG_LOG_OTD_LL(_logger, _level, _object_domain, _object_type, ...)
#endif

#endif /* GG_CONFIG_ENABLE_LOGGING */

#define GG_SET_LOCAL_LOGGER(_name) GG_DEFINE_LOGGER(_GG_LocalLogger, (_name))
#define GG_CHECK_L(_level, _result) GG_CHECK_LL(_GG_LocalLogger, (_level), (_result))
#define GG_CHECK_LABEL_L(_level, _result, _label) GG_CHECK_LABEL_LL(_GG_LocalLogger, (_level), NULL, (_result), _label)

#define GG_LOG_OT_LL(_logger, _level, _object_type, ...) \
GG_LOG_OTD_LL(_logger, _level, TOPLEVEL, _object_type, __VA_ARGS__)

#define GG_LOG_O_LL(_logger, _level, _object_type, ...) \
GG_LOG_OTD_LL(_logger, _level, GG, _object_type, __VA_ARGS__)

#define GG_LOG_FATAL(...) GG_LOG_LL((_GG_LocalLogger), GG_LOG_LEVEL_FATAL, __VA_ARGS__)
#define GG_LOG_FATAL_L(_logger, ...) GG_LOG_LL((_logger), GG_LOG_LEVEL_FATAL, __VA_ARGS__)
#define GG_LOG_SEVERE(...) GG_LOG_LL((_GG_LocalLogger), GG_LOG_LEVEL_SEVERE, __VA_ARGS__)
#define GG_LOG_SEVERE_L(_logger, ...) GG_LOG_LL((_logger), GG_LOG_LEVEL_SEVERE, __VA_ARGS__)
#define GG_LOG_WARNING(...) GG_LOG_LL((_GG_LocalLogger), GG_LOG_LEVEL_WARNING, __VA_ARGS__)
#define GG_LOG_WARNING_L(_logger, ...) GG_LOG_LL((_logger), GG_LOG_LEVEL_WARNING, __VA_ARGS__)
#define GG_LOG_INFO(...) GG_LOG_LL((_GG_LocalLogger), GG_LOG_LEVEL_INFO, __VA_ARGS__)
#define GG_LOG_INFO_L(_logger, ...) GG_LOG_LL((_logger), GG_LOG_LEVEL_INFO, __VA_ARGS__)
#define GG_LOG_FINE(...) GG_LOG_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINE, __VA_ARGS__)
#define GG_LOG_FINE_L(_logger, ...) GG_LOG_LL((_logger), GG_LOG_LEVEL_FINE, __VA_ARGS__)
#define GG_LOG_FINER(...) GG_LOG_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINER, __VA_ARGS__)
#define GG_LOG_FINER_L(_logger, ...) GG_LOG_LL((_logger), GG_LOG_LEVEL_FINER, __VA_ARGS__)
#define GG_LOG_FINEST(...) GG_LOG_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINEST, __VA_ARGS__)
#define GG_LOG_FINEST_L(_logger, ...) GG_LOG_LL((_logger), GG_LOG_LEVEL_FINEST, __VA_ARGS__)

#define GG_LOG_O_FATAL(_object_type, ...) GG_LOG_O_LL((_GG_LocalLogger), GG_LOG_LEVEL_FATAL, _object_type, __VA_ARGS__)
#define GG_LOG_O_FATAL_L(_logger, _object_type, ...) GG_LOG_O_LL((_logger), GG_LOG_LEVEL_FATAL, _object_type, __VA_ARGS__)
#define GG_LOG_O_SEVERE(_object_type, ...) GG_LOG_O_LL((_GG_LocalLogger), GG_LOG_LEVEL_SEVERE, _object_type, __VA_ARGS__)
#define GG_LOG_O_SEVERE_L(_logger, _object_type, ...) GG_LOG_O_LL((_logger), GG_LOG_LEVEL_SEVERE, _object_type, __VA_ARGS__)
#define GG_LOG_O_WARNING(_object_type, ...) GG_LOG_O_LL((_GG_LocalLogger), GG_LOG_LEVEL_WARNING, _object_type, __VA_ARGS__)
#define GG_LOG_O_WARNING_L(_logger, _object_type, ...) GG_LOG_O_LL((_logger), GG_LOG_LEVEL_WARNING, _object_type, __VA_ARGS__)
#define GG_LOG_O_INFO(_object_type, ...) GG_LOG_O_LL((_GG_LocalLogger), GG_LOG_LEVEL_INFO, _object_type, __VA_ARGS__)
#define GG_LOG_O_INFO_L(_logger, _object_type, ...) GG_LOG_O_LL((_logger), GG_LOG_LEVEL_INFO, _object_type, __VA_ARGS__)
#define GG_LOG_O_FINE(_object_type, ...) GG_LOG_O_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINE, _object_type, __VA_ARGS__)
#define GG_LOG_O_FINE_L(_logger, _object_type, ...) GG_LOG_O_LL((_logger), GG_LOG_LEVEL_FINE, _object_type, __VA_ARGS__)
#define GG_LOG_O_FINER(_object_type, ...) GG_LOG_O_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINER, _object_type, __VA_ARGS__)
#define GG_LOG_O_FINER_L(_logger, _object_type, ...) GG_LOG_O_LL((_logger), GG_LOG_LEVEL_FINER, _object_type, __VA_ARGS__)
#define GG_LOG_O_FINEST(_object_type, ...) GG_LOG_O_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINEST, _object_type, __VA_ARGS__)
#define GG_LOG_O_FINEST_L(_logger, _object_type, ...) GG_LOG_O_LL((_logger), GG_LOG_LEVEL_FINEST, _object_type, __VA_ARGS__)

#define GG_LOG_OT_FATAL(_object_type, ...) GG_LOG_OT_LL((_GG_LocalLogger), GG_LOG_LEVEL_FATAL, _object_type, __VA_ARGS__)
#define GG_LOG_OT_FATAL_L(_logger, _object_type, ...) GG_LOG_OT_LL((_logger), GG_LOG_LEVEL_FATAL, _object_type, __VA_ARGS__)
#define GG_LOG_OT_SEVERE(_object_type, ...) GG_LOG_OT_LL((_GG_LocalLogger), GG_LOG_LEVEL_SEVERE, _object_type, __VA_ARGS__)
#define GG_LOG_OT_SEVERE_L(_logger, _object_type, ...) GG_LOG_OT_LL((_logger), GG_LOG_LEVEL_SEVERE, _object_type, __VA_ARGS__)
#define GG_LOG_OT_WARNING(_object_type, ...) GG_LOG_OT_LL((_GG_LocalLogger), GG_LOG_LEVEL_WARNING, _object_type, __VA_ARGS__)
#define GG_LOG_OT_WARNING_L(_logger, _object_type, ...) GG_LOG_OT_LL((_logger), GG_LOG_LEVEL_WARNING, _object_type, __VA_ARGS__)
#define GG_LOG_OT_INFO(_object_type, ...) GG_LOG_OT_LL((_GG_LocalLogger), GG_LOG_LEVEL_INFO, _object_type, __VA_ARGS__)
#define GG_LOG_OT_INFO_L(_logger, _object_type, ...) GG_LOG_OT_LL((_logger), GG_LOG_LEVEL_INFO, _object_type, __VA_ARGS__)
#define GG_LOG_OT_FINE(_object_type, ...) GG_LOG_OT_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINE, _object_type, __VA_ARGS__)
#define GG_LOG_OT_FINE_L(_logger, _object_type, ...) GG_LOG_OT_LL((_logger), GG_LOG_LEVEL_FINE, _object_type, __VA_ARGS__)
#define GG_LOG_OT_FINER(_object_type, ...) GG_LOG_OT_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINER, _object_type, __VA_ARGS__)
#define GG_LOG_OT_FINER_L(_logger, _object_type, ...) GG_LOG_OT_LL((_logger), GG_LOG_LEVEL_FINER, _object_type, __VA_ARGS__)
#define GG_LOG_OT_FINEST(_object_type, ...) GG_LOG_OT_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINEST, _object_type, __VA_ARGS__)
#define GG_LOG_OT_FINEST_L(_logger, _object_type, ...) GG_LOG_OT_LL((_logger), GG_LOG_LEVEL_FINEST, _object_type, __VA_ARGS__)

#define GG_CHECK_FATAL(_result) GG_CHECK_LL((_GG_LocalLogger), GG_LOG_LEVEL_FATAL, (_result))
#define GG_CHECK_FATAL_L(_logger, _result) GG_CHECK_LL((_logger), GG_LOG_LEVEL_FATAL, (_result))
#define GG_CHECK_SEVERE(_result) GG_CHECK_LL((_GG_LocalLogger), GG_LOG_LEVEL_SEVERE, (_result))
#define GG_CHECK_SEVERE_L(_logger, _result) GG_CHECK_LL((_logger), GG_LOG_LEVEL_SEVERE, (_result))
#define GG_CHECK_WARNING(_result) GG_CHECK_LL((_GG_LocalLogger), GG_LOG_LEVEL_WARNING, (_result))
#define GG_CHECK_WARNING_L(_logger, _result) GG_CHECK_LL((_logger), GG_LOG_LEVEL_WARNING, (_result))
#define GG_CHECK_INFO(_result) GG_CHECK_LL((_GG_LocalLogger), GG_LOG_LEVEL_INFO, (_result))
#define GG_CHECK_INFO_L(_logger, _result) GG_CHECK_LL((_logger), GG_LOG_LEVEL_INFO, (_result))
#define GG_CHECK_FINE(_result) GG_CHECK_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINE, (_result))
#define GG_CHECK_FINE_L(_logger, _result) GG_CHECK_LL((_logger), GG_LOG_LEVEL_FINE, (_result))
#define GG_CHECK_FINER(_result) GG_CHECK_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINER, (_result))
#define GG_CHECK_FINER_L(_logger, _result) GG_CHECK_LL((_logger), GG_LOG_LEVEL_FINER, (_result))
#define GG_CHECK_FINEST(_result) GG_CHECK_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINEST, (_result))
#define GG_CHECK_FINEST_L(_logger, _result) GG_CHECK_LL((_logger), GG_LOG_LEVEL_FINEST, (_result))

#define GG_CHECK_LABEL_FATAL(_result, _label) GG_CHECK_LABEL_LL((_GG_LocalLogger), GG_LOG_LEVEL_FATAL, (_result), _label)
#define GG_CHECK_LABEL_FATAL_L(_logger, _result, _label) GG_CHECK_LABEL_LL((_logger), GG_LOG_LEVEL_FATAL, (_result), _label)
#define GG_CHECK_LABEL_SEVERE(_result, _label) GG_CHECK_LABEL_LL((_GG_LocalLogger), GG_LOG_LEVEL_SEVERE, (_result), _label)
#define GG_CHECK_LABEL_SEVERE_L(_logger, _result, _label) GG_CHECK_LABEL_LL((_logger), GG_LOG_LEVEL_SEVERE, (_result), _label)
#define GG_CHECK_LABEL_WARNING(_result, _label) GG_CHECK_LABEL_LL((_GG_LocalLogger), GG_LOG_LEVEL_WARNING, (_result), _label)
#define GG_CHECK_LABEL_WARNING_L(_logger, _result, _label) GG_CHECK_LABEL_LL((_logger), GG_LOG_LEVEL_WARNING, (_result), _label)
#define GG_CHECK_LABEL_INFO(_result, _label) GG_CHECK_LABEL_LL((_GG_LocalLogger), GG_LOG_LEVEL_INFO, (_result), _label)
#define GG_CHECK_LABEL_INFO_L(_logger, _result, _label) GG_CHECK_LABEL_LL((_logger), GG_LOG_LEVEL_INFO, (_result), _label)
#define GG_CHECK_LABEL_FINE(_result, _label) GG_CHECK_LABEL_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINE, (_result), _label)
#define GG_CHECK_LABEL_FINE_L(_logger, _result, _label) GG_CHECK_LABEL_LL((_logger), GG_LOG_LEVEL_FINE, (_result), _label)
#define GG_CHECK_LABEL_FINER(_result, _label) GG_CHECK_LABEL_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINER, (_result), _label)
#define GG_CHECK_LABEL_FINER_L(_logger, _result, _label) GG_CHECK_LABEL_LL((_logger), GG_LOG_LEVEL_FINER, (_result), _label)
#define GG_CHECK_LABEL_FINEST(_result, _label) GG_CHECK_LABEL_LL((_GG_LocalLogger), GG_LOG_LEVEL_FINEST, (_result), _label)
#define GG_CHECK_LABEL_FINEST_L(_logger, _result, _label) GG_CHECK_LABEL_LL((_logger), GG_LOG_LEVEL_FINEST, (_result), _label)

/*----------------------------------------------------------------------
|   prototypes
+---------------------------------------------------------------------*/
/**
 * Explicitly initialize the log manager singleton.
 *
 * It is normally not necessary to call this function, as the log manager
 * can self-initialize upon first use. But in cases where the application
 * needs to initialize the logging subsystem at a specifc point, it may
 * use this function for that purpose.
 *
 * This function may be called from any thread.
 */
GG_Result GG_LogManager_Initialize(void);

/**
 * Explicitly terminate the log manager singleton.
 *
 * It is normally not necessary to call this function, as the log manager
 * can self-terminate upon application exit. But in cases where the application
 * needs to terminate the logging subsystem at a specifc point, it may
 * use this function for that purpose.
 *
 * This function may be called from any thread.
 */
GG_Result GG_LogManager_Terminate(void);

/*
 * Enable the logging subsystem.
 *
 * This function can be used to enable the logging subsystem after it was disabled
 * by a call to GG_LogManager_Disable. By default, the logging subsystem starts
 * as enabled, so there is no need for the application to explicitly call this
 * function at startup.
 *
 * This function may be called from any thread.
 */
GG_Result GG_LogManager_Enable(void);

/*
 * Disable the logging subsystem.
 *
 * This function can be used to temporarily disable the logging subsystem without
 * completely teminating it. After this function is called, the logging subsystem
 * can be enabled again using the GG_LogManager_Enable function.
 *
 * This function may be called from any thread.
 */
GG_Result GG_LogManager_Disable(void);

/*
 * Reconfigure the logging subsystem.
 *
 * This function destroys all the loggers and resets the log manager to a new configuration.
 * Loggers will subsequently be re-created as needed, according to the new configuration.
 *
 * This function may be called from any thread.
 *
 */
GG_Result GG_LogManager_Configure(const char* configuration_sources);

/**
 * Register a factory for creating custom log handlers.
 *
 * Platform specific implementations may want to handle log records using custom
 * handlers.
 *
 * This function may be called from any thread.
 *
 * @note Calling this method will override the root logger default handler.
 * @param factory The GG_LogHandlerFactory method that will be called to create new "PlatformHandler" handlers.
 */
void GG_LogManager_SetPlatformHandlerFactory(GG_LogHandlerFactory factory);

/**
 * Register a default factory for creating custom log handlers.
 *
 * When set, the default log handler factory is called when no other factory (built-in or platform)
 * is suitable to create a log handler by name.
 *
 * @param factory The GG_LogHandlerFactory method that will be called to create new handlers.
 */
void GG_LogManager_SetDefaultHandlerFactory(GG_LogHandlerFactory factory);

/**
 * Emit a string-based log record through a logger, via a logger reference.
 * If the logger reference's logger isn't initialized, it will be automatically
 * initialized, and the reference will be updated before returning.
 *
 * This function should not be called directly. It is intended to be used by the logging macros.
 */
void GG_LoggerReference_LogString(GG_LoggerReference* self,
                                  int                 level,
                                  const char*         source_file,
                                  unsigned int        source_line,
                                  const char*         source_function,
                                  const char*         message,
                                                      ...)

#ifdef __GNUC__
        __attribute__ ((format (printf, 6, 7)))
#endif
    ;

/**
 * Emit a object-based log record through a logger, via a logger reference.
 * If the logger reference's logger isn't initialized, it will be automatically
 * initialized, and the reference will be updated before returning.
 *
 * This function should not be called directly. It is intended to be used by the logging macros.
 */
void GG_LoggerReference_LogObject(GG_LoggerReference* self,
                                  int                 level,
                                  const char*         source_file,
                                  unsigned int        source_line,
                                  const char*         source_function,
                                  GG_LogObject*       object);

GG_Result GG_Logger_AddHandler(GG_Logger* self, GG_LogHandler* handler);

/**
 * Get the numerical logging level for a logging level name string
 *
 * @param name Logging level name
 *
 * @return A numerical logging level, or -1 if the name does not exist.
 */
int GG_Log_GetLogLevel(const char* name);

/**
 * Get the logging level name string for a numerical logging level.
 *
 * @param level Numerical logging level
 *
 * @return Logging level name, of an empty string if the numerical level does not have a name.
 */
const char* GG_Log_GetLogLevelName(int level);

/**
 * Format log record to a string based on format filter
 *
 * @param record        Log record to format
 * @param buffer        Buffer to store formatted string
 * @param buffer_size   Size of buffer provided
 * @param use_colors    Whether to use colors or not
 * @param format_filter Filter indicating log string format
 * @param add_newline   Add a newline (\r\n) at the end of the buffer
 */
void GG_Log_FormatRecordToStringBuffer(const GG_LogRecord* record,
                                       char*               buffer,
                                       size_t              buffer_size,
                                       bool                use_colors,
                                       uint32_t            format_filter,
                                       bool                add_newline);

/**
 * Get configuration properties for the console handler
 *
 * @param logger_name    Name of the logger
 * @param handler_name   Name of the console handler
 *
 * @return use_colors    Whether to use colors or not in log string output
 * @return format_filter Filter indicating log string format
 */
GG_Result GG_Log_GetConsoleHandlerConfig(const char* logger_name,
                                         const char* handler_name,
                                         bool *use_colors,
                                         uint32_t *format_filter);
//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
