package com.fitbit.goldengate.bt.gatt.scanner

import com.fitbit.bluetooth.fbgatt.GattConnection
import io.reactivex.Observable
import io.reactivex.Single

interface BluetoothScanner {

    /**
     * Scans for a device with a specific mac address
     * @param macAddress The macaddress of the device
     */
    fun scanForDevice(macAddress: String): Single<GattConnection>

    /**
     * Scans for devices
     * @return An observable that will emit devices
     */
    fun scanForDevices(): Observable<GattConnection>

    /**
     * Scans for devices matching a specific name
     * @param name The name of the device for which to look
     * @return A [Single] emitting a [GattConnection]
     */
    fun scanForName(name: String): Observable<GattConnection>

}