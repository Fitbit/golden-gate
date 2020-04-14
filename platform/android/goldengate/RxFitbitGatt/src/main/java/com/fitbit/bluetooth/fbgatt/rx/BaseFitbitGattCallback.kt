package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.FitbitGatt.FitbitGattCallback
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.exception.BitGattStartException

/**
 * An abstract adapter class of [FitbitGattCallback] that provides no implementation
 * for all callbacks, given implementation ability to only add implementation for
 * specific callback method
 */
abstract class BaseFitbitGattCallback: FitbitGattCallback {

    override fun onBluetoothPeripheralDisconnected(connection: GattConnection) {
    }

    override fun onBluetoothPeripheralDiscovered(connection: GattConnection) {
    }

    override fun onScanStopped() {
    }

    override fun onPendingIntentScanStarted() {
    }

    override fun onScanStarted() {
    }

    override fun onPendingIntentScanStopped() {
    }

    override fun onBluetoothOff() {
    }

    override fun onBluetoothOn() {
    }

    override fun onBluetoothTurningOn() {
    }

    override fun onBluetoothTurningOff() {
    }

    override fun onGattClientStartError(exception: BitGattStartException?) {
    }

    override fun onGattServerStartError(exception: BitGattStartException?) {
    }

    override fun onScannerInitError(exception: BitGattStartException?) {
    }

    override fun onGattServerStarted(serverConnection: GattServerConnection?) {
    }

    override fun onGattClientStarted() {
    }
}