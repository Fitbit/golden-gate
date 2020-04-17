// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSender
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSenderProvider
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.ReceiveCharacteristic
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.anyOrNull
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.never
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Completable
import io.reactivex.schedulers.Schedulers
import org.junit.After
import org.junit.Before
import org.junit.Test
import java.util.Arrays
import java.util.UUID

class ReceiveCharacteristicDataListenerTest {

    private val device1 = mock<BluetoothDevice> {
        on { address } doReturn "1"
    }
    private val device2 = mock<BluetoothDevice> {
        on { address } doReturn "2"
    }
    private val data = byteArrayOf(0x01)

    private val mockGattServerConnection = mock<GattServerConnection>()
    private val mockGattServerResponseSender = mock<GattServerResponseSender>()
    private val mockGattServerResponseSenderProvider = mock<GattServerResponseSenderProvider> {
        on { provide(mockGattServerConnection) } doReturn mockGattServerResponseSender
    }

    private val listener = ReceiveCharacteristicDataListener(
            Schedulers.trampoline(),
            mockGattServerResponseSenderProvider
    )

    @Before
    fun setup() {
        mockGattServerResponse()
    }

    @After
    fun tearDown() {
        listener.unregisterAll()
    }

    @Test
    fun shouldReceiveDataOnRegisteredReceiver() {
        // register receiver
        val dataObservable = listener.register(device1)

        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult()
        )

        // verify data is available on previously registered receiver
        dataObservable
                .test()
                .assertValue { Arrays.equals(it, data) }
    }

    @Test
    fun shouldOnlyReceiveDataOnMatchingDeviceReceiver() {
        // register TWO device receiver
        val dataObservable1 = listener.register(device1)
        val dataObservable2 = listener.register(device2)

        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult()
        )

        // verify data is ONLY available on receiver registered for specific device
        dataObservable1
                .test()
                .assertValue { Arrays.equals(it, data) }

        // verify no data available
        dataObservable2
                .test()
                .assertNoValues()
    }

    @Test
    fun shouldRespondWithErrorIfNoRegisteredReceiver() {
        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult()
        )

        // verify error response send, as there is no registered handler
        verify(mockGattServerResponseSender).send(
                device = FitbitBluetoothDevice(device1),
                requestId = 1,
                status = BluetoothGatt.GATT_FAILURE
        )
    }

    @Test
    fun shouldRespondWithSuccessIfThereIsARegisteredReceiver() {
        // register receiver
        listener.register(device1)

        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult()
        )

        // verify error response send, as there is no registered handler
        verify(mockGattServerResponseSender).send(
                device = FitbitBluetoothDevice(device1),
                requestId = 1,
                status = BluetoothGatt.GATT_SUCCESS
        )
    }

    @Test
    fun shouldRespondWithSuccessIfNullDataIsReceived() {
        // register receiver
        listener.register(device1)

        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(data = null)
        )

        // verify error response send, as there is no registered handler
        verify(mockGattServerResponseSender).send(
                device = FitbitBluetoothDevice(device1),
                requestId = 1,
                status = BluetoothGatt.GATT_SUCCESS
        )
    }

    @Test
    fun shouldRespondWithErrorIfReceiverUnregistered() {
        // register and than unregister same receiver
        listener.register(device1)
        listener.unregister(device1)

        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult()
        )

        // verify error response send, as there is no registered handler
        verify(mockGattServerResponseSender).send(
                device = FitbitBluetoothDevice(device1),
                requestId = 1,
                status = BluetoothGatt.GATT_FAILURE
        )
    }

    @Test
    fun shouldNotRespondWithResponseIfNotRequired() {
        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(responseNeeded = false)
        )

        // verify No error response send for unregistered handler, as response if not required
        verify(mockGattServerResponseSender, never()).send(
                device = FitbitBluetoothDevice(device1),
                requestId = 1,
                status = BluetoothGatt.GATT_FAILURE
        )
    }

    @Test
    fun shouldIgnoreIfCallbackIsNotForReceiveCharacteristic() {
        val unknownCharacteristicId = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF")

        // register receiver
        val dataObservable = listener.register(device1)

        // mock to trigger data written on unknownCharacteristic
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(characteristicUuid = unknownCharacteristicId)
        )

        // verify that no data available message is sent
        dataObservable
                .test()
                .assertNoValues()
        // verify NO error response send
        verify(mockGattServerResponseSender, never()).send(any(), any(), any(), any(), any(), anyOrNull())
    }

    @Test
    fun shouldOnlyReceiveDataOnLatestRegisteredDeviceReceiver() {
        // register TWO device receiver
        val dataObservable1 = listener.register(device1)
        val dataObservable2 = listener.register(device1)

        // mock to trigger data written on Rx
        listener.onServerCharacteristicWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult()
        )

        // verify data is NOT available on older receiver
        dataObservable1
                .test()
                .assertNoValues()

        // verify data is only available on latest registered receiver
        dataObservable2
                .test()
                .assertValue { Arrays.equals(it, data) }
    }

    private fun mockGattServerResponse() {
        whenever(mockGattServerResponseSender.send(any(), any(), any(), any(), any(), anyOrNull())).thenReturn(Completable.complete())
    }

    private fun mockTransactionResult(
            characteristicUuid: UUID = ReceiveCharacteristic.uuid,
            responseNeeded: Boolean = true,
            data: ByteArray? = byteArrayOf(0x01)
    ): TransactionResult {
        return mock {
            on { getRequestId() } doReturn 1
            on { getCharacteristicUuid() } doReturn characteristicUuid
            on { isResponseRequired() } doReturn responseNeeded
            on { getData() } doReturn data
        }

    }
}
