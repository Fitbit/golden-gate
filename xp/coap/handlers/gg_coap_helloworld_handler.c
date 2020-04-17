/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2018-11-07
 *
 * @details
 *
 * CoAP Helloworld handler implementation
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include <string.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_types.h"

#include "xp/coap/handlers/gg_coap_helloworld_handler.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/

#define GG_COAP_HELLOWORLD_URI "helloworld"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

typedef struct {
    GG_IMPLEMENTS(GG_CoapRequestHandler);
} HelloworldHandler;

/*----------------------------------------------------------------------
|   function prototypes
+---------------------------------------------------------------------*/

static GG_Result HelloHandler_OnRequest(GG_CoapRequestHandler*   _self,
                                        GG_CoapEndpoint*         endpoint,
                                        const GG_CoapMessage*    request,
                                        GG_CoapResponder*        responder,
                                        const GG_BufferMetadata* transport_metadata,
                                        GG_CoapMessage**         response);

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/

GG_SET_LOCAL_LOGGER("gg.xp.coap.handlers.helloworld")

static HelloworldHandler g_helloworld_handler;
static GG_CoapRequestHandlerNode g_helloworld_node = {
    .handler = GG_CAST(&g_helloworld_handler, GG_CoapRequestHandler)
};

GG_IMPLEMENT_INTERFACE(HelloHandler, GG_CoapRequestHandler) {
    HelloHandler_OnRequest
};

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result HelloHandler_OnRequest(GG_CoapRequestHandler*   _self,
                                        GG_CoapEndpoint*         endpoint,
                                        const GG_CoapMessage*    request,
                                        GG_CoapResponder*        responder,
                                        const GG_BufferMetadata* transport_metadata,
                                        GG_CoapMessage**         response)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(responder);
    GG_COMPILER_UNUSED(transport_metadata);
    char payload[] = "Hello CoAP client!";

    GG_LOG_INFO("Got CoAP helloworld request");

    GG_CoapMessageOptionParam option_param = {
        .option = {
            .number = GG_COAP_MESSAGE_OPTION_CONTENT_FORMAT,
            .type = GG_COAP_MESSAGE_OPTION_TYPE_UINT,
            .value.uint = GG_COAP_MESSAGE_FORMAT_ID_TEXT_PLAIN
        },
        .next = NULL
    };

    return GG_CoapEndpoint_CreateResponse(endpoint,
                                        request,
                                        GG_COAP_MESSAGE_CODE_CONTENT,
                                        &option_param,
                                        1,
                                        (const uint8_t*)payload,
                                        strlen(payload),
                                        response);
}

//----------------------------------------------------------------------
GG_Result GG_CoapEndpoint_Register_HelloworldHandler(GG_CoapEndpoint* endpoint,
                                                     uint32_t         flags)
{
    GG_SET_INTERFACE(&g_helloworld_handler, HelloHandler, GG_CoapRequestHandler);

    return GG_CoapEndpoint_RegisterRequestHandlerNode(endpoint,
                                                      GG_COAP_HELLOWORLD_URI,
                                                      flags,
                                                      &g_helloworld_node);
}

//----------------------------------------------------------------------
GG_Result GG_CoapEndpoint_Unregister_HelloworldHandler(GG_CoapEndpoint *endpoint)
{
    return GG_CoapEndpoint_UnregisterRequestHandler(endpoint,
                                                    GG_COAP_HELLOWORLD_URI,
                                                    GG_CAST(&g_helloworld_handler,
                                                            GG_CoapRequestHandler));
}
