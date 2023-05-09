// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.scanner

import com.fitbit.bluetooth.fbgatt.GattConnection
import org.mockito.kotlin.mock
import io.reactivex.Observable
import io.reactivex.observers.TestObserver
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4

@RunWith(JUnit4::class)
class BluetoothScannerCallbackTest {

    @Test
    fun testEmitter() {

        // Given
        val testObserver = TestObserver<GattConnection>()
        val connection1 = mock<GattConnection>()
        val connection2 = mock<GattConnection>()

        // When
        Observable.create<GattConnection> { emitter ->
            val callback = BluetoothScannerCallbackProvider()(emitter)
            callback.onBluetoothPeripheralDiscovered(connection1)
            callback.onBluetoothPeripheralDiscovered(connection2)
        }.take(2).subscribe(testObserver)


        // Then
        testObserver.assertValueAt(0, connection1)
        testObserver.assertValueAt(1, connection2)
        testObserver.assertComplete()
    }

}
