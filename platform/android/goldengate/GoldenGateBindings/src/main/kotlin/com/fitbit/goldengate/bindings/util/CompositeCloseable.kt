package com.fitbit.goldengate.bindings.util

import java.io.Closeable
import java.util.Stack

class CompositeCloseable : Closeable {
    private val closeables: Stack<Closeable> = Stack()

    fun add(closable: Closeable) {
        closeables.add(closable)
    }

    override fun close() {
        while (closeables.isNotEmpty()) {
            closeables.pop().close()
        }
    }

    fun isEmpty() = closeables.isEmpty()
}
