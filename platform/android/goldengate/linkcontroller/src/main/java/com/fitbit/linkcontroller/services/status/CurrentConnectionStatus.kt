// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller.services.status

import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.experimental.and

/**
 * This class has Connection Status information.
 * This data is read from the tracker
 * @property bonded The tracker is in bonded state
 * @property encrypted The tracker is encrypted
 * @property dleEnabled The tracker has data length extension enabled
 * @property dleReboot the tracker requests a reboot to enable dle
 *
 */
data class CurrentConnectionStatus(
    val bonded: Boolean = false,
    val encrypted: Boolean = false,
    val dleEnabled: Boolean = false,
    val dleReboot: Boolean = false,
    // next values are valid only if dle is enabled
    val maxTxPayload: Byte = 0x1B,  // valid range: 0x1B - 0xFB
    val maxTxTime: Short = 0x148,    // valid range: 0x148 -0x848
    val maxRxPayload: Byte = 0x1B,  // valid range: 0x1B - 0xFB
    val maxRxTime: Short = 0x148  // valid range: 0x148 -0x848
) {

    companion object {

        fun parseFromByteArray(byteArray: ByteArray): CurrentConnectionStatus {

            val buffer = ByteBuffer.wrap(byteArray).order(ByteOrder.LITTLE_ENDIAN)
            val flags = buffer.get()
            val bonded = (flags and 0x01) == 0x01.toByte()
            val encrypted = (flags and 0x02) == 0x02.toByte()
            val dleEnabled = (flags and 0x04) == 0x04.toByte()
            val dleReboot = (flags and 0x08) == 0x08.toByte()
            val maxTxPayload = buffer.get()
            val maxTxTime = buffer.short
            val maxRxPayload = buffer.get()
            val maxRxTime = buffer.short

            return CurrentConnectionStatus(
                bonded, encrypted, dleEnabled, dleReboot,
                maxTxPayload, maxTxTime, maxRxPayload, maxRxTime
            )


        }

    }
}
