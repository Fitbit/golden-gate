// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.stack

import com.fitbit.goldengate.bindings.coap.CoapGroupRequestFilterMode
import com.fitbit.goldengate.bindings.io.Attachable
import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.util.Detachable
import java.io.Closeable

/**
 * Service that communicates with top of the stack
 */
interface StackService: Attachable, Detachable, Closeable {
    interface Provider {
        fun get() : StackService
    }

    /**
     * Update the current filter group used by the CoAP request filter.
     * @param CoapGroupRequestFilterMode group number
     * @param key unique identity of node
     */
    fun setFilterGroup(group: CoapGroupRequestFilterMode, key: NodeKey<String>) = Unit
}
