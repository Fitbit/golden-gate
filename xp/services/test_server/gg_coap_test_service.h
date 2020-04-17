/**
 * @file
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @brief
 * Fitbit GG CoAP test server: Mirror and Shelf
 */

#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/remote/gg_remote.h"
#include "xp/coap/gg_coap.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct GG_CoapTestService GG_CoapTestService;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_TEST_SERVICE_MIRROR_URI         "test/mirror"
#define GG_COAP_TEST_SERVICE_SHELF_URI          "test/shelf"

#define GG_RAPI_COAP_TEST_SERVICE_START_METHOD  "coap_test_service/start"
#define GG_RAPI_COAP_TEST_SERVICE_STOP_METHOD   "coap_test_service/stop"

/*----------------------------------------------------------------------
|   methods
+---------------------------------------------------------------------*/
/**
 * Create a CoAP test service object.
 *
 * @param endpoint CoAP endpoint where the service is to be registered.
 * @param service [out] Address of the variable in which the object will be returned.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code if it failed.
 */
GG_Result GG_CoapTestService_Create(GG_CoapEndpoint*     endpoint,
                                    GG_CoapTestService** service);

/**
 * Destroy a CoAP test service object.
 *
 * @param self The object on which the method is invoked.
 */
void GG_CoapTestService_Destroy(GG_CoapTestService* self);

 /**
  * Register CoAP test service handlers.
  *
  * @param self Pointer to the CoAP test service object
  *
  * @return GG_Result value
  */
GG_Result GG_CoapTestService_Register(GG_CoapTestService* self);

 /**
  * Unregister CoAP test service handlers.
  *
  * @param self Pointer to the CoAP test service object
  *
  * @return GG_Result value
  */
GG_Result GG_CoapTestService_Unregister(GG_CoapTestService* self);

/**
 * Register the Remote API handlers for the CoAP test service with given shell instance
 *
 * @param remote_shell The remote shell to register the methods to
 * @param handler The platform handler object to register the methods with.
 *
 * @return GG_Result result of the registration
 */
GG_Result GG_CoapTestService_RegisterSmoHandlers(GG_RemoteShell*      remote_shell,
                                                 GG_RemoteSmoHandler* handler);

/**
 * Unregister the Remote API handlers from the CoAP test service with given shell instance.
 *
 * NOTE: this method may be called from any thread.
 *
 * @param remote_shell The remote shell to unregister the methods from
 * @param handler The platform handler object to unregister the methods from.
 *
 * @return GG_Result result of the unregistration
 */
GG_Result GG_CoapTestService_UnregisterSmoHandlers(GG_RemoteShell*      remote_shell,
                                                   GG_RemoteSmoHandler* handler);


/**
 * Handler to start/stop the CoAP test server service
 *
 * @param _self SmoHandler reference
 * @param request_method the remote api method name
 * @param request_params request parameters, represented as an SMO object
 * @param rpc_error_code JSON RPC error code that can be filled to signal error
 * @param rpc_result pointer to Fb_Smo reference, where result can be populated
 *
 * @return GG_Result result of the method
 */
GG_Result GG_CoapTestService_HandleRequest(GG_RemoteSmoHandler*   _self,
                                           const char*            request_method,
                                           Fb_Smo*                request_params,
                                           GG_JsonRpcErrorCode*   rpc_error_code,
                                           Fb_Smo**               rpc_result);

/**
 * Get a reference to the coap test server service GG_RemoteSmoHandler object
 *
 * @param self The object on which the method is invoked.
 *
 * @return Pointer to GG_RemoteSmoHandler object
 */
GG_RemoteSmoHandler* GG_CoapTestService_AsRemoteSmoHandler(GG_CoapTestService* self);

#if defined(__cplusplus)
}
#endif
