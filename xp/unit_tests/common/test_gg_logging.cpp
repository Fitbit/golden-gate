/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include <stdlib.h>
#include <string.h>

#include "xp/common/gg_common.h"

//----------------------------------------------------------------------
GG_SET_LOCAL_LOGGER("test.foo.bar")
GG_DEFINE_LOGGER(FooLogger, "test.foo")
GG_DEFINE_LOGGER(FooBazLogger, "test.foo.baz")

//---------------------------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_LogHandler);

    struct {
        char*             logger_name;
        int               level;
        GG_LogMessageType message_type;
        void*             message;
        GG_Timestamp      timestamp;
        char*             source_file;
        unsigned int      source_line;
        char*             source_function;
    } last_record;
} TestLogHandler;

static void
TestLogHandler_Reset(TestLogHandler* self)
{
    if (self->last_record.logger_name) {
        free(self->last_record.logger_name);
    }
    if (self->last_record.message) {
        free(self->last_record.message);
    }
    if (self->last_record.source_file) {
        free(self->last_record.source_file);
    }
    if (self->last_record.source_function) {
        free(self->last_record.source_function);
    }
    memset(&self->last_record, 0, sizeof(self->last_record));
}

static void
TestLogHandler_Log(GG_LogHandler* _self, const GG_LogRecord* record)
{
    TestLogHandler* self = GG_SELF(TestLogHandler, GG_LogHandler);
    TestLogHandler_Reset(self);
    self->last_record.logger_name     = strdup(record->logger_name);
    self->last_record.level           = record->level;
    self->last_record.message_type    = record->message_type;
    if (record->message_type == GG_LOG_MESSAGE_TYPE_STRING) {
        self->last_record.message = strdup((const char *)record->message);
    }
    self->last_record.timestamp       = record->timestamp;
    self->last_record.source_file     = strdup(record->source_file);
    self->last_record.source_line     = record->source_line;
    self->last_record.source_function = strdup(record->source_function);
}

static void
TestLogHandler_Destroy(GG_LogHandler* self)
{
    GG_COMPILER_UNUSED(self);
    // do nothing, as we're only using a static handler
}

GG_IMPLEMENT_INTERFACE(TestLogHandler, GG_LogHandler) {
    .Log     = TestLogHandler_Log,
    .Destroy = TestLogHandler_Destroy
};

static TestLogHandler test_handler_1;
static TestLogHandler test_handler_2;

//---------------------------------------------------------------------------------------
static GG_Result
TestLogHandlerFactory(const char*     handler_name,
                      const char*     logger_name,
                      GG_LogHandler** handler) {
    GG_COMPILER_UNUSED(logger_name);

    if (!strcmp(handler_name, "TestHandler")) {
        GG_SET_INTERFACE(&test_handler_1, TestLogHandler, GG_LogHandler);
        *handler = GG_CAST(&test_handler_1, GG_LogHandler);
        return GG_SUCCESS;
    } else if (!strcmp(handler_name, "PlatformHandler")) {
        GG_SET_INTERFACE(&test_handler_2, TestLogHandler, GG_LogHandler);
        *handler = GG_CAST(&test_handler_2, GG_LogHandler);
        return GG_SUCCESS;
    } else {
        return GG_ERROR_NO_SUCH_ITEM;
    }
}

//----------------------------------------------------------------------
static void log_from_some_function(GG_LoggerReference* logger, int level, const char* message)
{
    GG_LOG_LL(*logger, level, "%s", message);
}

//----------------------------------------------------------------------
TEST_GROUP(GG_LOGGING)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

//----------------------------------------------------------------------
TEST(GG_LOGGING, Test_BasicLogging) {
    GG_LogManager_Initialize();

    GG_LogManager_SetPlatformHandlerFactory(NULL);
    GG_LogManager_SetDefaultHandlerFactory(NULL);

    // so a simple log with the console handler, just to exercise some code coverage
    GG_LogManager_Configure("plist:.level=ALL;.handlers=ConsoleHandler");
    GG_LOG_INFO("foobar");
    GG_LogManager_Configure("plist:.level=ALL;"
                            ".handlers=ConsoleHandler;"
                            ".ConsoleHandler.colors=true;"
                            ".ConsoleHandler.filter=1");
    GG_LOG_INFO("foobar");

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;.handlers=PlatformHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(0, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;.handlers=TestHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(0, test_handler_2.last_record.level);

    GG_LogManager_SetPlatformHandlerFactory(TestLogHandlerFactory);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;.handlers=PlatformHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_INFO, test_handler_2.last_record.level);

    GG_LogManager_SetDefaultHandlerFactory(TestLogHandlerFactory);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;.handlers=TestHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_INFO, test_handler_1.last_record.level);
    STRCMP_EQUAL("hello", (const char*)test_handler_1.last_record.message);
    STRCMP_EQUAL("test.foo.bar", (const char*)test_handler_1.last_record.logger_name);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    log_from_some_function(&FooBazLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_1.last_record.level);
    STRCMP_EQUAL("hello", (const char*)test_handler_1.last_record.message);
    STRCMP_EQUAL("test.foo.baz", (const char*)test_handler_1.last_record.logger_name);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=INFO;.handlers=TestHandler");
    log_from_some_function(&FooBazLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    POINTERS_EQUAL(NULL, test_handler_1.last_record.message);
    POINTERS_EQUAL(NULL, test_handler_1.last_record.logger_name);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:test.foo.bar.level=INFO;test.foo.baz.level=FINE;.handlers=TestHandler");
    log_from_some_function(&FooBazLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_1.last_record.level);
    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;.handlers=NullHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(0, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;.handlers=PlatformHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_INFO, test_handler_2.last_record.level);
    STRCMP_EQUAL("hello", (const char*)test_handler_2.last_record.message);
    STRCMP_EQUAL("test.foo.bar", (const char*)test_handler_2.last_record.logger_name);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;.handlers=BogusHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(0, test_handler_2.last_record.level);

    GG_LogManager_Initialize();
}

//----------------------------------------------------------------------
TEST(GG_LOGGING, Test_ConfigParsing) {
    GG_LogManager_Initialize();

    GG_LogManager_SetDefaultHandlerFactory(TestLogHandlerFactory);
    GG_LogManager_SetPlatformHandlerFactory(TestLogHandlerFactory);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=OFF");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=FINEST");
    log_from_some_function(&_GG_LocalLogger, 0, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=FINER");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINEST, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=FINE");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINER, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=INFO");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=WARNING");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_INFO, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=SEVERE");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_WARNING, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=FATAL");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_SEVERE, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=100");
    log_from_some_function(&_GG_LocalLogger, 99, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=BOGUS");
    log_from_some_function(&_GG_LocalLogger, 1, "hello");
    LONGS_EQUAL(1, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=FINE;.handlers=TestHandler,PlatformHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=FINE;"
                            ".handlers=TestHandler;"
                            "test.foo.bar.level=FINE;"
                            "test.foo.bar.handlers=PlatformHandler");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=FINE;"
                            ".handlers=TestHandler;"
                            "test.foo.bar.level=FINE;"
                            "test.foo.bar.handlers=PlatformHandler;"
                            "test.foo.bar.forward=false");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;"
                            ".handlers=TestHandler;"
                            "test.level=FINE;"
                            "test.handlers=PlatformHandler;"
                            "test.forward=no");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;"
                            ".handlers=TestHandler;"
                            "test.level=FINE;"
                            "test.handlers=PlatformHandler;"
                            "test.forward=off");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;"
                            ".handlers=TestHandler;"
                            "test.level=FINE;"
                            "test.handlers=PlatformHandler;"
                            "test.forward=0");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FINE, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_2.last_record.level);

    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL|plist:test.level=OFF");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_SEVERE, "hello");
    LONGS_EQUAL(0, test_handler_2.last_record.level);

    GG_LogManager_Terminate();
}

//----------------------------------------------------------------------
TEST(GG_LOGGING, Test_LoggingUtils) {
    const char* name;

    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_OFF);
    STRCMP_EQUAL("OFF", name);
    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_FINEST);
    STRCMP_EQUAL("FINEST", name);
    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_FINER);
    STRCMP_EQUAL("FINER", name);
    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_FINE);
    STRCMP_EQUAL("FINE", name);
    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_INFO);
    STRCMP_EQUAL("INFO", name);
    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_WARNING);
    STRCMP_EQUAL("WARNING", name);
    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_SEVERE);
    STRCMP_EQUAL("SEVERE", name);
    name = GG_Log_GetLogLevelName(GG_LOG_LEVEL_FATAL);
    STRCMP_EQUAL("FATAL", name);
    name = GG_Log_GetLogLevelName(1234567);
    STRCMP_EQUAL("", name);
}

//----------------------------------------------------------------------
TEST(GG_LOGGING, Test_EnableDisable) {
    GG_LogManager_Initialize();

    GG_LogManager_SetDefaultHandlerFactory(TestLogHandlerFactory);
    GG_LogManager_SetPlatformHandlerFactory(TestLogHandlerFactory);

    GG_LogManager_Disable();

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(0, test_handler_2.last_record.level);

    GG_LogManager_Enable();

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL");
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, "hello");
    LONGS_EQUAL(0, test_handler_1.last_record.level);
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);

    GG_LogManager_Terminate();
}

//----------------------------------------------------------------------
TEST(GG_LOGGING, Test_Truncation) {
    GG_LogManager_Initialize();

    GG_LogManager_SetDefaultHandlerFactory(TestLogHandlerFactory);
    GG_LogManager_SetPlatformHandlerFactory(TestLogHandlerFactory);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL");
    const char* message =
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff"
        "00112233445566778899aabbccddeeff";
    log_from_some_function(&_GG_LocalLogger, GG_LOG_LEVEL_FATAL, message);
    LONGS_EQUAL(GG_LOG_LEVEL_FATAL, test_handler_2.last_record.level);
    size_t output_length = strlen((const char *)test_handler_2.last_record.message);
    CHECK_TRUE(output_length == strlen(message));

    GG_LogManager_Terminate();
}

//----------------------------------------------------------------------
TEST(GG_LOGGING, Test_Tree) {
    GG_LogManager_Initialize();

    GG_LogManager_SetDefaultHandlerFactory(TestLogHandlerFactory);
    GG_LogManager_SetPlatformHandlerFactory(TestLogHandlerFactory);

    TestLogHandler_Reset(&test_handler_1);
    TestLogHandler_Reset(&test_handler_2);
    GG_LogManager_Configure("plist:.level=ALL;test.level=INFO;test.foo.baz.level=FINE");
    log_from_some_function(&FooLogger, GG_LOG_LEVEL_FINE, "hello foo");
    LONGS_EQUAL(0, test_handler_2.last_record.level);
    log_from_some_function(&FooBazLogger, GG_LOG_LEVEL_FINE, "hello baz");
    LONGS_EQUAL(GG_LOG_LEVEL_FINE, test_handler_2.last_record.level);

    GG_LogManager_Terminate();
}
