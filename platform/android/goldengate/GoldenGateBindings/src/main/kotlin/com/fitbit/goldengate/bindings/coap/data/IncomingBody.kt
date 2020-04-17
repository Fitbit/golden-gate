// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap.data

import io.reactivex.Single


/**
 * Represents the body of a incoming coap message.
 */
interface IncomingBody {

    // TODO: FC-1303  - Add stream

    /**
     * Body returned as [Data]
     */
    fun asData(): Single<Data>
}
