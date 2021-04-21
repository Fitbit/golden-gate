// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi.handlers

import co.nstant.`in`.cbor.model.DataItem
import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.UnicodeString
import com.fitbit.goldengate.bindings.dtls.InMemoryModeTlsKeyResolver
import com.fitbit.goldengate.bindings.dtls.TlsKeyResolverRegistry
import com.fitbit.goldengate.bindings.remote.CborHandler
import com.fitbit.goldengate.bindings.util.hexStringToByteArray
import com.fitbit.remoteapi.StagedCborHandler
import timber.log.Timber

private const val TLS_KEY_INPUT_LENGTH = 32

/**
 * Remote API Handler for DTLS key test
 */
class SetTlsKeyRpc private constructor() {
    companion object Handler {
        operator fun invoke(tlsKeyResolverRegistry: TlsKeyResolverRegistry = TlsKeyResolverRegistry
        ): CborHandler {
            return StagedCborHandler(
                    "stack/set_dtls_key",
                    StagedCborHandler.Stages(
                            SetTlsKeyRpc.Parser(),
                            SetTlsKeyRpc.Processor(tlsKeyResolverRegistry),
                            NullResponder()
                    )
            )
        }
    }

    class Parser : StagedCborHandler.Parser<Pair<String, String>> {
        private val keyIdArgumentErrorMessage = "key identity should not be empty"
        private val keyArgumentErrorMessage = "key identity should not be empty"

        override fun parse(dataItems: List<DataItem>): Pair<String, String> {
            val paramsMap = dataItems.firstOrNull { it is Map } as Map?
            val keyInfo = paramsMap?.get(UnicodeString("keyinfo")) as? Map

            val keyIdentity = keyInfo?.get(UnicodeString("key_identity"))
                    ?.let { it as? UnicodeString }
                    ?.string ?: throw IllegalArgumentException(keyIdArgumentErrorMessage)

            val key = keyInfo.get(UnicodeString("key"))
                    ?.let { it as? UnicodeString }
                    ?.string ?: throw IllegalArgumentException(keyArgumentErrorMessage)

            require(keyIdentity.isNotEmpty()) { keyIdArgumentErrorMessage }
            require(key.length == TLS_KEY_INPUT_LENGTH) { keyArgumentErrorMessage }

            Timber.w("test mode key_id: $keyIdentity, key: $key")

            return Pair(keyIdentity, key)
        }
    }

    class Processor(
            private val tlsKeyResolverRegistry: TlsKeyResolverRegistry
    ) : StagedCborHandler.Processor<Pair<String, String>, Unit> {

        override fun process(parameterObject: Pair<String, String>) {
            val keyIdentity = parameterObject.first
                    .toByteArray()
            val key = parameterObject.second
                    .hexStringToByteArray()
            tlsKeyResolverRegistry.register(InMemoryModeTlsKeyResolver(keyIdentity, key))
        }
    }
}
