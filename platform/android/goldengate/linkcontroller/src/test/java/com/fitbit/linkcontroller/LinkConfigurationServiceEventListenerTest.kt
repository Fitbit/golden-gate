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
import com.fitbit.linkcontroller.services.configuration.LinkConfigurationPeerRequestListener
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

    private val mockPeerRequestListener1 = mock<LinkConfigurationPeerRequestListener>()
    private val mockPeerRequestListener2 = mock<LinkConfigurationPeerRequestListener>()

    private val mockLinkController = mock<LinkController>{
        on { getLinkConfigurationPeerRequestListeners() } doReturn arrayListOf(mockPeerRequestListener1, mockPeerRequestListener2)
        on { getPreferredConnectionConfiguration() } doReturn PreferredConnectionConfiguration.Builder().build()
        on { getPreferredConnectionMode() } doReturn PreferredConnectionMode.SLOW
    }
    private val mockLinkControllerProvider = mock<LinkControllerProvider> {
        on { getLinkController(device = any()) } doReturn mockLinkController
    }

    private val serviceEventListener = LinkConfigurationServiceEventListener(
        Schedulers.trampoline(),
        mockGattServerResponseSenderProvider,
    )

    @Before
    fun setup() {
        serviceEventListener.linkControllerProvider = mockLinkControllerProvider
        mockGattServerResponse()
    }

    @Test
    fun shouldReceiveSubscriptionEnabledStatusOnListener() {
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceSubscribedValue)
        )

        serviceEventListener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
            .assertValue { it == GattCharacteristicSubscriptionStatus.ENABLED }

        //Also verify that the characteristic that was not subscribed to is not marked as subscribed.
        serviceEventListener.getDataObservable(device1, ClientPreferredConnectionModeCharacteristic.uuid)
            .test()
            .assertValue { it == GattCharacteristicSubscriptionStatus.DISABLED }
    }

    @Test
    fun shouldReceiveUnSubscriptionEnabledStatusOnListener() {
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceUnSubscribedValue)
        )

        serviceEventListener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
            .assertValue { it == GattCharacteristicSubscriptionStatus.DISABLED }
    }

    @Test
    fun shouldIgnoreIfRequestNotForGattlinkService() {
        val tester = serviceEventListener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(serviceId = unknownUuid)
        )

        tester.assertValueCount(1)
    }

    @Test
    fun shouldIgnoreAndSendFailureIfRequestNotForTransmitCharacteristic() {
        val tester = serviceEventListener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(characteristicId = unknownUuid)
        )

        tester.assertValueCount(1)
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureIfRequestNotForConfigurationDescriptor() {
        val tester = serviceEventListener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        serviceEventListener.onServerDescriptorWriteRequest(
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
        val tester = serviceEventListener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = unknownValue)
        )
        tester.assertValueCount(1)
        verifyFailureSent()
    }

    @Test
    fun shouldIgnoreAndSendFailureResponseIfRequestHasNullValue() {
        val tester = serviceEventListener.getDataObservable(
            device1,
            ClientPreferredConnectionConfigurationCharacteristic.uuid
        )
            .test()
        tester.assertValueCount(1)
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = null)
        )

        tester.assertValueCount(1)
        verifyFailureSent()
    }

    @Test
    fun shouldSendSuccessIfRequiredAndSubscribed() {
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceSubscribedValue)
        )

        verifySuccessSent()
    }

    @Test
    fun shouldSendSuccessIfRequiredAndUnSubscribed() {
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceUnSubscribedValue)
        )

        verifySuccessSent()
    }

    @Test
    fun shouldNotSendSuccessIfNotRequiredAndSubscribed() {
        serviceEventListener.onServerDescriptorWriteRequest(
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
        serviceEventListener.onServerDescriptorWriteRequest(
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
        serviceEventListener.onServerCharacteristicReadRequest(
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
        serviceEventListener.onServerCharacteristicReadRequest(
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
    fun shouldCallListenersWhenReceivingReadRequest() {
        serviceEventListener.onServerCharacteristicReadRequest(
            device1,
            mockTransactionResult(),
            mockGattServerConnection
        )
        verify(mockPeerRequestListener1).onPeerReadRequest()
        verify(mockPeerRequestListener2).onPeerReadRequest()

    }

    @Test
    fun shouldRespondCorrectlyToDescriptorRead() {

        //First, read before ever subscribing. Should return the unsubscribed value.
        serviceEventListener.onServerDescriptorReadRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult()
        )

        verifyResponseSent(gattServiceUnSubscribedValue)

        //Fake a subscribe
        serviceEventListener.onServerDescriptorWriteRequest(
            device = device1,
            connection = mockGattServerConnection,
            result = mockTransactionResult(value = gattServiceSubscribedValue)
        )

        //Send another read request and verify that it is the subscribed value.
        serviceEventListener.onServerDescriptorReadRequest(
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
}
