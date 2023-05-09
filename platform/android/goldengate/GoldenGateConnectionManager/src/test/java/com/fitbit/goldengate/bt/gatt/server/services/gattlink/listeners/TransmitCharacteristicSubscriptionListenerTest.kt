// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bt.gatt.server.services.gattlink.listeners

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.CLIENT_CONFIG_UUID
import com.fitbit.bluetooth.fbgatt.rx.GattCharacteristicSubscriptionStatus
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSender
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSenderProvider
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.TransmitCharacteristic
import org.mockito.kotlin.any
import org.mockito.kotlin.anyOrNull
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.never
import org.mockito.kotlin.verify
import org.mockito.kotlin.whenever
import io.reactivex.Completable
import io.reactivex.schedulers.Schedulers
import org.junit.Before
import org.junit.Test
import java.util.UUID

class TransmitCharacteristicSubscriptionListenerTest {

    private val unknownUuid = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF")
    private val device1 = mock<BluetoothDevice> {
        on { address } doReturn "1"
    }

    private val mockGattServerConnection = mock<GattServerConnection>()
    private val mockGattServerResponseSender = mock<GattServerResponseSender>()
    private val mockGattServerResponseSenderProvider = mock<GattServerResponseSenderProvider> {
        on { provide(mockGattServerConnection) } doReturn mockGattServerResponseSender
    }

    private val listener = TransmitCharacteristicSubscriptionListener(
            Schedulers.trampoline(),
            mockGattServerResponseSenderProvider
    )

    @Before
    fun setup() {
        mockGattServerResponse()
    }

    @Test
    fun shouldReceiveSubscriptionEnabledStatusOnListener() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = gattServiceSubscribedValue)
        )

        listener.getDataObservable(device1)
                .test()
                .assertValue { it == GattCharacteristicSubscriptionStatus.ENABLED }
    }

    @Test
    fun shouldReceiveUnSubscriptionEnabledStatusOnListener() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = gattServiceUnSubscribedValue)
        )

        listener.getDataObservable(device1)
                .test()
                .assertValue { it == GattCharacteristicSubscriptionStatus.DISABLED }
    }

    @Test
    fun shouldIgnoreIfRequestNotForGattlinkService() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(serviceId = unknownUuid)
        )

        listener.getDataObservable(device1)
                .test()
                .assertNoValues()
    }

    @Test
    fun shouldIgnoreAndSendFailureIfRequestNotForTransmitCharacteristic() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(characteristicId = unknownUuid)
        )

        listener.getDataObservable(device1)
                .test()
                .assertNoValues()
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureIfRequestNotForConfigurationDescriptor() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(descriptorId = unknownUuid)
        )

        listener.getDataObservable(device1)
                .test()
                .assertNoValues()
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureResponseIfRequestHasUnknownValue() {
        val unknownValue = byteArrayOf(0x10, 0x00)
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = unknownValue)
        )

        listener.getDataObservable(device1)
                .test()
                .assertNoValues()
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureResponseIfRequestHasNullValue() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = null)
        )

        listener.getDataObservable(device1)
                .test()
                .assertNoValues()
        verifyFailureSent()
    }

    @Test
    fun shouldSendSuccessIfRequiredAndSubscribed() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = gattServiceSubscribedValue)
        )

        verifySuccessSent()
    }

    @Test
    fun shouldSendSuccessIfRequiredAndUnSubscribed() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = gattServiceUnSubscribedValue)
        )

        verifySuccessSent()
    }

    @Test
    fun shouldNotSendSuccessIfNotRequiredAndSubscribed() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = gattServiceSubscribedValue, responseNeeded = false)
        )

        verifyNoResponseSent()
    }

    @Test
    fun shouldNotSendSuccessIfNotRequiredAndUnSubscribed() {
        listener.onServerDescriptorWriteRequest(
                device = device1,
                connection = mockGattServerConnection,
                result = mockTransactionResult(value = gattServiceUnSubscribedValue, responseNeeded = false)
        )

        verifyNoResponseSent()
    }

    private fun verifyNoResponseSent() {
        verify(mockGattServerResponseSender, never()).send(any(), any(), any(), any(), any(), anyOrNull())
    }

    private fun verifyFailureSent() {
        verify(mockGattServerResponseSender).send(
                device = FitbitBluetoothDevice(device1),
                requestId = 1,
                status = BluetoothGatt.GATT_FAILURE
        )
    }

    private fun verifySuccessSent() {
        verify(mockGattServerResponseSender).send(
                device = FitbitBluetoothDevice(device1),
                requestId = 1,
                status = BluetoothGatt.GATT_SUCCESS
        )
    }

    private fun mockGattServerResponse() {
        whenever(mockGattServerResponseSender.send(any(), any(), any(), any(), any(), anyOrNull()))
                .thenReturn(Completable.complete())
    }

    private fun mockTransactionResult(
            serviceId: UUID = GattlinkService.uuid,
            characteristicId: UUID = TransmitCharacteristic.uuid,
            descriptorId: UUID = CLIENT_CONFIG_UUID,
            responseNeeded: Boolean = true,
            value: ByteArray? = gattServiceSubscribedValue
    ): TransactionResult {
        return mock {
            on { requestId } doReturn 1
            on { serviceUuid } doReturn serviceId
            on { characteristicUuid} doReturn characteristicId
            on { descriptorUuid} doReturn descriptorId
            on { isResponseRequired } doReturn responseNeeded
            on { data } doReturn value
        }
    }
}
