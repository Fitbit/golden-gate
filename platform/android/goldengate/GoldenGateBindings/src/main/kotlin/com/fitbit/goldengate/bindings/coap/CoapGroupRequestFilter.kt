// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.coap

import com.fitbit.goldengate.bindings.NativeReferenceWithCallback
import com.fitbit.goldengate.bindings.util.isNotNull
import timber.log.Timber
import java.io.Closeable

/**
 * This class provides apis to setup/update CoAP request filters
 * group #1 is used for non-authenticated connections,
 * and group #0 (i.e. no filtering) is used for authenticated states.
 */
class CoapGroupRequestFilter : NativeReferenceWithCallback, Closeable {

    @get:Synchronized
    @set:Synchronized
    override var thisPointerWrapper: Long = 0

    override fun onFree() {
        thisPointerWrapper = 0
        Timber.d("free the native reference")
    }

    init {
        thisPointerWrapper = create()
        this.setGroupTo(CoapGroupRequestFilterMode.GROUP_1)
    }

    /**
     * Set the current group mode for the CoAP request filter.
     * @param CoapGroupRequestFilterMode group number
     */
    fun setGroupTo(group: CoapGroupRequestFilterMode) {
        if (thisPointerWrapper.isNotNull()) {
            setGroup(thisPointerWrapper, group.value)
        }
    }

    override fun close() {
        if (thisPointerWrapper.isNotNull()) {
            destroy(thisPointerWrapper)
        }
    }

    private external fun create(): Long

    private external fun setGroup(selfPtr: Long, group: Byte)

    private external fun destroy(selfPtr: Long)
}
