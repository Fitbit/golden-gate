/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2018-05-18
 *
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

#include <string.h>
#include <stdio.h>

#include "fb_smo.h"
#include "xp/coap/gg_coap.h"
#include "xp/coap/gg_coap_message.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_threads.h"
#include "xp/loop/gg_loop.h"
#include "xp/remote/gg_remote.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_CoapClientService GG_CoapClientService;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_CLIENT_SERVICE_SEND_REQUEST_METHOD   "coap_client/send_request"
#define GG_COAP_CLIENT_SERVICE_GET_STATUS_METHOD     "coap_client/get_status"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Register the CoAP client service with a remote API shell.
 *
 * NOTE: this method may be called from any thread.
 *
 * @param self CoAP client object
 * @param shell GG_RemoteShell object to which Remote API endpoints are to be registered
 *
 */
GG_Result GG_CoapClientService_Register(GG_CoapClientService* self, GG_RemoteShell* shell);

/**
 * Unregister the CoAP client service from a remote API shell.
 * This function may only be called from the same thread as the one in which the shell is
 * running.

 * @param self CoAP client object
 * @param shell GG_RemoteShell object from which Remote API endpoints are to be unregistered
 *
 */
GG_Result GG_CoapClientService_Unregister(GG_CoapClientService* self, GG_RemoteShell* shell);

/**
 * Get a reference to the coap client service GG_RemoteSmoHandler object
 *
 * @param self The object on which the method is invoked.
 *
 * @return Pointer to GG_RemoteSmoHandler object
 */
GG_RemoteSmoHandler* GG_CoapClientService_AsRemoteSmoHandler(GG_CoapClientService* self);

/**
 * Method to create a coap client object
 *
 * @param loop Pointer to GG_Loop object that needs to be bound with service object
 * @param endpoint Pointer to CoAP endpoint object that needs to be bound with service object
 * @param service CoAP client object
 *
 * @return GG_Result Result of the operation
 */
GG_Result GG_CoapClientService_Create(GG_Loop*               loop,
                                      GG_CoapEndpoint*       endpoint,
                                      GG_CoapClientService** service);

/**
 * Method to destroy a coap client object
 *
 * @param service CoAP client object that needs to be destroyed
 */
void GG_CoapClientService_Destroy(GG_CoapClientService* service);

#if defined(__cplusplus)
}
#endif
