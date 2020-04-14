package com.fitbit.bluetooth.fbgatt.rx.scanner

import android.content.Context
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.BaseFitbitGattCallback
import io.reactivex.Observable
import io.reactivex.disposables.Disposable
import timber.log.Timber
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Reactive interface for scanning devices
 *
 * This class is designed to support to only run one scan at any given time. When you start a scan
 * any previous ongoing scan operation will get canceled before starting new scan operation.
 */
class PeripheralScanner(private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance()) {

    /**
     * Scan for devices and emits [ScanEvent]s. Will scan until [disposed][Disposable.dispose],
     *
     * Any previous ongoing scan wil be canceled before starting a this new scan operation
     *
     * @param context the [Context] used for the Android gatt layer
     * @param scanFilters filters to apply for this scan operation
     * @param resetExistingFilters  If TRUE existing filters will be reset before applying new one,
     *                              when FALSE existing filters will be applied in addition to one
     *                              provided with this method (default is FALSE)
     * @param cancelScanOnDispose will cancel if scan is on-going when the stream is disposed (TRUE by default)
     *
     * @return an [Observable] that emits [ScanEvent]s made during. Disposing of this
     * stream will stop the scan
     *
     */
    @Synchronized
    fun scanForDevices(
            context: Context,
            scanFilters: List<PeripheralScannerFilter> = emptyList(),
            resetExistingFilters: Boolean = false,
            cancelScanOnDispose: Boolean = true
    ): Observable<ScanEvent> {
        var callback: FitbitGatt.FitbitGattCallback? = null
        return Observable.create<ScanEvent> { emitter ->
            val scanStarted = AtomicBoolean(false)
            callback = object : BaseFitbitGattCallback() {

                override fun onBluetoothPeripheralDiscovered(connection: GattConnection) {
                    emitter.onNext(ScanEvent.Discovered(connection))
                }

                override fun onBluetoothPeripheralDisconnected(connection: GattConnection) {
                    emitter.onNext(ScanEvent.Disconnected(connection))
                }

                override fun onScanStopped() {
                    /*
                     * We can get this global callback called as result of calling `startHighPriorityScan`,
                     * if there was already ongoing scan operation in any mode.
                     *
                     * `scanStarted` check will ensure we only emmit complete if the callback if called after
                     * successfully starting `startHighPriorityScan` followed by stopping of the scan operation
                     */
                    if(scanStarted.get()) {
                        emitter.onComplete()
                    }
                }
            }.also {
                fitbitGatt.registerGattEventListener(it)
            }
            applyScanFilters(scanFilters, resetExistingFilters)
            // bitgatt library cancels any previous ongoing scan before starting a new one
            scanStarted.set(fitbitGatt.startHighPriorityScan(context))
            Timber.d("Started Scanning: ${scanStarted.get()}")
            if(!scanStarted.get()) {
                // complete the stream if starting scanner failed
                emitter.onComplete()
            }
        }.doFinally {
            callback?.let { fitbitGatt.unregisterGattEventListener(it) }
            if (fitbitGatt.isScanning && cancelScanOnDispose) {
                fitbitGatt.cancelScan(context)
            }
        }
    }

    private fun applyScanFilters(scanFilters: List<PeripheralScannerFilter>, resetExistingFilters: Boolean) {
        if (resetExistingFilters) {
            // first clear any existing filters
            fitbitGatt.resetScanFilters()
        }
        // add all requested filters for next scan
        scanFilters.forEach { scanFilter ->
            scanFilter.add(fitbitGatt)
        }
    }

    /**
     * Events encountered during a device scan
     */
    sealed class ScanEvent(open val connection: GattConnection) {

        /** A device was found */
        data class Discovered(override val connection: GattConnection) : ScanEvent(connection)

        /** A device disconnected */
        data class Disconnected(override val connection: GattConnection) : ScanEvent(connection)
    }

}
