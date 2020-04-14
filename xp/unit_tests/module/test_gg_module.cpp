#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_port.h"

TEST_GROUP(GG_TIMER)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

TEST(GG_TIMER, Test_ModuleInit) {
    GG_Result result = GG_Module_Initialize();
    CHECK_EQUAL(GG_SUCCESS, result);
    GG_Module_Terminate();
}

static int callback_count = 0;
static int callback_values[2];

static void callback1(void* state) {
    GG_COMPILER_UNUSED(state);
    callback_values[callback_count++] = 1;
}
static void callback2(void* state) {
    GG_COMPILER_UNUSED(state);
    callback_values[callback_count++] = 2;
}

TEST(GG_TIMER, Test_Modulecallbacks) {
    GG_Result result = GG_Module_Initialize();
    CHECK_EQUAL(GG_SUCCESS, result);

    GG_SimpleCallback c1;
    GG_GenericCallbackHandler* h1 = GG_SimpleCallback_Init(&c1, callback1, NULL);
    GG_Module_RegisterTerminationHandler(h1);

    GG_SimpleCallback c2;
    GG_GenericCallbackHandler* h2 = GG_SimpleCallback_Init(&c2, callback2, NULL);
    GG_Module_RegisterTerminationHandler(h2);

    GG_Module_Terminate();

    CHECK_EQUAL(2, callback_values[0]);
    CHECK_EQUAL(1, callback_values[1]);
}
