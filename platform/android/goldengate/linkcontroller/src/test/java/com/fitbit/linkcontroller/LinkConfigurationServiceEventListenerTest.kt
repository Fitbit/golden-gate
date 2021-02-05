// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller

import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.TransactionResult
import com.fitbit.bluetooth.fbgatt.rx.CLIENT_CONFIG_UUID
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSender
import com.fitbit.bluetooth.fbgatt.rx.server.GattServerResponseSenderProvider
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionConfigurationCharacteristic
import com.fitbit.linkcontroller.services.configuration.ClientPreferredConnectionModeCharacteristic
import com.fitbit.linkcontroller.services.configuration.GattCharacteristicSubscriptionStatus
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationService
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationServiceEventListener
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionConfiguration
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionMode
import com.fitbit.linkcontroller.services.configuration.gattServiceSubscribedValue
import com.fitbit.linkcontroller.services.configuration.gattServiceUnSubscribedValue
import com.nhaarman.mockitokotlin2.*
import io.reactivex.Completable
import io.reactivex.schedulers.Schedulers
import org.junit.Before
import org.junit.Test
import java.util.UUID

class LinkConfigurationServiceEventListenerTest {

    private val unknownUuid = UUID.fromString("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF")
    private val device1 = mock<BluetoothDevice> {
        on { address } doReturn "1"
    }

    private val mockGattServerConnection = mock<GattServerConnection>()
    private val mockGattServerResponseSender = mock<GattServerResponseSender>()
    private val mockGattServerResponseSenderProvider = mock<GattServerResponseSenderProvider> {
        on { provide(mockGattServerConnection) } doReturn mockGattServerResponseSender
    }
    private val mockLinkControllerProvider = mock<LinkControllerProvider>()

    private val listener = LinkConfigurationServiceEventListener(
        Schedulers.trampoline(),
        mockGattServerResponseSenderProvider
    )

    @Before
    fun setup() {
        listener.linkControllerProvider = mockLinkControllerProvider
        mockGattServerResponse()
    }

    @Test
    fun shouldReceiveSubscriptionEnabledStatusOnListener() {
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceSubscribedValue)
        )

        listener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
            .assertValue { it == GattCharacteristicSubscriptionStatus.ENABLED }

        //Also verify that the characteristic that was not subscribed to is not marked as subscribed.
        listener.getDataObservable(device1, ClientPreferredConnectionModeCharacteristic.uuid)
            .test()
            .assertValue { it == GattCharacteristicSubscriptionStatus.DISABLED }
    }

    @Test
    fun shouldReceiveUnSubscriptionEnabledStatusOnListener() {
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceUnSubscribedValue)
        )

        listener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
            .assertValue { it == GattCharacteristicSubscriptionStatus.DISABLED }
    }

    @Test
    fun shouldIgnoreIfRequestNotForGattlinkService() {
        val tester = listener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(serviceId = unknownUuid)
        )

        tester.assertValueCount(1)
    }

    @Test
    fun shouldIgnoreAndSendFailureIfRequestNotForTransmitCharacteristic() {
        val tester = listener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(characteristicId = unknownUuid)
        )

        tester.assertValueCount(1)
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureIfRequestNotForConfigurationDescriptor() {
        val tester = listener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(descriptorId = unknownUuid)
        )

        tester.assertValueCount(1)
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureResponseIfRequestHasUnknownValue() {
        val unknownValue = byteArrayOf(0x10, 0x00)
        val tester = listener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = unknownValue)
        )
        tester.assertValueCount(1)
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureResponseIfRequestHasNullValue() {
        val tester = listener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = null)
        )

        tester.assertValueCount(1)
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
            result = mockTransactionResult(
                value = gattServiceSubscribedValue,
                responseNeeded = false
            )
        )

        verifyNoResponseSent()
    }

    @Test
    fun shouldNotSendSuccessIfNotRequiredAndUnSubscribed() {
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(
                value = gattServiceUnSubscribedValue,
                responseNeeded = false
            )
        )

        verifyNoResponseSent()
    }

    @Test
    fun shouldReadPreferredConfigurationWhenRequested() {
        doReturn(mockLinkControllerResult()).whenever(mockLinkControllerProvider)
            .getLinkController(device1)
        listener.onServerCharacteristicReadRequest(
            device1,
            mockTransactionResult(),
            mockGattServerConnection
        )
        verifyResponseSent(
            mockLinkControllerProvider.getLinkController(device1)!!
                .getPreferredConnectionConfiguration().toByteArray()
        )
    }

    @Test
    fun shouldReadPreferredConnectionModeWhenRequested() {
        doReturn(mockLinkControllerResult()).whenever(mockLinkControllerProvider)
            .getLinkController(device1)
        listener.onServerCharacteristicReadRequest(
            device1,
            mockTransactionResult(characteristicId = ClientPreferredConnectionModeCharacteristic.uuid),
            mockGattServerConnection
        )
        verifyResponseSent(
            mockLinkControllerProvider.getLinkController(device1)!!.getPreferredConnectionMode()
                .toByteArray()
        )
    }

    @Test
    fun shouldRespondCorrectlyToDescriptorRead() {

        //First, read before ever subscribing. Should return the unsubscribed value.
        listener.onServerDescriptorReadRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult()
        )

        verifyResponseSent(gattServiceUnSubscribedValue)

        //Fake a subscribe
        listener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceSubscribedValue)
        )

        //Send another read request and verify that it is the subscribed value.
        listener.onServerDescriptorReadRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult()
        )

        verifyResponseSent(gattServiceSubscribedValue)
    }

    private fun verifyResponseSent(byteArray: ByteArray) {
        verify(mockGattServerResponseSender).send(
            device = FitbitBluetoothDevice(device1),
            requestId = 1,
            status = BluetoothGatt.GATT_SUCCESS,
            value = byteArray
        )
    }

    private fun verifyNoResponseSent() {
        verify(mockGattServerResponseSender, never()).send(
            any(),
            any(),
            any(),
            any(),
            any(),
            anyOrNull()
        )
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
        serviceId: UUID = LinkConfigurationService.uuid,
        characteristicId: UUID = ClientPreferredConnectionConfigurationCharacteristic.uuid,
        descriptorId: UUID = CLIENT_CONFIG_UUID,
        responseNeeded: Boolean = true,
        value: ByteArray? = gattServiceSubscribedValue
    ): TransactionResult {
        return mock {
            on { requestId } doReturn 1
            on { serviceUuid } doReturn serviceId
            on { characteristicUuid } doReturn characteristicId
            on { descriptorUuid } doReturn descriptorId
            on { isResponseRequired } doReturn responseNeeded
            on { data } doReturn value
        }
    }

    private fun mockLinkControllerResult(
        preferredConnectionConfiguration: PreferredConnectionConfiguration = PreferredConnectionConfiguration.Builder()
            .build(),
        preferredConnectionMode: PreferredConnectionMode = PreferredConnectionMode.SLOW
    ): LinkController {
        val mockLinkController = mock<LinkController>()
        doReturn(preferredConnectionConfiguration).whenever(mockLinkController)
            .getPreferredConnectionConfiguration()
        doReturn(preferredConnectionMode).whenever(mockLinkController).getPreferredConnectionMode()
        return mockLinkController
    }
}
