package com.fitbit.goldengate.bindings.dtls

/**
 * Registry for all [TlsKeyResolver], when resolving a keyId default [TlsKeyResolver] will be
 * applied fist followed by any registered [TlsKeyResolver] in order they are registered
 */
object TlsKeyResolverRegistry {

    // register [HelloTlsKeyResolver] as 1st default TlsKeyResolver
    private val firstTlsKeyResolver = HelloTlsKeyResolver()
    private var lastTlsKeyResolver: TlsKeyResolver = firstTlsKeyResolver

    /**
     * Get chain of [TlsKeyResolver]
     */
    internal val resolvers = firstTlsKeyResolver

    init {
        // register [BootstrapTlsKeyResolver] as 2nd default TlsKeyResolver
        register(BootstrapTlsKeyResolver())
    }

    /**
     * Register a new [TlsKeyResolver], this resolver will be used in order they are registered
     */
    fun register(tlsKeyResolver: TlsKeyResolver) {
        lastTlsKeyResolver.next = tlsKeyResolver
        lastTlsKeyResolver = tlsKeyResolver
    }
}
