/**
 *
 * @file
 *
 * @copyright
 * Copyright 2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Bogdan Davidoaia
 *
 * @date 2018-11-07
 *
 * @details
 *
 * CoAP Helloworld handler interface
 */

# pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include <stdint.h>

#include "xp/coap/gg_coap.h"
#include "xp/common/gg_results.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Register Helloworld handler to a CoAP endpoint.
 *
 * @param endpoint Endpoint to which to register the handler
 * @param flags Flags associated with the handler for the request.
 *
 * @return GG_SUCCESS on success, or a negative error code.
 */
GG_Result GG_CoapEndpoint_Register_HelloworldHandler(GG_CoapEndpoint* endpoint,
                                                     uint32_t         flags);

/**
 * Unregister Helloworld handler from a CoAP endpoint.
 *
 * @param endpoint Endpoint from which to unregister the handler
 *
 * @return GG_SUCCESS on success, or a negative error code.
 */
GG_Result GG_CoapEndpoint_Unregister_HelloworldHandler(GG_CoapEndpoint* endpoint);
