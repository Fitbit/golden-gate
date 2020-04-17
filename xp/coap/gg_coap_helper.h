/**
 * @file
 * @brief Helper structs, macros, functions to help with GG CoAP implementation.
 * Not esential to the CoAP implementation or of limited scope.
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2018-11-07
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "gg_coap.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Registration and unregistration helper element for CoAP handlers.
 * Using this will cull some of the boilerplate code in the handler implementation files.
 * Handler pointer is accesed through .node->handler
 */
typedef struct {
    GG_CoapRequestHandlerNode*   node;
    uint8_t                      method;
    char*                        uri;
} GG_CoapRequestHandlerRegistationData;

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
/**
 * Compile-time instancing macro for GG_CoapRequestHandlerRegistationData struct.
 *
 * ```
 * Example:
 * ```
 * GG_CoapRequestHandlerRegistationData reg_data[] = {
 *     GG_COAP_REQUEST_HANDLER_BUILD_REG_DATA(GG_CoAP_ClassName_RequestHandler,
 *                                            GG_CoAP_ClassName_RequestHandler_OnRequest,
 *                                            GG_COAP_REQUEST_HANDLER_FLAG_ALLOW_POST,
 *                                            "classname/set"),
 *     ...
 * }
 *
 */
#define GG_COAP_REQUEST_HANDLER_REGISTRATION_DATA(_handler_class_name,          \
                                                  _handler_func_name,           \
                                                  _coap_method,                 \
                                                  _uri) {                       \
    &((GG_CoapRequestHandlerNode) {                                             \
        .handler = GG_CAST(&((_handler_class_name) {                            \
                               GG_INTERFACE_INLINE(GG_CoapRequestHandler,       \
                                                   { _handler_func_name })      \
                           }),                                                  \
                           GG_CoapRequestHandler),                              \
    }),                                                                         \
    _coap_method,                                                               \
    _uri                                                                        \
}
