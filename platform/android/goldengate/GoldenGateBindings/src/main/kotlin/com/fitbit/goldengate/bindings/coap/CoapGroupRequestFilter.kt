// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.NativeReference
import java.io.Closeable

/**
 * This class provides apis to setup/update CoAP request filters
 * group #1 is used for non-authenticated connections,
 * and group #0 (i.e. no filtering) is used for authenticated states.
 */
class CoapGroupRequestFilter: NativeReference, Closeable {

    override val thisPointer: Long

    init {
        thisPointer = create()
        this.setGroupTo(CoapGroupRequestFilterMode.GROUP_1)
    }

    /**
     * Set the current group mode for the CoAP request filter.
     * @param CoapGroupRequestFilterMode group number
     */
    fun setGroupTo(group: CoapGroupRequestFilterMode) {
        setGroup(thisPointer, group.value)
    }

    override fun close() {
        destroy(thisPointer)
    }

    private external fun create(): Long

    private external fun setGroup(selfPtr: Long = thisPointer, group: Byte)

    private external fun destroy(selfPtr: Long = thisPointer)
}
