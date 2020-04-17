// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import io.reactivex.Observable

/**
 * Receives data from connected Node
 */
interface NodeDataReceiver {

    /**
     * Observer to receive data from connected Node
     */
    fun receive(): Observable<ByteArray>

}
