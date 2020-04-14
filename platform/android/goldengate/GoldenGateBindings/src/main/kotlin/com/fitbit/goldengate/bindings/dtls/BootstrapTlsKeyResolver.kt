package com.fitbit.goldengate.bindings.dtls

import androidx.annotation.Keep
import com.fitbit.goldengate.bindings.node.NodeKey

internal val BOOTSTRAP_KEY_ID = "BOOTSTRAP".toByteArray()
internal val BOOTSTRAP_KEY = byteArrayOf(
        0x81.toByte(),
        0x06,
        0x54,
        0xE3.toByte(),
        0x36,
        0xAD.toByte(),
        0xCA.toByte(),
        0xB0.toByte(),
        0xA0.toByte(),
        0x3C,
        0x60,
        0xF7.toByte(),
        0x4A,
        0xA0.toByte(),
        0xB6.toByte(),
        0xFB.toByte()
)

/**
 * Static [TlsKeyResolver] for [BOOTSTRAP_KEY_ID] key ID
 */
@Keep
internal class BootstrapTlsKeyResolver : TlsKeyResolver() {

    override fun resolveKey(nodeKey: NodeKey<*>, keyId: ByteArray): ByteArray? {
        return when {
            keyId.contentEquals(BOOTSTRAP_KEY_ID) -> BOOTSTRAP_KEY
            else -> null
        }
    }
}
