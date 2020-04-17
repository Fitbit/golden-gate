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
 * @date 2017-09-18
 *
 * @details
 *
 * Implementation of the logging module.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "gg_logging.h"
#include "gg_lists.h"
#include "gg_io.h"
#include "gg_memory.h"
#include "gg_system.h"
#include "gg_utils.h"
#include "gg_threads.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_LOG_HEAP_BUFFER_INCREMENT 512
#define GG_LOG_STACK_BUFFER_MAX_SIZE 128
#define GG_LOG_HEAP_BUFFER_MAX_SIZE  4096

#if !defined(GG_CONFIG_LOG_CONFIG_ENV)
#define GG_CONFIG_LOG_CONFIG_ENV "GG_LOG_CONFIG"
#endif

#if !defined(GG_CONFIG_DEFAULT_LOG_CONFIG_SOURCE)
#define GG_CONFIG_DEFAULT_LOG_CONFIG_SOURCE "file:gg-logging.properties"
#endif

#if !defined(GG_CONFIG_DEFAULT_LOG_LEVEL)
#define GG_CONFIG_DEFAULT_LOG_LEVEL GG_LOG_LEVEL_ALL
#endif
#define GG_LOG_ROOT_DEFAULT_HANDLER   "ConsoleHandler"

#if defined(GG_CONFIG_ENABLE_LOG_FILE_HANDLER)
#if !defined(GG_CONFIG_DEFAULT_LOG_FILE_HANDLER_FILENAME)
#define GG_CONFIG_DEFAULT_LOG_FILE_HANDLER_FILENAME "_gg.log"
#endif
#endif

#define GG_LOG_CONSOLE_HANDLER_DEFAULT_COLOR_MODE 1
#define GG_LOG_CONSOLE_HANDLER_DEFAULT_FILTER     (GG_LOG_FORMAT_FILTER_NO_SOURCE)
#if !defined(GG_CONFIG_LOG_MANAGER_STATIC_BUFFER_SIZE)
#if defined(GG_CONFIG_LOGGING_ENABLE_FILENAME)
#define GG_LOG_CONSOLE_HANDLER_BUFFER_SIZE        512
#else
#define GG_LOG_CONSOLE_HANDLER_BUFFER_SIZE        192
#endif
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_LinkedListNode list_node;
    GG_String         key;
    GG_String         value;
} GG_LogConfigEntry;

typedef struct {
    GG_LinkedList config;
    GG_LinkedList loggers;
    GG_LinkedList logger_references;
    GG_Logger*    root;
    bool          disabled;
    bool          initialized;
    GG_Mutex*     lock;
    GG_ThreadId   lock_owner;
    uint32_t      lock_recursion;
#if defined(GG_CONFIG_LOG_MANAGER_STATIC_BUFFER_SIZE)
    char          workspace[GG_CONFIG_LOG_MANAGER_STATIC_BUFFER_SIZE];
#endif
} GG_LogManager;

typedef struct {
    GG_IMPLEMENTS(GG_LogHandler);

    bool      use_colors;
    uint32_t  format_filter;
} GG_LogConsoleHandler;

typedef struct {
    GG_IMPLEMENTS(GG_LogHandler);

    GG_OutputStream* stream;
} GG_LogFileHandler;

typedef struct {
    GG_LinkedListNode list_node;
    GG_LogHandler*    handler;
} GG_LogHandlerEntry;

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_LogManager        LogManager;
static GG_LogHandlerFactory LogPlatformHandlerFactory;
static GG_LogHandlerFactory LogDefaultHandlerFactory;

/*----------------------------------------------------------------------
|   forward references
+---------------------------------------------------------------------*/
static GG_Logger* GG_LogManager_GetLogger(GG_Logger* logger_mem, const char* name);
static GG_Logger* GG_Logger_Create(GG_Logger* logger_ptr, const char* name);
static GG_Result GG_Logger_Destroy(GG_Logger* self);
static GG_Result GG_LogConsoleHandler_Create(const char* logger_name, GG_LogHandler** handler);
static GG_Result GG_LogNullHandler_Create(const char* logger_name, GG_LogHandler** handler);
#if defined(GG_CONFIG_ENABLE_LOG_FILE_HANDLER)
static GG_Result GG_LogFileHandler_Create(const char* logger_name, GG_LogHandler** handler);
#endif

/*----------------------------------------------------------------------
|   GG_LogHandler_Create
+---------------------------------------------------------------------*/
static GG_Result
GG_LogHandler_Create(const char*     logger_name,
                     const char*     handler_name,
                     GG_LogHandler** handler)
{
    if (GG_StringsEqual(handler_name, GG_LOG_NULL_HANDLER_NAME)) {
        return GG_LogNullHandler_Create(logger_name, handler);
    } else
#if defined(GG_CONFIG_ENABLE_LOG_FILE_HANDLER)
    if (GG_StringsEqual(handler_name, GG_LOG_FILE_HANDLER_NAME)) {
        return GG_LogFileHandler_Create(logger_name, handler);
    } else
#endif
    if (GG_StringsEqual(handler_name, GG_LOG_PLATFORM_HANDLER_NAME)) {
        if (LogPlatformHandlerFactory) {
            return LogPlatformHandlerFactory(GG_LOG_PLATFORM_HANDLER_NAME, logger_name, handler);
        } else {
            return GG_ERROR_NO_SUCH_ITEM;
        }
    } else if (GG_StringsEqual(handler_name, GG_LOG_CONSOLE_HANDLER_NAME)) {
        return GG_LogConsoleHandler_Create(logger_name, handler);
    } else if (LogDefaultHandlerFactory) {
        return LogDefaultHandlerFactory(handler_name, logger_name, handler);
    }

    return GG_ERROR_NO_SUCH_ITEM;
}

/*----------------------------------------------------------------------
|   GG_Log_GetLogLevel
+---------------------------------------------------------------------*/
int
GG_Log_GetLogLevel(const char* name)
{
    if (       GG_StringsEqual(name, "FATAL")) {
        return GG_LOG_LEVEL_FATAL;
    } else if (GG_StringsEqual(name, "SEVERE")) {
        return GG_LOG_LEVEL_SEVERE;
    } else if (GG_StringsEqual(name, "WARNING")) {
        return GG_LOG_LEVEL_WARNING;
    } else if (GG_StringsEqual(name, "INFO")) {
        return GG_LOG_LEVEL_INFO;
    } else if (GG_StringsEqual(name, "FINE")) {
        return GG_LOG_LEVEL_FINE;
    } else if (GG_StringsEqual(name, "FINER")) {
        return GG_LOG_LEVEL_FINER;
    } else if (GG_StringsEqual(name, "FINEST")) {
        return GG_LOG_LEVEL_FINEST;
    } else if (GG_StringsEqual(name, "ALL")) {
        return GG_LOG_LEVEL_ALL;
    } else if (GG_StringsEqual(name, "OFF")) {
        return GG_LOG_LEVEL_OFF;
    } else {
        return -1;
    }
}

/*----------------------------------------------------------------------
|   GG_Log_GetLogLevelName
+---------------------------------------------------------------------*/
const char*
GG_Log_GetLogLevelName(int level)
{
    switch (level) {
        case GG_LOG_LEVEL_FATAL:   return "FATAL";
        case GG_LOG_LEVEL_SEVERE:  return "SEVERE";
        case GG_LOG_LEVEL_WARNING: return "WARNING";
        case GG_LOG_LEVEL_INFO:    return "INFO";
        case GG_LOG_LEVEL_FINE:    return "FINE";
        case GG_LOG_LEVEL_FINER:   return "FINER";
        case GG_LOG_LEVEL_FINEST:  return "FINEST";
        case GG_LOG_LEVEL_OFF:     return "OFF";
        default:                   return "";
    }
}

/*----------------------------------------------------------------------
|   GG_Log_GetLogLevelAnsiColor
+---------------------------------------------------------------------*/
static const char*
GG_Log_GetLogLevelAnsiColor(int level)
{
    switch (level) {
        case GG_LOG_LEVEL_FATAL:   return "31";
        case GG_LOG_LEVEL_SEVERE:  return "31";
        case GG_LOG_LEVEL_WARNING: return "33";
        case GG_LOG_LEVEL_INFO:    return "32";
        case GG_LOG_LEVEL_FINE:    return "34";
        case GG_LOG_LEVEL_FINER:   return "35";
        case GG_LOG_LEVEL_FINEST:  return "36";
        default:                   return NULL;
    }
}

/*----------------------------------------------------------------------
|   GG_LogManager_Lock
+---------------------------------------------------------------------*/
static void
GG_LogManager_Lock(void)
{
    GG_ThreadId me = GG_GetCurrentThreadId();
    if (LogManager.lock_owner != me) {
        GG_Result result = GG_Mutex_LockAutoCreate(&LogManager.lock);
        GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
        GG_ASSERT(GG_SUCCEEDED(result));
        LogManager.lock_owner = me;
    }
    ++LogManager.lock_recursion;
}

/*----------------------------------------------------------------------
|   GG_LogManager_Lock
+---------------------------------------------------------------------*/
static void
GG_LogManager_Unlock(void)
{
    if (--LogManager.lock_recursion == 0) {
        LogManager.lock_owner = (GG_ThreadId)0;
        GG_Mutex_Unlock(LogManager.lock);
    }
}

/*----------------------------------------------------------------------
|   GG_LogManager_ConfigValueIsBooleanTrue
+---------------------------------------------------------------------*/
static bool
GG_LogManager_ConfigValueIsBooleanTrue(GG_String* value)
{
    return
        GG_String_Equals(value, "true", true) ||
        GG_String_Equals(value, "yes",  true) ||
        GG_String_Equals(value, "on",   true) ||
        GG_String_Equals(value, "1",    false);
}

/*----------------------------------------------------------------------
|   GG_LogManager_ConfigValueIsBooleanFalse
+---------------------------------------------------------------------*/
static bool
GG_LogManager_ConfigValueIsBooleanFalse(GG_String* value)
{
    return
        GG_String_Equals(value, "false", true) ||
        GG_String_Equals(value, "no",    true) ||
        GG_String_Equals(value, "off",   true) ||
        GG_String_Equals(value, "0",     true);
}

/*----------------------------------------------------------------------
|   GG_LogManager_GetConfigValue
+---------------------------------------------------------------------*/
static GG_String*
GG_LogManager_GetConfigValue(const char* prefix, const char* suffix)
{
    size_t prefix_length = prefix?strlen(prefix):0;
    size_t suffix_length = suffix?strlen(suffix):0;
    size_t key_length    = prefix_length+suffix_length;
    GG_LINKED_LIST_FOREACH(node, &LogManager.config) {
        GG_LogConfigEntry* entry = GG_LINKED_LIST_ITEM(node, GG_LogConfigEntry, list_node);
        if (GG_String_GetLength(&entry->key) == key_length &&
            (prefix == NULL || GG_String_StartsWith(&entry->key, prefix)) &&
            (suffix == NULL || GG_String_EndsWith(&entry->key, suffix  )) ) {
            return &entry->value;
        }
    }

    /* not found */
    return NULL;
}

/*----------------------------------------------------------------------
|   GG_LogManager_SetConfigValue
+---------------------------------------------------------------------*/
static GG_Result
GG_LogManager_SetConfigValue(const char* key, const char* value)
{
    GG_String* value_string = GG_LogManager_GetConfigValue(key, NULL);
    if (value_string) {
        /* the key already exists, replace the value */
        return GG_String_Assign(value_string, value);
    } else {
        /* the value does not already exist, create a new one */
        GG_LogConfigEntry* entry = GG_AllocateZeroMemory(sizeof(GG_LogConfigEntry));
        if (entry == NULL) return GG_ERROR_OUT_OF_MEMORY;
        GG_LINKED_LIST_APPEND(&LogManager.config, &entry->list_node);
        entry->key = GG_String_Create(key);
        entry->value = GG_String_Create(value);
    }

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_ClearConfig
+---------------------------------------------------------------------*/
static GG_Result
GG_LogManager_ClearConfig(void)
{
    GG_LINKED_LIST_FOREACH_SAFE(node, &LogManager.config) {
        GG_LogConfigEntry* entry = GG_LINKED_LIST_ITEM(node, GG_LogConfigEntry, list_node);
        GG_String_Destruct(&entry->key);
        GG_String_Destruct(&entry->value);
        GG_LINKED_LIST_NODE_REMOVE(node);
        GG_FreeMemory((void*)entry);
    }

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_ParseConfig
+---------------------------------------------------------------------*/
static GG_Result
GG_LogManager_ParseConfig(const char* config, size_t config_size)
{
    const char* cursor    = config;
    const char* line      = config;
    const char* separator = NULL;
    GG_String  key        = GG_EMPTY_STRING;
    GG_String  value      = GG_EMPTY_STRING;

    /* parse all entries */
    while (cursor <= config+config_size) {
        /* separators are newlines, ';' or end of buffer */
        if ( cursor == config+config_size ||
            *cursor == '\n'              ||
            *cursor == '\r'              ||
            *cursor == ';') {
            /* newline or end of buffer */
            if (separator && line[0] != '#') {
                /* we have a property */
                GG_String_AssignN(&key,   line,                    (size_t)(separator-line));
                GG_String_AssignN(&value, line+(separator+1-line), (size_t)(cursor-(separator+1)));
                GG_String_TrimWhitespace(&key);
                GG_String_TrimWhitespace(&value);

                GG_LogManager_SetConfigValue(GG_CSTR(key), GG_CSTR(value));
            }
            line = cursor+1;
            separator = NULL;
        } else if (*cursor == '=' && separator == NULL) {
            separator = cursor;
        }
        cursor++;
    }

    /* cleanup */
    GG_String_Destruct(&key);
    GG_String_Destruct(&value);

    return GG_SUCCESS;
}

#if defined(GG_CONFIG_ENABLE_LOG_CONFIG_FILE)
/*----------------------------------------------------------------------
|   GG_LogManager_ParseConfigFile
+---------------------------------------------------------------------*/
static GG_Result
GG_LogManager_ParseConfigFile(const char* filename)
{
    GG_Result result;

    /* load the file */
    GG_DataBuffer* buffer = NULL;
    result = GG_LoadFile(filename, &buffer);
    if (GG_FAILED(result)) return result;

    /* parse the config */
    result = GG_LogManager_ParseConfig((const char*)GG_DataBuffer_GetData(buffer),
                                        GG_DataBuffer_GetDataSize(buffer));

    /* destroy the buffer */
    GG_DataBuffer_Destroy(buffer);

    return result;
}
#endif // GG_CONFIG_ENABLE_LOG_CONFIG_FILE

/*----------------------------------------------------------------------
|   GG_LogManager_ParseConfigSource
+---------------------------------------------------------------------*/
static GG_Result
GG_LogManager_ParseConfigSource(GG_String* source)
{
#if defined(GG_CONFIG_ENABLE_LOG_CONFIG_FILE)
    if (GG_String_StartsWith(source, "file:")) {
        /* file source */
        GG_LogManager_ParseConfigFile(GG_CSTR(*source)+5);
    } else
#endif
    if (GG_String_StartsWith(source, "plist:")) {
        GG_LogManager_ParseConfig(GG_CSTR(*source) + 6, (size_t)(GG_String_GetLength(source) - 6));
    } else {
        return GG_ERROR_INVALID_SYNTAX;
    }

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_HaveLoggerConfig
+---------------------------------------------------------------------*/
static bool
GG_LogManager_HaveLoggerConfig(const char* name)
{
    size_t name_length = strlen(name);
    GG_LINKED_LIST_FOREACH(node, &LogManager.config) {
        GG_LogConfigEntry* entry = GG_LINKED_LIST_ITEM(node, GG_LogConfigEntry, list_node);
        if (GG_String_StartsWith(&entry->key, name)) {
            const char* suffix = GG_CSTR(entry->key)+name_length;
            if (GG_StringsEqual(suffix, ".level") ||
                GG_StringsEqual(suffix, ".handlers") ||
                GG_StringsEqual(suffix, ".forward")) {
                return true;
            }
        }
    }

    /* no config found */
    return false;
}

/*----------------------------------------------------------------------
|   GG_LogManager_ConfigureLogger
+---------------------------------------------------------------------*/
static GG_Result
GG_LogManager_ConfigureLogger(GG_Logger* logger)
{
    /* configure the level */
    {
        GG_String* level_value = GG_LogManager_GetConfigValue(logger->name, ".level");
        if (level_value) {
            int value;
            /* try a symbolic name */
            value = GG_Log_GetLogLevel(GG_CSTR(*level_value));
            if (value < 0) {
                /* try a numeric value */
                if (GG_FAILED(GG_String_ToInteger(level_value, &value))) {
                    value = -1;
                }
            }
            if (value >= 0) {
                logger->level = value;
                logger->level_is_inherited = false;
            }
        }
    }

    /* configure the handlers */
    {
        GG_String* handlers = GG_LogManager_GetConfigValue(logger->name, ".handlers");
        if (handlers) {
            const char*    handlers_list = GG_CSTR(*handlers);
            const char*    cursor = handlers_list;
            const char*    name_start = handlers_list;
            GG_String      handler_name = GG_EMPTY_STRING;
            GG_LogHandler* handler = NULL;
            for (;;) {
                if (*cursor == '\0' || *cursor == ',') {
                    if (cursor != name_start) {
                        GG_String_AssignN(&handler_name, name_start, (size_t)(cursor-name_start));
                        GG_String_TrimWhitespace(&handler_name);

                        /* create a handler */
                        if (GG_SUCCEEDED(
                            GG_LogHandler_Create(logger->name,
                                                 GG_CSTR(handler_name),
                                                 &handler))) {
                            GG_Logger_AddHandler(logger, handler);
                        }
                    }
                    if (*cursor == '\0') break;
                    name_start = cursor+1;
                }
                ++cursor;
            }
            GG_String_Destruct(&handler_name);
        }
    }

    /* configure the forwarding */
    {
        GG_String* forward = GG_LogManager_GetConfigValue(logger->name, ".forward");
        if (forward && GG_LogManager_ConfigValueIsBooleanFalse(forward)) {
            logger->forward_to_parent = false;
        }
    }

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_SetPlatformHandlerFactory
+---------------------------------------------------------------------*/
void
GG_LogManager_SetPlatformHandlerFactory(GG_LogHandlerFactory factory)
{
    LogPlatformHandlerFactory = factory;
}

/*----------------------------------------------------------------------
|   GG_LogManager_SetDefaultHandlerFactory
+---------------------------------------------------------------------*/
void
GG_LogManager_SetDefaultHandlerFactory(GG_LogHandlerFactory factory)
{
    LogDefaultHandlerFactory = factory;
}

/*----------------------------------------------------------------------
|   GG_LogManager_Reset
|
|   Must be called with the lock held
+---------------------------------------------------------------------*/
static void
GG_LogManager_Reset(void)
{
    // destroy everything we've created
    GG_LogManager_ClearConfig();
    GG_LINKED_LIST_FOREACH_SAFE(node, &LogManager.loggers) {
        GG_Logger* logger = GG_LINKED_LIST_ITEM(node, GG_Logger, list_node);
        GG_LINKED_LIST_NODE_REMOVE(node);
        GG_Logger_Destroy(logger);
    }

    // reset all the references
    GG_LINKED_LIST_FOREACH_SAFE(node, &LogManager.logger_references) {
        GG_LoggerReference* logger_reference = GG_LINKED_LIST_ITEM(node, GG_LoggerReference, list_node);
        GG_LINKED_LIST_NODE_REMOVE(node);

        // reset the reference but keep the name
        const char* name = logger_reference->logger.name;
        memset(logger_reference, 0, sizeof(*logger_reference));
        logger_reference->logger.name = name;
    }

    // no more root
    LogManager.root = NULL;

    // all lists should be empty
    GG_ASSERT(GG_LINKED_LIST_IS_EMPTY(&LogManager.config));
    GG_ASSERT(GG_LINKED_LIST_IS_EMPTY(&LogManager.loggers));
    GG_ASSERT(GG_LINKED_LIST_IS_EMPTY(&LogManager.logger_references));
}

/*----------------------------------------------------------------------
|   GG_LogManager_Terminate
+---------------------------------------------------------------------*/
GG_Result
GG_LogManager_Terminate(void)
{
    /* check if we're initialized */
    if (!LogManager.initialized) return GG_ERROR_INVALID_STATE;

    /* reset to free resources */
    GG_LogManager_Reset();

    /* destroy the lock */
    if (LogManager.lock) {
        GG_Mutex_Destroy(LogManager.lock);
        LogManager.lock = NULL;
    }

    /* we are no longer initialized */
    LogManager.initialized = false;
    LogManager.disabled = true;

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_Initialize
+---------------------------------------------------------------------*/
GG_Result
GG_LogManager_Initialize(void)
{
    /* exit now if we're already initialized */
    if (LogManager.initialized) {
        return GG_SUCCESS;
    }

    /* create and lock a mutex for the log manager */
    GG_Result result = GG_Mutex_LockAutoCreate(&LogManager.lock);
    GG_COMPILER_UNUSED(result); // needed to remove warning if GG_ASSERT is compiled out below
    GG_ASSERT(GG_SUCCEEDED(result));
    if (LogManager.initialized) {
        GG_Mutex_Unlock(LogManager.lock);
        return GG_SUCCESS;
    }

    /* init the logger list */
    GG_LINKED_LIST_INIT(&LogManager.loggers);

    /* init the logger reference list */
    GG_LINKED_LIST_INIT(&LogManager.logger_references);

    /* init the config list */
    GG_LINKED_LIST_INIT(&LogManager.config);

    /* we are now initialized */
    LogManager.initialized = true;
    LogManager.disabled    = false;
    GG_Mutex_Unlock(LogManager.lock);

    /* configure with the auto-selected config source */
    GG_LogManager_Configure(NULL);

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_Enable
+---------------------------------------------------------------------*/
GG_Result
GG_LogManager_Enable(void)
{
    LogManager.disabled = false;

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_Disable
+---------------------------------------------------------------------*/
GG_Result
GG_LogManager_Disable(void)
{
    LogManager.disabled = true;

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_Configure
+---------------------------------------------------------------------*/
GG_Result
GG_LogManager_Configure(const char* config_sources)
{
    // check that we're initialized
    if (!LogManager.initialized) return GG_ERROR_INVALID_STATE;

    GG_LogManager_Lock();

    // start from a clean slate
    GG_LogManager_Reset();

    // set some default config values
    GG_LogManager_SetConfigValue(".handlers", LogPlatformHandlerFactory ?
                                              GG_LOG_PLATFORM_HANDLER_NAME :
                                              GG_LOG_ROOT_DEFAULT_HANDLER);

    // check for auto-selected sources if no source was specified explicitly
    GG_String config_sources_system = GG_EMPTY_STRING;
    GG_String config_sources_env    = GG_EMPTY_STRING;
    if (!config_sources) {
        config_sources = GG_CONFIG_DEFAULT_LOG_CONFIG_SOURCE;

        // check system specific log configuration
        if (GG_SUCCEEDED(GG_System_GetLogConfig(&config_sources_system))) {
            config_sources = GG_CSTR(config_sources_system);
        }

        // see if the config sources have been set to non-default values
        if (GG_SUCCEEDED(GG_System_GetEnvironment(GG_CONFIG_LOG_CONFIG_ENV, &config_sources_env))) {
            config_sources = GG_CSTR(config_sources_env);
        }
    }

    // load all configs
    GG_String config_source = GG_EMPTY_STRING;
    const char* cursor = config_sources;
    const char* source = config_sources;
    for (;;) {
        if (*cursor == '\0' || *cursor == '|') {
            if (cursor != source) {
                GG_String_AssignN(&config_source, source, (size_t)(cursor-source));
                GG_String_TrimWhitespace(&config_source);
                GG_LogManager_ParseConfigSource(&config_source);
                if (*cursor == '|') source = cursor+1;
            }
            if (*cursor == '\0') break;
        }
        cursor++;
    }

    // cleanup
    GG_String_Destruct(&config_source);
    GG_String_Destruct(&config_sources_env);
    GG_String_Destruct(&config_sources_system);

    // create the root logger
    LogManager.root = GG_Logger_Create(NULL, "");
    if (LogManager.root) {
        LogManager.root->level = GG_CONFIG_DEFAULT_LOG_LEVEL;
        LogManager.root->level_is_inherited = false;
        GG_LogManager_ConfigureLogger(LogManager.root);
        GG_LINKED_LIST_APPEND(&LogManager.loggers, &LogManager.root->list_node);
    }

    GG_LogManager_Unlock();
    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_Logger_Create
+---------------------------------------------------------------------*/
static GG_Logger*
GG_Logger_Create(GG_Logger* logger_mem, const char* name)
{
    GG_Logger* logger;

    if (logger_mem) {
        /* pre-allocated logger */
        logger = logger_mem;
        logger->auto_release = false;
        logger->name = name;
    } else {
        /* create a new logger */
        size_t name_length = strlen(name);
        logger = (GG_Logger*)GG_AllocateZeroMemory(sizeof(GG_Logger) + name_length + 1);
        if (logger == NULL) {
            return NULL;
        }

        logger->auto_release = true;

        char *name_ptr = (char *)(logger + 1);
        memcpy(name_ptr, name, name_length + 1);
        logger->name = name_ptr;
    }

    /* setup the logger */
    GG_LINKED_LIST_INIT(&logger->handlers);
    logger->level              = GG_LOG_LEVEL_OFF;
    logger->level_is_inherited = true;
    logger->forward_to_parent  = true;
    logger->parent             = NULL;

    return logger;
}

/*----------------------------------------------------------------------
|   GG_Logger_Destroy
+---------------------------------------------------------------------*/
static GG_Result
GG_Logger_Destroy(GG_Logger* self)
{
    /* destroy all handlers */
    GG_LINKED_LIST_FOREACH_SAFE(node, &self->handlers) {
        GG_LogHandlerEntry* entry = GG_LINKED_LIST_ITEM(node, GG_LogHandlerEntry, list_node);
        GG_LogHandler_Destroy(entry->handler);
        GG_FreeMemory(entry);
    }

    /* free the object memory */
    if (self->auto_release) {
        GG_FreeMemory((void*)self);
    }

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_Logger_DispatchRecord
+---------------------------------------------------------------------*/
static void
GG_Logger_DispatchRecord(GG_Logger* self, GG_LogRecord* record)
{
    GG_Logger* logger = self;

    /* call all handlers for this logger and parents */
    bool was_disabled = LogManager.disabled;
    LogManager.disabled = true; // avoid recursion
    while (logger) {
        /* call all handlers for the current logger */
        GG_LINKED_LIST_FOREACH(node, &logger->handlers) {
            GG_LogHandlerEntry* entry = GG_LINKED_LIST_ITEM(node, GG_LogHandlerEntry, list_node);
            GG_LogHandler_Log(entry->handler, record);
        }

        /* forward to the parent unless this logger does not forward */
        if (logger->forward_to_parent) {
            logger = logger->parent;
        } else {
            break;
        }
    }
    LogManager.disabled = was_disabled;
}

/*----------------------------------------------------------------------
|   GG_LoggerReference_Prepare
+---------------------------------------------------------------------*/
static void
GG_LoggerReference_Prepare(GG_LoggerReference* self)
{
    // nothing to do if the reference is already initialized
    if (self->initialized) {
        return;
    }

    // get the logger by name
    GG_Logger* logger = GG_LogManager_GetLogger(&self->logger, self->logger.name);

    // remember the reference so we can clean it up later
    if (logger) {
        self->initialized = true;
        GG_LINKED_LIST_APPEND(&LogManager.logger_references, &self->list_node);
    }
}

/*----------------------------------------------------------------------
|   GG_LoggerReference_LogString
+---------------------------------------------------------------------*/
void
GG_LoggerReference_LogString(GG_LoggerReference* self,
                             int                 level,
                             const char*         source_file,
                             unsigned int        source_line,
                             const char*         source_function,
                             const char*         message,
                                                 ...)
{
    char    buffer[GG_LOG_STACK_BUFFER_MAX_SIZE];
    size_t  buffer_size = sizeof(buffer);
    char*   message_buffer = buffer;
    int     result;
    va_list args;

    /* first check if we're disabled (in case logging was terminated) */
    if (LogManager.disabled) return;

    /* check that the manager is initialized */
    if (!LogManager.initialized) {
        /* init the manager */
        GG_LogManager_Initialize();
    }

    /* lock so that we can safely check if the reference is initialized */
    GG_LogManager_Lock();

    /* ensure that the reference is initialized */
    GG_LoggerReference_Prepare(self);
    if (!self->initialized) {
        GG_LogManager_Unlock();
        return;
    }

    /* check the log level (in case filtering has not already been done) */
    if (level < self->logger.level) {
        GG_LogManager_Unlock();
        return;
    }

    for(;;) {
        va_start(args, message);
        /* try to format the message (it might not fit) */
        result = vsnprintf(message_buffer, buffer_size, message, args);
        va_end(args);
        if (result >= (int)(buffer_size)) result = -1;
        message_buffer[buffer_size - 1] = 0; /* force a NULL termination */
        if (result >= 0) break;

        /* the buffer was too small, try something bigger */
        buffer_size = (buffer_size + GG_LOG_HEAP_BUFFER_INCREMENT) * 2;
        if (buffer_size > GG_LOG_HEAP_BUFFER_MAX_SIZE) break;
        if (message_buffer != buffer) {
            GG_FreeMemory(message_buffer);
        }
        message_buffer = GG_AllocateMemory(buffer_size);
        if (message_buffer == NULL) {
            GG_LogManager_Unlock();
            return;
        }
    }

    // dispatch the record
    GG_LogRecord record = {
        .logger_name     = self->logger.name,
        .level           = level,
        .message_type    = GG_LOG_MESSAGE_TYPE_STRING,
        .message         = message_buffer,
        .source_file     = source_file,
        .source_line     = source_line,
        .source_function = source_function,
        .timestamp       = GG_System_GetCurrentTimestamp()
    };
    GG_Logger_DispatchRecord(&self->logger, &record);

    /* free anything we may have allocated */
    if (message_buffer != buffer) {
        GG_FreeMemory(message_buffer);
    }

    GG_LogManager_Unlock();
}

/*----------------------------------------------------------------------
|   GG_LoggerReference_LogObject
+---------------------------------------------------------------------*/
void
GG_LoggerReference_LogObject(GG_LoggerReference* self,
                             int                 level,
                             const char*         source_file,
                             unsigned int        source_line,
                             const char*         source_function,
                             GG_LogObject*       object)
{
    /* check that the manager is initialized */
    if (!LogManager.initialized) {
        /* init the manager */
        GG_LogManager_Initialize();
    }

    /* lock so that we can safely check if the reference is initialized */
    GG_LogManager_Lock();

    /* ensure that the reference is initialized */
    GG_LoggerReference_Prepare(self);
    if (!self->initialized) {
        GG_LogManager_Unlock();
        return;
    }

    // dispatch the record
    GG_LogRecord record = {
        .logger_name     = self->logger.name,
        .level           = level,
        .message_type    = GG_LOG_MESSAGE_TYPE_OBJECT,
        .message         = object,
        .source_file     = source_file,
        .source_line     = source_line,
        .source_function = source_function,
        .timestamp       = GG_System_GetCurrentTimestamp()
    };
    GG_Logger_DispatchRecord(&self->logger, &record);

    GG_LogManager_Unlock();
}

/*----------------------------------------------------------------------
|   GG_Logger_AddHandler
+---------------------------------------------------------------------*/
GG_Result
GG_Logger_AddHandler(GG_Logger* self, GG_LogHandler* handler)
{
    GG_LogHandlerEntry* entry;

    /* check parameters */
    if (handler == NULL) return GG_ERROR_INVALID_PARAMETERS;

    /* allocate a new entry */
    entry = (GG_LogHandlerEntry*)GG_AllocateMemory(sizeof(GG_LogHandlerEntry));
    if (entry == NULL) return GG_ERROR_OUT_OF_MEMORY;

    /* setup the entry */
    GG_LINKED_LIST_INIT(&entry->list_node);
    entry->handler = handler;

    /* attach the new entry at the beginning of the list */
    GG_LINKED_LIST_PREPEND(&self->handlers, &entry->list_node);

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_Logger_SetParent
+---------------------------------------------------------------------*/
static GG_Result
GG_Logger_SetParent(GG_Logger* self, GG_Logger* parent)
{
    GG_Logger* logger = self;

    /* set our new parent */
    self->parent = parent;

    /* find the first ancestor with its own log level */
    while (logger->level_is_inherited && logger->parent) {
        logger = logger->parent;
    }
    if (logger != self) self->level = logger->level;

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogManager_FindLogger
+---------------------------------------------------------------------*/
static GG_Logger*
GG_LogManager_FindLogger(const char* name)
{
    GG_LINKED_LIST_FOREACH(node, &LogManager.loggers) {
        GG_Logger* logger = GG_LINKED_LIST_ITEM(node, GG_Logger, list_node);
        if (GG_StringsEqual(logger->name, name)) {
            return logger;
        }
    }

    return NULL;
}

/*----------------------------------------------------------------------
|   GG_LogManager_GetLogger
|
|   This function should only be called while holding the lock
+---------------------------------------------------------------------*/
static GG_Logger*
GG_LogManager_GetLogger(GG_Logger* logger_mem, const char* name)
{
    /* just return if we're disabled (to prevent recursion) */
    if (LogManager.disabled) {
        return NULL;
    }

    /* check if this logger is already configured */
    GG_Logger* logger = GG_LogManager_FindLogger(name);
    if (logger) {
        return logger;
    }

    /* create a new logger */
    logger = GG_Logger_Create(logger_mem, name);
    if (logger == NULL) {
        return NULL;
    }

    /* configure the logger */
    GG_LogManager_ConfigureLogger(logger);

    /* find which parent to attach to */
    {
        GG_Logger* parent = LogManager.root;
        GG_String  parent_name = GG_String_Create(logger->name);
        for (;;) {
            GG_Logger* candidate_parent;

            /* find the last dot */
            int dot = GG_String_ReverseFindChar(&parent_name, '.');
            if (dot < 0) break;
            GG_String_SetLength(&parent_name, dot);

            /* see if the parent exists */
            candidate_parent = GG_LogManager_FindLogger(GG_CSTR(parent_name));
            if (candidate_parent) {
                parent = candidate_parent;
                break;
            }

            /* this parent name does not exist, see if we need to create it */
            if (GG_LogManager_HaveLoggerConfig(GG_CSTR(parent_name))) {
                parent = GG_LogManager_GetLogger(NULL, GG_CSTR(parent_name));
                break;
            }
        }
        GG_String_Destruct(&parent_name);

        /* attach to the parent */
        GG_Logger_SetParent(logger, parent);
    }

    /* add this logger to the list */
    GG_LINKED_LIST_APPEND(&LogManager.loggers, &logger->list_node);

    return logger;
}

/*--------------------------------------------------------------------*/
void
GG_LogHandler_Log(GG_LogHandler* self, const GG_LogRecord* record)
{
    GG_INTERFACE(self)->Log(self, record);
}

/*----------------------------------------------------------------------
|   GG_LogHandler_Destroy
+---------------------------------------------------------------------*/
void
GG_LogHandler_Destroy(GG_LogHandler* self)
{
    GG_INTERFACE(self)->Destroy(self);
}

/*----------------------------------------------------------------------
|   GG_LogNullHandler forward references
+---------------------------------------------------------------------*/
static const GG_LogHandlerInterface GG_LogNullHandler_Interface;

/*----------------------------------------------------------------------
|   GG_LogNullHandler_Destroy
+---------------------------------------------------------------------*/
static void
GG_LogNullHandler_Destroy(GG_LogHandler* self)
{
    GG_COMPILER_UNUSED(self);
}

/*----------------------------------------------------------------------
|   GG_LogNullHandler_Create
+---------------------------------------------------------------------*/
static GG_Result
GG_LogNullHandler_Create(const char* logger_name, GG_LogHandler** handler)
{
    GG_COMPILER_UNUSED(logger_name);

    /* setup the interface */
    static const struct {
        const GG_LogHandlerInterface* handler_interface;
    } null_handler = {
        &GG_LogNullHandler_Interface
    };

    *handler = GG_CONST_CAST(&null_handler, GG_LogHandler*);

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogNullHandler_Log
+---------------------------------------------------------------------*/
static void
GG_LogNullHandler_Log(GG_LogHandler* self, const GG_LogRecord* record)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(record);
}

/*----------------------------------------------------------------------
|   GG_LogNullHandler_Interface
+---------------------------------------------------------------------*/
static const GG_LogHandlerInterface
GG_LogNullHandler_Interface = {
    GG_LogNullHandler_Log,
    GG_LogNullHandler_Destroy
};

#if defined(GG_CONFIG_ENABLE_LOG_FILE_HANDLER)
/*----------------------------------------------------------------------
|   GG_Log_FormatRecordToStream
+---------------------------------------------------------------------*/
static void
GG_Log_FormatRecordToStream(const GG_LogRecord* record,
                            GG_OutputStream*    stream,
                            bool                use_colors,
                            uint32_t            format_filter)
{
    const char* level_name = GG_Log_GetLogLevelName(record->level);
    char        level_string[8];
    char        buffer[16];
    const char* ansi_color = NULL;

    /* format and emit the record */
    if (level_name[0] == '\0') {
        snprintf(level_string, sizeof(level_string), "%d", record->level);
        level_name = level_string;
    }

#if defined(GG_CONFIG_LOGGING_ENABLE_FILENAME)
    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_SOURCE) == 0) {
        GG_OutputStream_WriteString(stream, record->source_file);
        GG_OutputStream_WriteFully(stream, "(", 1);
        snprintf(buffer, sizeof(buffer), "%d", record->source_line);
        GG_OutputStream_WriteString(stream, buffer);
        GG_OutputStream_WriteFully(stream, "): ", 3);
    }
#endif

    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_TIMESTAMP) == 0) {
        snprintf(buffer, sizeof(buffer), "%"PRIu32".%03"PRIu32" ",
                 (uint32_t)(record->timestamp / GG_NANOSECONDS_PER_SECOND),
                 (uint32_t)((record->timestamp % GG_NANOSECONDS_PER_SECOND) /
                            GG_NANOSECONDS_PER_MILLISECOND));
        GG_OutputStream_WriteString(stream, buffer);
        GG_OutputStream_WriteFully(stream, " ", 1);
    }

    GG_OutputStream_WriteFully(stream, "[", 1);
    GG_OutputStream_WriteString(stream, record->logger_name);
    GG_OutputStream_WriteFully(stream, "] ", 2);


    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_FUNCTION_NAME) == 0) {
        GG_OutputStream_WriteFully(stream, "[",1);
        if (record->source_function) {
            GG_OutputStream_WriteString(stream, record->source_function);
        }
        GG_OutputStream_WriteFully(stream, "] ",2);
    }

    if (use_colors) {
        ansi_color = GG_Log_GetLogLevelAnsiColor(record->level);
        if (ansi_color) {
            GG_OutputStream_WriteFully(stream, "\033[", 2);
            GG_OutputStream_WriteString(stream, ansi_color);
            GG_OutputStream_WriteFully(stream, ";1m", 3);
        }
    }

    GG_OutputStream_WriteString(stream, level_name);
    if (use_colors && ansi_color) {
        GG_OutputStream_WriteFully(stream, "\033[0m", 4);
    }

    GG_OutputStream_WriteFully(stream, ": ", 2);
    GG_OutputStream_WriteString(stream, record->message);
    GG_OutputStream_WriteFully(stream, "\r\n", 2);
}
#endif

/*----------------------------------------------------------------------
|   GG_Log_FormatRecordToStringBuffer
+---------------------------------------------------------------------*/
void
GG_Log_FormatRecordToStringBuffer(const GG_LogRecord* record,
                                  char*               buffer,
                                  size_t              buffer_size,
                                  bool                use_colors,
                                  uint32_t            format_filter,
                                  bool                add_newline)
{
    const char* level_name = GG_Log_GetLogLevelName(record->level);
    char        level_string[8];
    const char* ansi_color = NULL;
    char*       buffer_start = buffer;
    bool        need_space = false;
    int         r = 0;

    // reserve some space at the end in case we need to truncate
    size_t min_size = 4 + (add_newline ? 2 : 0);
    if (buffer_size <= min_size) {
        if (buffer_size) {
            buffer[0] = '\0';
        }
        return;
    }
    buffer_size -= min_size;

    // get the level name
    if (level_name[0] == '\0') {
        snprintf(level_string, sizeof(level_string), "%d", record->level);
        level_name = level_string;
    }

#if defined(GG_CONFIG_LOGGING_ENABLE_FILENAME)
    // source file and line
    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_SOURCE) == 0) {
        r = snprintf(buffer, buffer_size, "%s(%d): ", record->source_file, record->source_line);
        if (r < 0 || r >= (int)buffer_size) goto truncate;
        buffer      += r;
        buffer_size -= r;
        need_space = false;
    }
#endif

    // timestamp
    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_TIMESTAMP) == 0) {
        r = snprintf(buffer, buffer_size, "%"PRIu32".%03"PRIu32" ",
                     (uint32_t)(record->timestamp / GG_NANOSECONDS_PER_SECOND),
                     (uint32_t)((record->timestamp % GG_NANOSECONDS_PER_SECOND) /
                                GG_NANOSECONDS_PER_MILLISECOND));
        if (r < 0 || r >= (int)buffer_size) goto truncate;
        buffer      += r;
        buffer_size -= r;
        need_space = false;
    }

    // logger name
    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_LOGGER_NAME) == 0) {
        r = snprintf(buffer, buffer_size, "[%s]", record->logger_name);
        if (r < 0 || r >= (int)buffer_size) goto truncate;
        buffer      += r;
        buffer_size -= r;
        need_space = true;
    }

    // function name
    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_FUNCTION_NAME) == 0) {
        r = snprintf(buffer, buffer_size, "[%s]", record->source_function);
        if (r < 0 || r >= (int)buffer_size) goto truncate;
        buffer      += r;
        buffer_size -= r;
        need_space = true;
    }

    // level name
    if ((format_filter & GG_LOG_FORMAT_FILTER_NO_LEVEL_NAME) == 0) {
        if (use_colors) {
            ansi_color = GG_Log_GetLogLevelAnsiColor(record->level);
            if (ansi_color) {
                r = snprintf(buffer,
                             buffer_size,
                             "%s\033[%s;1m%s\033[0m: ",
                             need_space ? " " : "",
                             ansi_color,
                             level_name);
                if (r < 0 || r >= (int)buffer_size) goto truncate;
                buffer      += r;
                buffer_size -= r;
            }
        } else {
            r = snprintf(buffer, buffer_size, "%s%s: ", need_space ? " " : "", level_name);
            if (r < 0 || r >= (int)buffer_size) goto truncate;
            buffer      += r;
            buffer_size -= r;
        }
        need_space = false;
    }

    // message
    r = 0; // just in case the switch statement doesn't cover all cases
    switch (record->message_type) {
        case GG_LOG_MESSAGE_TYPE_STRING:
            r = snprintf(buffer,
                         buffer_size,
                         "%s%s",
                         need_space ? " " : "",
                         (const char*)record->message);
            break;

        case GG_LOG_MESSAGE_TYPE_OBJECT:
            r = snprintf(buffer,
                         buffer_size,
                         "%s[object, type=%c%c%c%c]",
                         need_space ? " " : "",
                         (char)((((const GG_LogObject*)record->message)->type >> 24) & 0xFF),
                         (char)((((const GG_LogObject*)record->message)->type >> 16) & 0xFF),
                         (char)((((const GG_LogObject*)record->message)->type >> 8) & 0xFF),
                         (char)((((const GG_LogObject*)record->message)->type) & 0xFF));
            break;
    }
    if (r < 0 || r >= (int)buffer_size) goto truncate;
    buffer      += r;
    buffer_size -= r;

    // optional newline
    if (add_newline) {
        buffer[0] = '\r';
        buffer[1] = '\n';
        buffer[2] = '\0';
    }

    return;

truncate:
    // did not fit, just truncate with a proper termination
    // (we know there's enough space at the end of the buffer for a ...\r\n termination)
    snprintf(buffer_start + strlen(buffer_start), min_size, add_newline ? "...\r\n" : "...");
}

/*----------------------------------------------------------------------
|   GG_Log_GetConsoleHandlerConfig
+---------------------------------------------------------------------*/
GG_Result
GG_Log_GetConsoleHandlerConfig(const char* logger_name,
                               const char* handler_name,
                               bool *use_colors,
                               uint32_t *format_filter)
{
    /* compute a prefix for the configuration of this handler */
    GG_String logger_prefix = GG_String_Create(logger_name);
    if (GG_String_IsEmpty(&logger_prefix) && strlen(logger_name) != 0) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    GG_String_Append(&logger_prefix, handler_name);

    /* Get the configuration */
    {
        GG_String* colors;
        *use_colors = GG_LOG_CONSOLE_HANDLER_DEFAULT_COLOR_MODE;
        colors = GG_LogManager_GetConfigValue(GG_CSTR(logger_prefix), ".colors");
        if (colors) {
            if (GG_LogManager_ConfigValueIsBooleanTrue(colors)) {
                *use_colors = true;
            } else if (GG_LogManager_ConfigValueIsBooleanFalse(colors)) {
                *use_colors = false;
            }
        }
    }
    {
        GG_String* filter;
        *format_filter = GG_LOG_CONSOLE_HANDLER_DEFAULT_FILTER;
        filter = GG_LogManager_GetConfigValue(GG_CSTR(logger_prefix), ".filter");
        if (filter) {
            int flags;
            GG_String_ToInteger(filter, &flags);
            *format_filter = flags;
        }
    }

    /* cleanup */
    GG_String_Destruct(&logger_prefix);

    return GG_SUCCESS;
}

/*----------------------------------------------------------------------
|   GG_LogConsoleHandler_Log
+---------------------------------------------------------------------*/
static void
GG_LogConsoleHandler_Log(GG_LogHandler* _self, const GG_LogRecord* record)
{
    GG_LogConsoleHandler* self = GG_SELF(GG_LogConsoleHandler, GG_LogHandler);

#if defined(GG_CONFIG_LOG_MANAGER_STATIC_BUFFER_SIZE)
    char*  string_buffer      = &LogManager.workspace;
    size_t string_buffer_size = sizeof(LogManager.workspace);
#else
    char   string_buffer[GG_LOG_CONSOLE_HANDLER_BUFFER_SIZE];
    size_t string_buffer_size = sizeof(string_buffer);
#endif

    GG_Log_FormatRecordToStringBuffer(record,
                                      string_buffer,
                                      string_buffer_size,
                                      self->use_colors,
                                      self->format_filter,
                                      true);
    GG_System_ConsoleOutput(string_buffer);
}

/*----------------------------------------------------------------------
|   GG_LogConsoleHandler_Destroy
+---------------------------------------------------------------------*/
static void
GG_LogConsoleHandler_Destroy(GG_LogHandler* _self)
{
    GG_LogConsoleHandler* self = GG_SELF(GG_LogConsoleHandler, GG_LogHandler);

    /* free the object memory */
    GG_ClearAndFreeObject(self, 1);
}

/*----------------------------------------------------------------------
|   GG_LogConsoleHandler interface
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(GG_LogConsoleHandler, GG_LogHandler) {
    GG_LogConsoleHandler_Log,
    GG_LogConsoleHandler_Destroy
};

/*----------------------------------------------------------------------
|   GG_LogConsoleHandler_Create
+---------------------------------------------------------------------*/
static GG_Result
GG_LogConsoleHandler_Create(const char*     logger_name,
                            GG_LogHandler** handler)
{
    GG_Result result = GG_SUCCESS;

    /* allocate a new object */
    GG_LogConsoleHandler* self = GG_AllocateZeroMemory(sizeof(GG_LogConsoleHandler));
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    result = GG_Log_GetConsoleHandlerConfig(logger_name,
                                            ".ConsoleHandler",
                                            &self->use_colors,
                                            &self->format_filter);
    if (GG_FAILED(result)) {
        GG_FreeMemory(self);
        return result;
    }

    /* setup the interface */
    GG_SET_INTERFACE(self, GG_LogConsoleHandler, GG_LogHandler);

    /* return the new object */
    *handler = GG_CAST(self, GG_LogHandler);

    return result;
}

#if defined(GG_CONFIG_ENABLE_LOG_FILE_HANDLER)

/*----------------------------------------------------------------------
|   GG_LogFileHandler_Log
+---------------------------------------------------------------------*/
static void
GG_LogFileHandler_Log(GG_LogHandler* _self, const GG_LogRecord* record)
{
    GG_LogFileHandler* self = GG_SELF(GG_LogFileHandler, GG_LogHandler);
    GG_Log_FormatRecordToStream(record, self->stream, false, 0);
}

/*----------------------------------------------------------------------
|   GG_LogFileHandler_Destroy
+---------------------------------------------------------------------*/
static void
GG_LogFileHandler_Destroy(GG_LogHandler* _self)
{
    GG_LogFileHandler* self = GG_SELF(GG_LogFileHandler, GG_LogHandler);

    /* release the stream */
    GG_RELEASE_OBJECT(self->stream);

    /* free the object memory */
    GG_FreeMemory((void*)self);
}

/*----------------------------------------------------------------------
|   GG_LogFileHandler_Create
+---------------------------------------------------------------------*/
static GG_Result
GG_LogFileHandler_Create(const char*     logger_name,
                         GG_LogHandler** handler)
{
    GG_LogFileHandler* instance;
    const char*        filename;
    GG_String          filename_synth = GG_EMPTY_STRING;
    GG_File*           file;
    bool               append = true;
    GG_Result          result = GG_SUCCESS;

    /* allocate a new object */
    instance = GG_AllocateZeroMemory(sizeof(GG_LogFileHandler));
    if (instance == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    /* compute a prefix for the configuration of this handler */
    GG_String logger_prefix = GG_String_Create(logger_name);
    if (GG_String_IsEmpty(&logger_prefix) && strlen(logger_name) != 0) {
        GG_FreeMemory(instance);
        return GG_ERROR_OUT_OF_MEMORY;
    }

    GG_CHECK(GG_String_Append(&logger_prefix, ".FileHandler"));

    /* configure the object */
    {
        /* filename */
        GG_String* filename_conf = GG_LogManager_GetConfigValue(GG_CSTR(logger_prefix), ".filename");
        if (filename_conf) {
            filename = GG_CSTR(*filename_conf);
        } else if (logger_name[0]) {
            GG_String_Reserve(&filename_synth, strlen(logger_name));
            GG_String_Assign(&filename_synth, logger_name);
            GG_String_Append(&filename_synth, ".log");
            filename = GG_CSTR(filename_synth);
        } else {
            /* default name for the root logger */
            filename = GG_CONFIG_DEFAULT_LOG_FILE_HANDLER_FILENAME;
        }
    }
    {
        /* append mode */
        GG_String* append_mode = GG_LogManager_GetConfigValue(GG_CSTR(logger_prefix),".append");
        if (append_mode && !GG_LogManager_ConfigValueIsBooleanFalse(append_mode)) {
            append = false;
        }

    }

    /* open the log file */
    if (GG_SUCCEEDED(GG_File_Create(filename, &file))) {
        result = GG_File_Open(file,
                               GG_FILE_OPEN_MODE_CREATE |
                               GG_FILE_OPEN_MODE_WRITE  |
                               (append?GG_FILE_OPEN_MODE_APPEND:0));
        if (GG_SUCCEEDED(result)) {
            result = GG_File_GetOutputStream(file, &instance->stream);
            if (GG_FAILED(result)) {
                instance->stream = NULL;
            }
        }

        GG_DESTROY_OBJECT(file);
    }

    /* setup the interface */
    handler->instance = (GG_LogHandlerInstance*)instance;
    handler->iface    = &GG_LogFileHandler_Interface;

    /* cleanup */
    GG_String_Destruct(&logger_prefix);
    GG_String_Destruct(&filename_synth);

    return result;
}

#endif // GG_CONFIG_ENABLE_LOG_FILE_HANDLER
