// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import io.reactivex.Observer

/**
 * Holds common data for [IncomingRequest] and [OutgoingRequest]
 */
interface BaseRequest {

    /**
     * Request [Method] type
     */
    val method: Method

    /**
     * Observer for the number of bytes Sent/Received in a Blockwise Operation
     */
    val progressObserver: Observer<Int>
}
