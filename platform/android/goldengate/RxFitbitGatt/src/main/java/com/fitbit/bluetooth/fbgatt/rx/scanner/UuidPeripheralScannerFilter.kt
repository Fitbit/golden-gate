// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.scanner

import android.os.ParcelUuid
import com.fitbit.bluetooth.fbgatt.FitbitGatt

/**
 * Will look for devices with given [uuid] after [mask] is applied
 */
class ServiceUuidPeripheralScannerFilter(
    private val uuid: ParcelUuid,
    private val mask: ParcelUuid
) : PeripheralScannerFilter {

    override fun add(fitbitGatt: FitbitGatt) {
        fitbitGatt.addScanServiceUUIDWithMaskFilter(uuid, mask)
    }
}
