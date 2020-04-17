// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.node

/**
 * Marker interface for uniquely identifying a remote Node device
 */
interface NodeKey<out T> {
    /**
     * Unique identifier for a Node
     */
    val value: T
}
