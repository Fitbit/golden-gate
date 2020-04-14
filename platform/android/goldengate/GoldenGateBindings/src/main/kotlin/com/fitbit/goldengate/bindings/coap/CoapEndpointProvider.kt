package com.fitbit.goldengate.bindings.coap

import androidx.annotation.VisibleForTesting
import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import java.util.concurrent.ConcurrentHashMap

/**
 * Provider for [Endpoint] that ensures only single CoapEndpoint instance exist per node
 */
object CoapEndpointProvider {

    @VisibleForTesting
    internal val endpointMap = ConcurrentHashMap<NodeKey<*>, CoapEndpoint>()

    /**
     * Get the the [Endpoint] for the given node
     *
     * @param key unique identifier for a node
     * @return return single [Endpoint] instance for requested node
     */
    fun <T> getEndpoint(key: NodeKey<T>): Endpoint = getCoapEndpointInternal(key)

    /**
     * For **internal** use only.
     *
     * Get the the [StackService] for the given node.
     * Used from internal module code for attaching and detaching stack
     *
     * @param key unique identifier for a node
     * @return return single [StackService] instance for requested node
     */
    fun <T> getStackService(key: NodeKey<T>): StackService = getCoapEndpointInternal(key)

    /**
     * For **internal** use only.
     *
     * Get the the [CoapEndpoint] for the given node.
     *
     * @param key unique identifier for a node
     * @return return single [CoapEndpoint] instance for requested node
     */
    @Synchronized
    fun <T> getCoapEndpointInternal(key: NodeKey<T>): CoapEndpoint {
        var endpoint: CoapEndpoint? = endpointMap[key]
        if (endpoint == null) {
            endpoint = CoapEndpoint()
            endpointMap[key] = endpoint
        }
        return endpoint
    }
}
