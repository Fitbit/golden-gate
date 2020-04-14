/**
 * @file
 *
 * @copyright
 * Copyright 2017-2019 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-12-01
 *
 * @details
 *
 * CoAP Proxy Handler example
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include "xp/common/gg_port.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/handlers/gg_coap_splitter.h"
#include "xp/coap/handlers/gg_coap_helloworld_handler.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/module/gg_module.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define MAX_DATAGRAM_SIZE     2048

/*----------------------------------------------------------------------
|   CoAP handler that can be used to trigger a request coming from the
|   same endpoint
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);
    GG_IMPLEMENTS(GG_CoapResponseListener);
} TriggerHandler;

static GG_CoapRequestHandlerResult
TriggerHandler_OnRequest(GG_CoapRequestHandler*   _self,
                         GG_CoapEndpoint*         endpoint,
                         const GG_CoapMessage*    request,
                         GG_CoapResponder*        responder,
                         const GG_BufferMetadata* transport_metadata,
                         GG_CoapMessage**         response)
{
    GG_COMPILER_UNUSED(request);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);
    GG_COMPILER_UNUSED(response);
    TriggerHandler* self = GG_SELF(TriggerHandler, GG_CoapRequestHandler);

    // make a request
    printf("---> sending request\n");
    GG_CoapMessageOptionParam uri_param;
    size_t uri_param_count = 1;
    GG_Coap_SplitPathOrQuery("hello", '/', &uri_param, &uri_param_count, GG_COAP_MESSAGE_OPTION_URI_PATH);
    GG_CoapEndpoint_SendRequest(endpoint,
                                GG_COAP_METHOD_GET,
                                &uri_param,
                                1,
                                NULL,
                                0,
                                NULL,
                                GG_CAST(self, GG_CoapResponseListener),
                                NULL);

    return GG_CoapEndpoint_CreateResponse(endpoint, request, GG_COAP_MESSAGE_CODE_CONTENT, NULL, 0, NULL, 0, response);
}

GG_IMPLEMENT_INTERFACE(TriggerHandler, GG_CoapRequestHandler) {
    .OnRequest = TriggerHandler_OnRequest
};

static void
TriggerHandler_OnAck(GG_CoapResponseListener* self)
{
    GG_COMPILER_UNUSED(self);
}

static void
TriggerHandler_OnError(GG_CoapResponseListener* self, GG_Result error, const char* message)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(message);
    printf("TriggerHandler_OnError: %d\n", (int)error);
}

static void
TriggerHandler_OnResponse(GG_CoapResponseListener* self, GG_CoapMessage* response)
{
    GG_COMPILER_UNUSED(self);
    printf("TriggerHandler_OnResponse code=%d\n", GG_CoapMessage_GetCode(response));
}

GG_IMPLEMENT_INTERFACE(TriggerHandler, GG_CoapResponseListener) {
    .OnAck      = TriggerHandler_OnAck,
    .OnError    = TriggerHandler_OnError,
    .OnResponse = TriggerHandler_OnResponse
};

/*----------------------------------------------------------------------
|   main
|
| In this example, we instantiate a CoAP endpoint, a splitter and two
| sockets, one for the bottom and one for the top.
| The bottom socket listens on port 6683 and sends to port 5683.
| The top socket listens on port 7683 and sends to port 8683.
| This example is designed to run alongside a CoAP server listening on
| port 5683 (which will receive requests sent through the bottom socket
| and another one on port 8683, which will receive requests sent
| through the top socket.
| In addition, the "side" CoAP endpoint hosts two handlers: one is a
| handler that responds to GET requests for /helloworld, and the other
| listens for GET requests for /trigger, and when accessed will send a
| GET request from the host endpoint to /helloworld resource.
| Assuming that both CoAP servers on 5683 and 8683 both respond to
| GET requests for /hello (which the gg-coap-server application does),
| this example enables following scenarios:
|
| 1/ a GET request to coap://127.0.0.1:5683/hello should reach the "bottom"
| CoAP server directly (i.e not through the splitter)
|
| 2/ a GET request to coap://127.0.0.1:8683/hello should reach the "top"
| CoAP server directly (i.e not through the splitter)
|
| 3/ a GET request to coap://127.0.0.1:6683/helloworld should reach the
| "side" CoAP endpoint attached to the splitter.
|
| 4/ a GET request to coap://127.0.0.1:6683/hello should reach the "top"
| CoAP server through the splitter from the "bottom" socket (because the
| "side" CoAP endpoint doesn't have a handler for /hello)
|
| 5/ a GET request to coap://127.0.0.1:7683/hello should reach the "bottom"
| CoAP server through the splitter, from the "top" socket.
|
| 6/ a GET request to coap://127.0.0.1:6683/trigger should trigger a GET request
| *from *the "side" CoAP endpoint to the "bottom" CoAP server (which should
| return an empty response to the /trigger request, and show that the trigger
| handler itself receives a response from the /hello resource of the "bottom"
| server.
|
+---------------------------------------------------------------------*/
int
main(int argc, char** argv)
{
    GG_COMPILER_UNUSED(argc);
    GG_COMPILER_UNUSED(argv);

    printf("=== Golden Gate CoAP Splitter ===\n");

    // initialize Golden Gate
    GG_Module_Initialize();

    // setup a loop
    GG_Loop* loop;
    GG_Loop_Create(&loop);
    GG_Loop_BindToCurrentThread(loop);

    // create a UDP socket for the bottom
    GG_SocketAddress bottom_local_address = {
        GG_IpAddress_Any,
        6683
    };
    GG_SocketAddress bottom_remote_address = {
        GG_IpAddress_Any,
        5683
    };
    GG_DatagramSocket* bottom_socket = NULL;
    GG_Result result = GG_BsdDatagramSocket_Create(&bottom_local_address,
                                                   &bottom_remote_address,
                                                   false,
                                                   MAX_DATAGRAM_SIZE,
                                                   &bottom_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: cannot create bottom socket, GG_DatagramSocket_Create failed (%d)\n", result);
        return 1;
    }
    GG_DatagramSocket_Attach(bottom_socket, loop);

    // create a UDP socket for the top
    GG_SocketAddress top_local_address = {
        GG_IpAddress_Any,
        7683
    };
    GG_SocketAddress top_remote_address = {
        GG_IpAddress_Any,
        8683
    };
    GG_DatagramSocket* top_socket = NULL;
    result = GG_BsdDatagramSocket_Create(&top_local_address,
                                         &top_remote_address,
                                         false,
                                         MAX_DATAGRAM_SIZE,
                                         &top_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: cannot create top socket, GG_DatagramSocket_Create failed (%d)\n", result);
        return 1;
    }
    GG_DatagramSocket_Attach(top_socket, loop);

    // create a CoAP endpoint
    GG_CoapEndpoint* endpoint = NULL;
    GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(loop), NULL, NULL, &endpoint);

    // register a hello-world handler with the endpoint
    GG_CoapEndpoint_Register_HelloworldHandler(endpoint, GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET);

    // init and register a trigger handler
    TriggerHandler trigger_handler;
    GG_SET_INTERFACE(&trigger_handler, TriggerHandler, GG_CoapRequestHandler);
    GG_SET_INTERFACE(&trigger_handler, TriggerHandler, GG_CoapResponseListener);
    GG_CoapEndpoint_RegisterRequestHandler(endpoint,
                                           "trigger",
                                           GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_GET,
                                           GG_CAST(&trigger_handler, GG_CoapRequestHandler));

    // create the splitter
    GG_CoapSplitter* splitter;
    GG_CoapSplitter_Create(endpoint, &splitter);

    // set the splitter as the default handler
    GG_CoapEndpoint_SetDefaultRequestHandler(endpoint, GG_CoapSplitter_AsCoapRequestHandler(splitter));

    // set a token prefix so that responses can be differentiated
    uint8_t token_prefix[4] = {0x01, 0x02, 0x03, 0x04};
    GG_CoapEndpoint_SetTokenPrefix(endpoint, token_prefix, sizeof(token_prefix));

    // make the connections
    GG_DataSource_SetDataSink(GG_CoapSplitter_GetBottomPortAsDataSource(splitter),
                              GG_DatagramSocket_AsDataSink(bottom_socket));
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(bottom_socket),
                              GG_CoapSplitter_GetBottomPortAsDataSink(splitter));
    GG_DataSource_SetDataSink(GG_CoapSplitter_GetTopPortAsDataSource(splitter),
                              GG_DatagramSocket_AsDataSink(top_socket));
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(top_socket),
                              GG_CoapSplitter_GetTopPortAsDataSink(splitter));

    // loop!
    printf("+++ running loop\n");
    result = GG_Loop_Run(loop);
    printf("--- loop done\n");

    GG_Loop_Destroy(loop);
    GG_CoapSplitter_Destroy(splitter);
    GG_CoapEndpoint_Destroy(endpoint);
    GG_DatagramSocket_Destroy(top_socket);
    GG_DatagramSocket_Destroy(bottom_socket);

    return 0;
}
