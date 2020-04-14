package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService

/**
 * Used to expose building a stack for use with the Remote API
 */
object RemoteApiStackNodeBuilder {
    operator fun <T: StackService> invoke(
            clazz: Class<T>,
            stackConfig: StackConfig,
            stackServiceProvider: () -> T
    ): StackNodeBuilder<T> = StackNodeBuilder(clazz, stackConfig) {
        StackNode(it, stackConfig, stackServiceProvider())
    }
}
