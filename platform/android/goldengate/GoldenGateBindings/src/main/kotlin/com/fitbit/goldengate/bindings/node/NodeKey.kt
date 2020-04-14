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
