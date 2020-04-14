package com.fitbit.bluetooth.fbgatt.rx.client

import java.util.concurrent.Semaphore

/**
 * A lock to synchronize remote GATT Characteristic value change until the read/write transaction is complete
 */
internal object ReadWriteCharacteristicLock {

    val lock = Semaphore(1)

}
