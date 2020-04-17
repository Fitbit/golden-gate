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
 * @date 2019-02-19
 *
 * @details
 *
 * Object inspection example
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <inttypes.h>

#include "xp/common/gg_common.h"
#include "xp/module/gg_module.h"
#include "xp/loop/gg_loop.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_blockwise.h"
#include "xp/stack_builder/gg_stack_builder.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void
DummyListener_OnAck(GG_CoapResponseListener* self) {
    GG_COMPILER_UNUSED(self);
}

static void
DummyListener_OnError(GG_CoapResponseListener* self, GG_Result error, const char* message)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(error);
    GG_COMPILER_UNUSED(message);
}

static void
DummyListener_OnResponse(GG_CoapResponseListener* self, GG_CoapMessage* response)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(response);
}

static GG_CoapResponseListener* coap_listener = &GG_INTERFACE_INLINE_UNNAMED(GG_CoapResponseListener, {
    .OnAck      = DummyListener_OnAck,
    .OnError    = DummyListener_OnError,
    .OnResponse = DummyListener_OnResponse
});

//----------------------------------------------------------------------
static void
DummyBlockwiseListener_OnResponseBlock(GG_CoapBlockwiseResponseListener* self,
                                       GG_CoapMessageBlockInfo*          block_info,
                                       GG_CoapMessage*                   block_message)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(block_info);
    GG_COMPILER_UNUSED(block_message);
}

static void
DummyBlockwiseListener_OnError(GG_CoapBlockwiseResponseListener* self, GG_Result error, const char* message)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(error);
    GG_COMPILER_UNUSED(message);
}

static GG_CoapBlockwiseResponseListener* coap_blockwise_listener =
    &GG_INTERFACE_INLINE_UNNAMED(GG_CoapBlockwiseResponseListener, {
    .OnResponseBlock = DummyBlockwiseListener_OnResponseBlock,
    .OnError         = DummyBlockwiseListener_OnError
});

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate Timer Object Inspection Example ===\n");

    // init Golden Gate
    GG_Module_Initialize();

    // setup a loop
    GG_Loop* loop = NULL;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);


    // setup a stack
    GG_Stack* stack = NULL;
    uint8_t psk[16] = { 0 };
    uint8_t psk_identity[1] = { 0 };
    static GG_TlsClientOptions tls_options = {
        .base = {
            .cipher_suites       = NULL,
            .cipher_suites_count = 0
        }
    };
    tls_options.psk_identity      = psk_identity;
    tls_options.psk_identity_size = sizeof(psk_identity);
    tls_options.psk               = psk;
    tls_options.psk_size          = sizeof(psk);
    GG_StackBuilderParameters params = {
        .element_type = GG_STACK_ELEMENT_TYPE_DTLS_CLIENT,
        .element_parameters = &tls_options
    };
    GG_StackBuilder_BuildStack("DSNG",
                               &params,
                               1,
                               GG_STACK_ROLE_NODE,
                               NULL,
                               loop,
                               NULL,
                               NULL,
                               &stack);

    // setup a CoAP endpoint
    GG_CoapEndpoint* endpoint = NULL;
    GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(loop),
                           NULL,
                           NULL,
                           &endpoint);

    // queue a CoAP request
    GG_CoapMessageOptionParam options[2] = {
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "foo"),
        GG_COAP_MESSAGE_OPTION_PARAM_STRING(URI_PATH, "bar"),
    };
    uint8_t payload[3] = { 1, 2, 3 };
    GG_CoapEndpoint_SendRequest(endpoint,
                                GG_COAP_METHOD_GET,
                                options,
                                GG_ARRAY_SIZE(options),
                                payload,
                                sizeof(payload),
                                NULL,
                                coap_listener,
                                NULL);

    // queue a blockwise CoAP request
    GG_CoapEndpoint_SendBlockwiseRequest(endpoint,
                                         GG_COAP_METHOD_GET,
                                         options,
                                         GG_ARRAY_SIZE(options),
                                         NULL,
                                         0,
                                         NULL,
                                         coap_blockwise_listener,
                                         NULL);

    // create a logging inspector
    GG_LoggingInspector* inspector;
    GG_LoggingInspector_Create("foo.bar", GG_LOG_LEVEL_INFO, &inspector);

    // inspect the loop
    GG_Inspectable_Inspect(GG_Loop_AsInspectable(loop), GG_LoggingInspector_AsInspector(inspector), NULL);

    // inspect the stack
    GG_Inspectable_Inspect(GG_Stack_AsInspectable(stack), GG_LoggingInspector_AsInspector(inspector), NULL);

    // inspect the endpoint
    GG_Inspectable_Inspect(GG_CoapEndpoint_AsInspectable(endpoint), GG_LoggingInspector_AsInspector(inspector), NULL);

    // cleanup
    GG_CoapEndpoint_Destroy(endpoint);
    GG_Stack_Destroy(stack);
    GG_Loop_Destroy(loop);
    GG_LoggingInspector_Destroy(inspector);

    return 0;
}
