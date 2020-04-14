/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include <cstring>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/module/gg_module.h"
#include "xp/common/gg_lists.h"
#include "xp/common/gg_port.h"
#include "xp/module/gg_module.h"
#include "xp/sockets/gg_sockets.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_SOCKETS)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

//----------------------------------------------------------------------
TEST(GG_SOCKETS, Test_SocketsUtils) {
    GG_IpAddress addr;

    GG_Result result = GG_IpAddress_SetFromString(&addr, "");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, ".");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, ".1");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "a,b,c,d");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2.3");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2.3.1234");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2.3.4.5");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "#");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2.3.4.");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2.3.4#");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2.345.6");
    LONGS_EQUAL(GG_ERROR_INVALID_SYNTAX, result);

    result = GG_IpAddress_SetFromString(&addr, "1.2.3.4");
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(addr.ipv4[0], 1);
    LONGS_EQUAL(addr.ipv4[1], 2);
    LONGS_EQUAL(addr.ipv4[2], 3);
    LONGS_EQUAL(addr.ipv4[3], 4);

    result = GG_IpAddress_SetFromString(&addr, "169.254.35.7");
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(addr.ipv4[0], 169);
    LONGS_EQUAL(addr.ipv4[1], 254);
    LONGS_EQUAL(addr.ipv4[2], 35);
    LONGS_EQUAL(addr.ipv4[3], 7);
}
