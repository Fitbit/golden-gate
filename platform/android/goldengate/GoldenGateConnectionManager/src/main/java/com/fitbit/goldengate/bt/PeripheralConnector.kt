package com.fitbit.goldengate.bt

import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.getGattConnection
import io.reactivex.Completable
import io.reactivex.Single
import timber.log.Timber

/**
 * Utility to connect and disconnect to a GoldenGate device
 *
 * @param bluetoothAddress BT address of Peripheral to connect to
 * @param priority connection priority requested (default to High)
 */
internal class PeripheralConnector(
        private val bluetoothAddress: String,
        private val priority: Int = BluetoothGatt.CONNECTION_PRIORITY_HIGH,
        private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
        private val peripheralProvider: (gattConnection: GattConnection) -> BitGattPeripheral = { gattConnection -> BitGattPeripheral(gattConnection) }
) {

    /**
     * Connect to the given device if not already connected.
     *
     * It is assumed that call has already discovered the device via BT scan operation, else BT
     * connect is known to fail will status 133 error, in which case this method will return error.
     *
     * @return Emits [GattConnection] if already connected or a connection succeeds
     */
    fun connect(): Single<GattConnection> {
        // Getting matching connection as we assume device has already been discovered
        return fitbitGatt.getGattConnection(bluetoothAddress)
                .toSingle()
                .doOnError { Timber.w(it, "Error getting Connection for: $bluetoothAddress, may be a result of not performing BT scan with all edge cases") }
                .flatMap { gattConnection -> connect(gattConnection) }
    }

    fun connect(gattConnection: GattConnection): Single<GattConnection> {
        val peripheral = peripheralProvider(gattConnection)
        return peripheral.connect() // will be noop is device already connected
                .map{ gattConnection }
                .doOnSubscribe { Timber.d("Connecting to device: $bluetoothAddress GattConnection: $gattConnection") }
                .doOnSuccess { Timber.d("Successfully connected to : $bluetoothAddress GattConnection: $gattConnection") }
                .doOnError { Timber.w("Failed to connect to device: $bluetoothAddress GattConnection: $gattConnection") }
    }

}
