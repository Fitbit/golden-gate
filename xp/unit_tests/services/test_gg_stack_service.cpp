#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_io.h"
#include "xp/common/gg_port.h"
#include "xp/common/gg_buffer.h"
#include "xp/services/stack/gg_stack_service.h"

/*----------------------------------------------------------------------
|   tests
+---------------------------------------------------------------------*/
TEST_GROUP(GG_SERVICES) {
    void setup(void) {
    }

    void teardown(void) {
    }
};

TEST(GG_SERVICES, Test_StackServiceBasics) {
    GG_StackService* service = NULL;
    GG_Result result = GG_StackService_Create(&service);
    CHECK_TRUE(result == GG_SUCCESS);
    CHECK_TRUE(service != NULL);

    // Default type should be dtls + coap
    const char* stack_type = GG_StackService_GetStackType(service);
    CHECK_TRUE(!strcmp(stack_type, "dtls"));
    const char* service_type = GG_StackService_GetServiceType(service);
    CHECK_TRUE(!strcmp(service_type, "coap"));

    // Test with invalid input
    result = GG_StackService_SetType(service, "type", "service");
    CHECK_TRUE(GG_FAILED(result));

    // CoAP over Gattlink shouldn't be allowed
    result = GG_StackService_SetType(service, "gattlink", "coap");
    CHECK_TRUE(GG_FAILED(result));

    // Set stack and service type
    result = GG_StackService_SetType(service, "udp", "blast");
    CHECK_TRUE(result == GG_SUCCESS);

    // Check stack and service type
    stack_type = GG_StackService_GetStackType(service);
    CHECK_TRUE(!strcmp(stack_type, "udp"));
    service_type = GG_StackService_GetServiceType(service);
    CHECK_TRUE(!strcmp(service_type, "blast"));

    // Default service for Gattlink should be blast
    result = GG_StackService_SetType(service, "gattlink", NULL);
    CHECK_TRUE(result == GG_SUCCESS);
    stack_type = GG_StackService_GetStackType(service);
    CHECK_TRUE(!strcmp(stack_type, "gattlink"));
    service_type = GG_StackService_GetServiceType(service);
    CHECK_TRUE(!strcmp(service_type, "blast"));

    GG_StackService_Destroy(service);
}
