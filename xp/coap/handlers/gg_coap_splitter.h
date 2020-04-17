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
 * @date 2018-12-01
 *
 * @details
 *
 * CoAP Splitter
 */

#pragma once

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
#include "xp/common/gg_common.h"
#include "xp/coap/gg_coap.h"
#include "xp/sockets/gg_sockets.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * A CoAP splitter is an object that allows "splitting" CoAP traffic
 * over two separate domains in a way that makes them appear to the
 * outside as a single domain.
 * In order to remain lightweight, this implementation isn't completely
 * generic, so as to avoid having to re-write CoAP datagrams. One of the
 * limitations is that while the splitter can work with CoAP clients that
 * don't send requests from a fixed port number, it can handle sequential
 * requests, but not concurrent requests from clients on different ports.
 *
 * The splitter object is connected to a "bottom" socket, from which
 * requests are received, will split CoAP traffic between a "side" CoAP
 * endpoint and a "top" socket, behind which another CoAP endpoint is
 * located. In addition the splitter implements the GG_CoapResponseHandler
 * interface so that it can be invoked by the "side" CoAP endpoint when
 * it processes a request for which is has no local handler.
 *
 * The splitter routes datagrams according to the following rules:
 *
 * 1/ Requests received from the "bottom" socket are forwarded to the "side"
 * CoAP endpoint, which will look for a handler for the request. If a handler
 * is found, a response is sent back to the splitter, which then sends it
 * through the "bottom" socket back to the client that sent the request.
 * If no local handler is found, the OnRequest handler of the splitter is
 * invoked, which forwards the request to the "top" socket.
 *
 * 2/ Responses received from the "bottom" socket are examined, and routed
 * either to the "side" CoAP endpoint, or to the "top" socket, based on
 * whether a specified pattern prefix is found in the response token.
 *
 * 3/ Requests received from the "side" endpoint are forwarded to the "bottom" socket.
 *
 * 4/ Responses received from the "side" endpoint are forwarded to the "bottom" socket.
 *
 * 5/ Requests received from the "top" socket are forwarded to the "bottom" socket.
 *
 * 6/ Responses received from the "top" socket are forwarded to the "bottom" socket.
 *
 *                              +----------------------+
 *                              |     CoAP Endpoint    |
 *                              +----------------------+
 *                                 ^     ^     |     |
 *                                 |     |     |     |
 *                               [REQ] [RSP] [REQ] [RSP]
 *                                 |     |     |     |     (IP Addr A)
 *                                 |     |     v     v
 *                              +----------------------+
 *                              |     Bottom Socket    |
 *                              +----------------------+
 *                                 ^     ^     |     |
 *                                 |     |     |     |
 *                               [REQ] [RSP] [REQ] [RSP]
 *                                 |     |     |     |
 * ~~~~~~~(IP connection)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                                 |     |     |     |
 *                                 |     |     v     v
 *                             +------------------------+
 *                             |        Top Port        |
 *                             +------------------------+
 *                             |   ^     ^     |     |  |
 * +-----------+               |   |     |     |     |  |
 * |           |==[OnRequest]=>|---+     |     |     |  |
 * |  Side     |--[RSP]------->|---------|-----+     |  |
 * |  CoAP     |--[REQ]------->|---------|-----|-----+  |  (IP Addr B)
 * |  Endpoint |<-[RSP]--------|<--------+     |     |  |
 * |           |<-[REQ]--------|<--+     |     |     |  |
 * +-----------+               |   |     |     v     v  |
 *                             +------------------------+
 *                             |       Bottom Port      |
 *                             +------------------------+
 *                                 ^     ^     |     |
 *                                 |     |     |     |
 * ~~~~~~~(IP connection)~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *                                 |     |     |     |
 *                               [REQ] [RSP] [REQ] [RSP]
 *                                 |     |     |     |
 *                                 |     |     v     v
 *                              +----------------------+
 *                              |     Bottom Socket    |
 *                              +----------------------+
 *                                 ^     ^     |     |
 *                                 |     |     |     |
 *                               [REQ] [RSP] [REQ] [RSP]
 *                                 |     |     |     |    (IP Addr C)
 *                                 |     |     v     v
 *                              +----------------------+
 *                              |     CoAP Endpoint    |
 *                              +----------------------+
 */
typedef struct GG_CoapSplitter GG_CoapSplitter;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_COAP_SPLITTER_DEFAULT_CONTEXT_COUNT 32

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/


/**
 * Create an instance.
 *
 * The new object will connect itself to the CoAP endpoint passed to
 * the constructor, but won't automatically register itself as the
 * default handler (because that may not always be desired). So if
 * this splitter should be the default handler for the CoAP endpoint,
 * it is the responsibility of the caller to invoke
 * GG_CoapEndpoint_SetDefaultRequestHandler, passing
 * GG_CoapSplitter_AsCoapRequestHandler(splitter) as an argument.
 *
 * @param endpoint CoAP endpoint to use as the 'side' component.
 * @param splitter Pointer to the variable where the instance will be returned.
 *
 * @return GG_SUCCESS if the call succeeded, or a negative error code.
 */
GG_Result GG_CoapSplitter_Create(GG_CoapEndpoint*  endpoint,
                                 GG_CoapSplitter** splitter);

/**
 * Destroy an instance.
 *
 * @param self The object on which this method is invoked.
 */
void GG_CoapSplitter_Destroy(GG_CoapSplitter* self);

/**
 * Obtain the GG_CoapRequestHandler interface for the object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's GG_CoapRequestHandler interface.
 */
GG_CoapRequestHandler* GG_CoapSplitter_AsCoapRequestHandler(GG_CoapSplitter* self);

/**
 * Obtain the GG_DataSource interface for the object's bottom port.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's bottom port's GG_DataSource interface.
 */
GG_DataSource* GG_CoapSplitter_GetBottomPortAsDataSource(GG_CoapSplitter* self);

/**
 * Obtain the GG_DataSink interface for the object's bottom port.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's bottom port's GG_DataSink interface.
 */
GG_DataSink* GG_CoapSplitter_GetBottomPortAsDataSink(GG_CoapSplitter* self);

/**
 * Obtain the GG_DataSource interface for the object's top port.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's top port's GG_DataSource interface.
 */
GG_DataSource* GG_CoapSplitter_GetTopPortAsDataSource(GG_CoapSplitter* self);

/**
 * Obtain the GG_DataSink interface for the object's top port.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's top port's GG_DataSink interface.
 */
GG_DataSink* GG_CoapSplitter_GetTopPortAsDataSink(GG_CoapSplitter* self);
