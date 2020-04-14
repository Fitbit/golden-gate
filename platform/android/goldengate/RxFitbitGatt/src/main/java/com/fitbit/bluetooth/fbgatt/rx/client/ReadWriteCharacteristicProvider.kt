package com.fitbit.bluetooth.fbgatt.rx.client

import io.reactivex.Scheduler
import io.reactivex.schedulers.Schedulers
import java.util.concurrent.Executors

/**
 * Provides [Scheduler] on which all remote GATT Characteristic read/write operations will be executed
 */
internal object ReadWriteCharacteristicProvider {

    /**
     * Read/Write GATT characteristic is not thread safe, so execute all transaction on single shared thread
     */
    val scheduler: Scheduler = Schedulers.from(Executors.newSingleThreadExecutor())
}
