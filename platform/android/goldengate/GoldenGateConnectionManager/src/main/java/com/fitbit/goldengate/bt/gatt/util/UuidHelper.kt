// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.util

import java.util.UUID

fun ByteArray.toUuid(): UUID {

    var msb: Long = 0
    var lsb: Long = 0
    for (i in 0..7) {
        msb = msb shl 8 or (this[i].toInt() and 255).toLong()
    }
    for (i in 8..15) {
        lsb = lsb shl 8 or (this[i].toInt() and 255).toLong()
    }

    return UUID(msb, lsb)
}
