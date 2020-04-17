/**
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2018-08-01
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/coap/gg_coap.h"
#include "xp/common/gg_results.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Initialize CoAP Client
 *
 * @param loop Loop on which to send CoAP requests
 * @param endpoint CoAP endpoint use to send requests and receive responses
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result CoapClient_Init(GG_Loop* loop, GG_CoapEndpoint* endpoint);

/**
 * CoAP Client CLI handler
 *
 * @param argc Number of CLI arguments (including command name)
 * @param argv Array of char* CLI arguments (argv[0] is "coap/client")
 *
 * @return 0 if the call succeeded, or a non-zero error code if it failed.
 */
int CoapClient_CLI_Handler(int argc, char** argv);
