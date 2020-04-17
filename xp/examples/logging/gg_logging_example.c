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
 * Usage examples for the interfaces macros.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "xp/common/gg_common.h"
#include "xp/annotations/gg_annotations.h"
#include "gg_logging_example.h"

// set a local logger
GG_SET_LOCAL_LOGGER("foo.bar.x")

// define an explicit logger
GG_DEFINE_LOGGER(FooLogger, "gg.test.foo")

// example handler
//---------------------------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_LogHandler);
} ExampleHandler;

static void
ExampleHandlerLog(GG_LogHandler* self, const GG_LogRecord* record)
{
    GG_COMPILER_UNUSED(self);
    printf("LOG: logger=%s, level=%d, function=%s, line=%d\n",
           record->logger_name,
           record->level,
           record->source_function,
           record->source_line);
}

static void
ExampleHandlerDestroy(GG_LogHandler* self)
{
    GG_FreeMemory(self);
}

GG_IMPLEMENT_INTERFACE(ExampleHandler, GG_LogHandler) {
    .Log     = ExampleHandlerLog,
    .Destroy = ExampleHandlerDestroy
};

//---------------------------------------------------------------------------------------
// example handler factory
//---------------------------------------------------------------------------------------
static GG_Result
ExampleHandlerFactory(const char*     handler_name,
                      const char*     logger_name,
                      GG_LogHandler** handler) {
    GG_COMPILER_UNUSED(logger_name);
    GG_COMPILER_UNUSED(handler_name);
    ExampleHandler* example_handler = (ExampleHandler*)GG_AllocateZeroMemory(sizeof(ExampleHandler));
    GG_SET_INTERFACE(example_handler, ExampleHandler, GG_LogHandler);
    *handler = GG_CAST(example_handler, GG_LogHandler);
    return GG_SUCCESS;
}

static void
LogSomeStuff(void)
{
    GG_LOG_FATAL("this log record should be seen at FATAL level");
    GG_LOG_SEVERE("this log record should be seen at SEVERE level");
    GG_LOG_WARNING("this log record should be seen at WARNING level");
    GG_LOG_INFO("this log record should be seen at INFO level");
    GG_LOG_FINE("this log record should be seen at FINE level");
    GG_LOG_FINER("this log record should be seen at FINER level");
    GG_LOG_FINEST("this log record should be seen at FINEST level");

    example_func1();
    example_func2();

    GG_LOG_INFO_L(FooLogger, "Hello from FooLogger - level info");
    GG_LOG_FINE_L(FooLogger, "Bye from FooLogger - level fine");

    GG_LOG_FATAL("this very long log record might get truncated 0 -"
                 "this very long log record might get truncated 1 -"
                 "this very long log record might get truncated 2 -"
                 "this very long log record might get truncated 3 -"
                 "this very long log record might get truncated 4 -"
                 "this very long log record might get truncated 5 -"
                 "this very long log record might get truncated 6 -"
                 "this very long log record might get truncated 7 -"
                 "this very long log record might get truncated 8 -"
                 "this very long log record might get truncated 9 -");
}

//---------------------------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("Hello Golden Gate\n");

    printf("------------ step 1 --------------\n");
    LogSomeStuff();

    // set a platform handler factory
    GG_LogManager_SetPlatformHandlerFactory(ExampleHandlerFactory);

    printf("------------ step 2 --------------\n");
    LogSomeStuff();

    // reconfigure the loggers
    GG_LogManager_Configure("plist:foo.bar.x.level=ALL;.level=INFO;foo.bar.Handlers=PlatformHandler,ConsoleHandler");

    printf("------------ step 3 --------------\n");
    LogSomeStuff();

    // force termination of the log manager (safe to do, since we know it has been auto-initialized before)
    GG_LogManager_Terminate();

    printf("------------ step 4 --------------\n");
    LogSomeStuff();

    // go back to the default handler
    GG_LogManager_SetPlatformHandlerFactory(NULL);

    GG_LogManager_Terminate();
    GG_LogManager_Initialize();

    printf("------------ step 5 --------------\n");
    LogSomeStuff();

    GG_LogManager_Disable();
    printf("------------ step 6 --------------\n");
    LogSomeStuff();
    GG_LogManager_Enable();
    printf("------------ step 7 --------------\n");
    LogSomeStuff();

    // enable all log levels
    printf("------------ step 8 --------------\n");
    GG_LogManager_Configure("plist:.level=ALL");
    LogSomeStuff();

    // enable only INFO level
    printf("------------ step 9 --------------\n");
    GG_LogManager_Configure("plist:.level=INFO");
    LogSomeStuff();

    // console handler with filters
    printf("------------ step 10 --------------\n");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=0");
    GG_LOG_INFO("some log message, filter=0: EVERYTHING");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=1");
    GG_LOG_INFO("some log message, filter=1: NO SOURCE");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=2");
    GG_LOG_INFO("some log message, filter=2: NO TIMESTAMP");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=4");
    GG_LOG_INFO("some log message, filter=2: NO FUNCTION NAME");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=8");
    GG_LOG_INFO("some log message, filter=2: NO LEVEL NAME");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=15");
    GG_LOG_INFO("some log message, filter=15: NO LOGGER NAME");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=29");
    GG_LOG_INFO("some log message, filter=29: JUST TIMESTAMP");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=23");
    GG_LOG_INFO("some log message, filter=29: JUST LEVEL NAME");
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler;.ConsoleHandler.filter=31");
    GG_LOG_INFO("some log message, filter=15: NOTHING");

    // object logging
    printf("------------ step 11 --------------\n");
    typedef struct {
        int foobar;
    } my_custom_log_object;
    typedef struct {
        GG_LogObject         base;
        my_custom_log_object inner;
    } MyGGObject;

    #if defined(GG_LOG_TYPEOF_GG_OBJECT)
    #undef GG_LOG_TYPEOF_GG_OBJECT
    #endif
    #if defined(GG_LOG_MAKE_GG_OBJECT)
    #undef GG_LOG_MAKE_GG_OBJECT
    #endif

    #define GG_LOG_TYPEOF_GG_OBJECT MyGGObject
    #define GG_LOG_MAKE_GG_OBJECT(_object_type, ...)             \
    ((_object_type) {                                    \
        .base = {                                                \
            .type = GG_4CC('f', 'o', 'o', 'b')                   \
        },                                                       \
        .inner = __VA_ARGS__                                     \
    })
    GG_LogManager_Configure("plist:.level=INFO;.handlers=ConsoleHandler");

    // log an object to the local logger
    GG_LOG_O_INFO(MyGGObject, {
        .foobar = 3
    });

    // log an object to a specific logger
    GG_LOG_O_WARNING_L(FooLogger, MyGGObject, {
        .foobar = 4
    });

    // custom domain
    typedef struct {
        GG_LogObject         base;
        my_custom_log_object inner;
    } MyDomainObject;
    #define GG_LOG_TYPEOF_FOO_OBJECT MyDomainObject
    #define GG_LOG_MAKE_FOO_OBJECT(_object_type, ...) \
    ((_object_type) {                                    \
        .base = {                                                \
            .type = GG_4CC('f', 'o', 'o', '2')                   \
        },                                                       \
        .inner = __VA_ARGS__                                     \
    })
    GG_LOG_OTD_LL(FooLogger, GG_LOG_LEVEL_SEVERE, FOO, MyDomainObject, {
        .foobar = 5
    });

#if 0

    // log with a computed array/string
#if defined(GG_CONFIG_ENABLE_LOGGING)
    GG_Bar object = (GG_Bar) {
        .version = 4,
        .state   = GG_State_STATE_PQ
    };
    memset(object.info, 1, sizeof(object.info));
    GG_LOG_O_FINE(bar, object);
#endif

    GG_LOG_O_FINE(bar, {
        .info = { 1, 2, 3}
    });
#endif

#if defined(GG_CONFIG_ENABLE_ANNOTATIONS)
    // log a generic (non GG) object
    GG_LOG_OT_FINEST(malloc_fail, { .fail_count = 123 });
#endif

    GG_LogManager_Terminate();

    return 0;
}
