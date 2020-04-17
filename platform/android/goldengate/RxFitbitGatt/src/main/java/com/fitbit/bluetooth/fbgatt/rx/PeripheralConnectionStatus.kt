// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

/**
 * Peripheral bluetooth connectivity status with remote devices
 */
enum class PeripheralConnectionStatus {
    /**
     * Remote device is connected
     */
    CONNECTED,
    /**
     * Remote device is disconnected
     */
    DISCONNECTED
}
