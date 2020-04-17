// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.scanner

import com.fitbit.bluetooth.fbgatt.FitbitGatt

/**
 * Filter to add before starting next scan
 */
interface PeripheralScannerFilter {

    /**
     * Add filter, implementation is expected to call one of the add** filter method exposed on [FitbitGatt]
     */
    fun add(fitbitGatt: FitbitGatt)
}
