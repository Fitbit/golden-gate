// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx.client

import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothGattService
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus.FAILURE
import com.fitbit.bluetooth.fbgatt.TransactionResult.TransactionResultStatus.SUCCESS
import com.fitbit.bluetooth.fbgatt.rx.GattTransactionException
import com.fitbit.bluetooth.fbgatt.rx.mockBluetoothGatt
import com.fitbit.bluetooth.fbgatt.rx.mockConnected
import com.fitbit.bluetooth.fbgatt.rx.mockFitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.rx.mockGattCharacteristic
import com.fitbit.bluetooth.fbgatt.rx.mockGattConnection
import com.fitbit.bluetooth.fbgatt.rx.mockGattDescriptor
import com.fitbit.bluetooth.fbgatt.rx.mockGattService
import com.fitbit.bluetooth.fbgatt.rx.mockGattTransactionCompletion
import com.fitbit.bluetooth.fbgatt.rx.mockTransactionResult
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.observers.TestObserver
import org.junit.Test
import org.junit.runner.RunWith
import org.junit.runners.JUnit4
import kotlin.test.assertEquals

@RunWith(JUnit4::class)
class BitGattPeerTest {

    private val mockTransaction = mock<GattTransaction>()
    private val mockTransactionProvider = mock<ClientTransactionProvider> {
        on { getConnectTransactionFor(any()) } doReturn mockTransaction
        on { getDisconnectTransactionFor(any()) } doReturn mockTransaction
        on { getRequestConnectionIntervalTransactionFor(any(), any()) } doReturn mockTransaction
        on { getRequestMtuTransactionFor(any(), any()) } doReturn mockTransaction
        on { getDiscoverServicesTransactionFor(any()) } doReturn mockTransaction
        on { getReadDescriptorTransactionFor(any(),any()) } doReturn mockTransaction
        on { getSubscribeToGattCharacteristicTransactionFor(any(),any()) } doReturn mockTransaction
        on { getWriteDescriptorTransaction(any(), any()) } doReturn mockTransaction
        on { getUnsubscribeFromGattCharacteristicTransactionFor(any(),any()) } doReturn mockTransaction
    }

    private val peripheral = BitGattPeer(mockGattConnection, mockTransactionProvider)

    @Test
    fun testConnectSuccess() {
        // Given
        mockGattConnection.mockConnected(false)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testSubscriber = TestObserver.create<BluetoothGatt>()
        peripheral.connect().subscribe(testSubscriber)

        // Then
        testSubscriber.assertValueCount(1)
        testSubscriber.assertComplete()
    }

    @Test
    fun testConnectWhenConnectedSuccess() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testSubscriber = TestObserver.create<BluetoothGatt>()
        peripheral.connect().subscribe(testSubscriber)

        // Then
        testSubscriber.assertValueCount(1)
        testSubscriber.assertComplete()
    }

    @Test
    fun testConnectFailure() {
        // Given
        mockGattConnection.mockConnected(false)
        mockTransaction.mockGattTransactionCompletion(FAILURE)

        // When
        val testSubscriber = TestObserver.create<BluetoothGatt>()
        peripheral.connect().subscribe(testSubscriber)

        // Then
        testSubscriber.assertError { it is GattTransactionException }
    }

    @Test
    fun testRequestConnectionPrioritySuccess() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testObserver = TestObserver.create<Boolean>()
        peripheral.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH).subscribe(testObserver)

        // Then
        assertEquals(1, testObserver.valueCount())
        testObserver.assertValueAt(0, true)
        testObserver.assertComplete()
    }

    @Test
    fun testRequestConnectionPriorityFailure() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(FAILURE)

        // When
        val testObserver = TestObserver.create<Boolean>()
        peripheral.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH).subscribe(testObserver)

        // Then
        testObserver.assertError{ it is GattTransactionException }
    }

    @Test
    fun testRequestMtuSuccess() {
        val requestedMtu = 185 // we will request MTU to be changed to this value
        val actualMtu = 100 // This is what the MTU will be changed to

        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)
        whenever(mockTransactionResult.mtu).thenReturn(actualMtu)

        // When
        val testObserver = TestObserver.create<Int>()
        peripheral.requestMtu(requestedMtu).subscribe(testObserver)

        // Then
        assertEquals(1, testObserver.valueCount())
        testObserver.assertValueAt(0, actualMtu)
        testObserver.assertComplete()
    }

    @Test
    fun testRequestMtuFailure() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(FAILURE)

        // When
        val testObserver = TestObserver.create<Int>()
        peripheral.requestMtu(185).subscribe(testObserver)

        // Then
        testObserver.assertValueCount(0)
        testObserver.assertError { it is GattTransactionException }
    }

    @Test
    fun testDiscoverServicesSuccess() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)
        whenever(mockBluetoothGatt.services).thenReturn(listOf(mockGattService))

        // When
        val testObserver = TestObserver.create<List<BluetoothGattService>>()
        peripheral.discoverServices().subscribe(testObserver)

        // Then
        testObserver.assertValueCount(1)
        testObserver.assertComplete()
    }

    @Test
    fun testDiscoverServicesFailure() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(FAILURE)
        whenever(mockBluetoothGatt.services).thenReturn(listOf(mockGattService))

        // When
        val testObserver = TestObserver.create<List<BluetoothGattService>>()
        peripheral.discoverServices().subscribe(testObserver)

        // Then
        testObserver.assertError { it is GattTransactionException }
    }

    @Test
    fun testDiscoverServicesNullGatt() {
        // Given
        val mockGattConnectionNullGatt = mock<GattConnection> {
            on { device } doReturn mockFitbitBluetoothDevice
            on { gatt } doReturn null
        }
        mockGattConnectionNullGatt.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testObserver = TestObserver.create<List<BluetoothGattService>>()
        val peripheral = BitGattPeer(mockGattConnectionNullGatt, mockTransactionProvider)
        peripheral.discoverServices().subscribe(testObserver)

        // Then
        testObserver.assertValueCount(0)
    }

    @Test
    fun testReadDescriptorSuccess(){
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testObserver = TestObserver.create<BluetoothGattDescriptor>()
        peripheral.readDescriptor(mockGattDescriptor).subscribe(testObserver)

        // Then
        testObserver.assertValueCount(1)
        testObserver.assertComplete()
    }

    @Test
    fun testReadDescriptorFailure(){
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(FAILURE)

        // When
        val testObserver = TestObserver.create<BluetoothGattDescriptor>()
        peripheral.readDescriptor(mockGattDescriptor).subscribe(testObserver)

        // Then
        testObserver.assertError{ it is GattTransactionException }
    }

    @Test
    fun testSetupNotificationsSubscribeSuccess() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testObserver = TestObserver.create<BluetoothGattCharacteristic>()
        peripheral.setupNotifications(mockGattCharacteristic, true).subscribe(testObserver)

        // Then
        testObserver.assertValueCount(1)
        testObserver.assertComplete()
    }

    @Test
    fun testSetupIndicationSubscribeSuccess() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testObserver = TestObserver.create<BluetoothGattCharacteristic>()
        peripheral.setupNotifications(mockGattCharacteristic, true, isIndication = true).subscribe(testObserver)

        // Then
        testObserver.assertValueCount(1)
        testObserver.assertComplete()
    }

    @Test
    fun testSetupNotificationSubscribeFailure() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(FAILURE)

        // When
        val testObserver = TestObserver.create<BluetoothGattCharacteristic>()
        peripheral.setupNotifications(mockGattCharacteristic, true).subscribe(testObserver)

        // Then
        testObserver.assertError{ it is GattTransactionException }
    }

    @Test
    fun testSetupNotificationsUnsubscribeSuccess() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(SUCCESS)

        // When
        val testObserver = TestObserver.create<BluetoothGattCharacteristic>()
        peripheral.setupNotifications(mockGattCharacteristic, false).subscribe(testObserver)

        // Then
        testObserver.assertValueCount(1)
        testObserver.assertComplete()
    }

    @Test
    fun testSetupNotificationUnsubscribeFailure() {
        // Given
        mockGattConnection.mockConnected(true)
        mockTransaction.mockGattTransactionCompletion(FAILURE)

        // When
        val testObserver = TestObserver.create<BluetoothGattCharacteristic>()
        peripheral.setupNotifications(mockGattCharacteristic, false).subscribe(testObserver)

        // Then
        testObserver.assertError{ it is GattTransactionException }
    }

}
