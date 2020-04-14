/**
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 *
 * @details
 *
 * Simple CoAP server command line application
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "xp/common/gg_port.h"
#include "xp/coap/gg_coap.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/module/gg_module.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/services/test_server/gg_coap_test_service.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define MAX_DATAGRAM_SIZE  2048

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);
} HelloHandler;

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);
} InternalServerErrorHandler;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
static GG_Result
HelloHandler_OnRequest(GG_CoapRequestHandler*   _self,
                       GG_CoapEndpoint*         endpoint,
                       const GG_CoapMessage*    request,
                       GG_CoapResponder*        responder,
                       const GG_BufferMetadata* transport_metadata,
                       GG_CoapMessage**         response)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);

    // get the second part of the path after the 'hello'
    GG_CoapMessageOptionIterator options;
    GG_CoapMessage_InitOptionIterator(request, GG_COAP_MESSAGE_OPTION_URI_PATH, &options);
    GG_CoapMessage_StepOptionIterator(request, &options);
    char payload[128] = { 0 };
    size_t payload_size = 0;
    if (options.option.number && options.option.value.string.length < 32) {
        memcpy(payload, "Hello ", 6);
        memcpy(payload+6, options.option.value.string.chars, options.option.value.string.length);
        payload_size = 6+options.option.value.string.length;
    } else {
        memcpy(payload, "Hello, World", 12);
        payload_size = 12;
    }

    GG_CoapMessageOptionParam option_param = {{
        .number             = GG_COAP_MESSAGE_OPTION_CONTENT_FORMAT,
        .type               = GG_COAP_MESSAGE_OPTION_TYPE_UINT,
        .value.uint         = GG_COAP_MESSAGE_FORMAT_ID_TEXT_PLAIN
    }, NULL, NULL};
    return GG_CoapEndpoint_CreateResponse(endpoint,
                                          request,
                                          GG_COAP_MESSAGE_CODE_CONTENT,
                                          &option_param,
                                          1,
                                          (const uint8_t*)payload,
                                          payload_size,
                                          response);
}

static GG_Result
InternalServerErrorHandler_OnRequest(GG_CoapRequestHandler*   _self,
                                     GG_CoapEndpoint*         endpoint,
                                     const GG_CoapMessage*    request,
                                     GG_CoapResponder*        responder,
                                     const GG_BufferMetadata* transport_metadata,
                                     GG_CoapMessage**         response)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(endpoint);
    GG_COMPILER_UNUSED(request);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);
    GG_COMPILER_UNUSED(response);

    return GG_ERROR_INTERNAL;
}

/*----------------------------------------------------------------------
|   functions tables
+---------------------------------------------------------------------*/
GG_IMPLEMENT_INTERFACE(HelloHandler, GG_CoapRequestHandler) {
    HelloHandler_OnRequest
};

GG_IMPLEMENT_INTERFACE(InternalServerErrorHandler, GG_CoapRequestHandler) {
    InternalServerErrorHandler_OnRequest
};

/*----------------------------------------------------------------------
|   main
+---------------------------------------------------------------------*/
int
main(int argc, char** argv)
{
    uint16_t server_port = GG_COAP_DEFAULT_PORT;

    // parse command-line arguments
    if (argc > 1) {
        if (!strcmp(argv[1], "--port")) {
            if (argc < 3) {
                fprintf(stderr, "ERROR: missing parameter for --port option\n");
                return 1;
            }
            server_port = (uint16_t)strtoul(argv[2], NULL, 10);
        } else {
            fprintf(stderr, "ERROR: invalid option %s\n", argv[1]);
            return 1;
        }
    }

    printf("=== Golden Gate CoAP Server port %d ===\n", server_port);

    // This isn't how a CoAP server will work, just a temporary test app to
    // experiment with the sockets and loops

    // initialize Golden Gate
    GG_Module_Initialize();

    // create a loop
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_Loop_BindToCurrentThread(loop);
    GG_ASSERT(GG_SUCCEEDED(result));

    // create a UDP socket
    GG_SocketAddress local_address = {
        GG_IpAddress_Any,
        server_port
    };
    GG_DatagramSocket* socket = NULL;
    result = GG_BsdDatagramSocket_Create(&local_address, NULL, false, MAX_DATAGRAM_SIZE, &socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_DatagramSocket_Create failed (%d)\n", result);
        return 1;
    }
    GG_DatagramSocket_Attach(socket, loop);

    // create a server
    GG_CoapEndpoint* server = NULL;
    result = GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(loop),
                                    GG_DatagramSocket_AsDataSink(socket),
                                    GG_DatagramSocket_AsDataSource(socket),
                                    &server);


    // create and register the HelloWorld handler (under two resource names: /hello and /helloworld)
    HelloHandler hello_handler;
    GG_SET_INTERFACE(&hello_handler, HelloHandler, GG_CoapRequestHandler);
    GG_CoapEndpoint_RegisterRequestHandler(server,
                                           "hello",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&hello_handler, GG_CoapRequestHandler));
    GG_CoapEndpoint_RegisterRequestHandler(server,
                                           "helloworld",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&hello_handler, GG_CoapRequestHandler));

    // create and register the InternalServerError handler
    InternalServerErrorHandler error_handler;
    GG_SET_INTERFACE(&error_handler, InternalServerErrorHandler, GG_CoapRequestHandler);
    GG_CoapEndpoint_RegisterRequestHandler(server,
                                           "error",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&error_handler, GG_CoapRequestHandler));

    // Create a coap test service
    GG_CoapTestService* coap_test_service = NULL;
    result = GG_CoapTestService_Create(server, &coap_test_service);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_CoapTestService Create failed (%d)\n", result);
        return 1;
    }
    // Register the coap test service handlers
    result = GG_CoapTestService_Register(coap_test_service);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_CoapTestService handle registration failed (%d)\n", result);
        return 1;
    }
    fprintf(stderr, "CoAP Test Service initialized\n");

    // loop!
    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    return 0;
}
