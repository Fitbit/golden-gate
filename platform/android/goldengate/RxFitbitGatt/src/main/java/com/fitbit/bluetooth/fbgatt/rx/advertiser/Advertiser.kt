package com.fitbit.bluetooth.fbgatt.rx.advertiser

import android.bluetooth.*
import android.bluetooth.le.AdvertiseCallback
import android.bluetooth.le.AdvertiseData
import android.bluetooth.le.AdvertiseSettings
import android.bluetooth.le.BluetoothLeAdvertiser
import android.content.Context
import android.os.ParcelUuid
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.GattTransactionCallback
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.runTxReactive
import io.reactivex.Completable
import io.reactivex.CompletableEmitter
import io.reactivex.Observable
import io.reactivex.Single
import timber.log.Timber
import java.lang.RuntimeException

/**
 * Utility class wrapping framework gattServer and rxGattServerCallback
 */
class Advertiser(val context: Context, private val bleAdvertiser: BluetoothLeAdvertiser = BluetoothAdapter.getDefaultAdapter().bluetoothLeAdvertiser) {

    /**
     * Temporary AdvertiseCallback until Bitgatt has advertising.
     */
    private inner class AdvertiserCallback : AdvertiseCallback() {

        @Volatile
        var completableEmitter: CompletableEmitter? = null

        override fun onStartSuccess(settingsInEffect: AdvertiseSettings) {
            super.onStartSuccess(settingsInEffect)
            Timber.d("Started server")
            completableEmitter?.onComplete()
        }

        override fun onStartFailure(errorCode: Int) {
            super.onStartFailure(errorCode)
            Timber.d("Failed to start server")
            completableEmitter?.onError(RuntimeException("Failed to start server with code $errorCode"))
        }
    }

    private val advertiseCallback = AdvertiserCallback()

    fun startAdvertising(advertiseSettings: AdvertiseSettings,
                         advertiseData: AdvertiseData,
                         scanResponse: AdvertiseData? = null):
        Completable = Completable.create { emitter ->
        advertiseCallback.completableEmitter = emitter
        bleAdvertiser.startAdvertising(advertiseSettings,
            advertiseData,
            scanResponse,
            advertiseCallback)
    }

    fun stopAdvertising(): Completable = Completable.fromAction {
        bleAdvertiser.stopAdvertising(advertiseCallback)
    }

    companion object {
        fun getAdvertiseSettings(): AdvertiseSettings {
            val advertiseSettingsBuilder = AdvertiseSettings.Builder()
            advertiseSettingsBuilder.setConnectable(true)
            advertiseSettingsBuilder.setAdvertiseMode(AdvertiseSettings.ADVERTISE_MODE_LOW_LATENCY)
            advertiseSettingsBuilder.setTimeout(0)
            advertiseSettingsBuilder.setTxPowerLevel(AdvertiseSettings.ADVERTISE_TX_POWER_MEDIUM)
            return advertiseSettingsBuilder.build()
        }

        fun getAdvertiseData(uuid: String): AdvertiseData {
            val advertiseDataBuilder = AdvertiseData.Builder()
            advertiseDataBuilder.addServiceUuid(ParcelUuid.fromString(uuid))
            advertiseDataBuilder.setIncludeDeviceName(false)
            advertiseDataBuilder.setIncludeTxPowerLevel(false)
            return advertiseDataBuilder.build()
        }

        fun getScanResponseData(): AdvertiseData {
            val advertiseDataBuilder = AdvertiseData.Builder()
            advertiseDataBuilder.setIncludeDeviceName(true)
            advertiseDataBuilder.setIncludeTxPowerLevel(true)
            return advertiseDataBuilder.build()
        }
    }
}
