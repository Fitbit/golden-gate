// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.goldengate.bindings.io.RxSource
import com.fitbit.goldengate.bindings.io.TxSink
import com.fitbit.goldengate.bt.mockGattConnection
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.times
import com.nhaarman.mockitokotlin2.verify
import io.reactivex.Completable
import io.reactivex.plugins.RxJavaPlugins
import io.reactivex.schedulers.Schedulers
import io.reactivex.subjects.PublishSubject
import org.junit.After
import org.junit.Before
import org.junit.Test
import kotlin.test.assertTrue

class BridgeTest {

    private val data = byteArrayOf(0x01)

    // transmit mock
    private val transmitSubject = PublishSubject.create<ByteArray>()
    private val mockTxSink = mock<TxSink> {
        on { thisPointer } doReturn 1
        on { dataObservable } doReturn transmitSubject
    }
    private val mockDataSender: NodeDataSender = mock {
        on { send(any()) } doReturn Completable.complete()
    }
    private val mockDataSenderProvider: NodeDataSenderProvider = mock {
        on { provide(mockGattConnection) } doReturn mockDataSender
    }

    // receive mock
    private val receiveSubject = PublishSubject.create<ByteArray>()
    private val mockRxSource = mock<RxSource> {
        on { thisPointer } doReturn 2
    }
    private val mockDataReceiver: NodeDataReceiver = mock {
        on { receive() } doReturn receiveSubject
    }
    private val mockDataReceiverProvider: NodeDataReceiverProvider = mock {
        on { provide(mockGattConnection) } doReturn mockDataReceiver
    }

    private val bridge = Bridge(
            connection = mockGattConnection,
            txSink = mockTxSink,
            rxSource = mockRxSource,
            dataReceiverProvider = mockDataReceiverProvider,
            dataSenderProvider = mockDataSenderProvider
    )

    @Before
    fun setup() {
        RxJavaPlugins.setIoSchedulerHandler { Schedulers.trampoline() }
        bridge.start()
    }

    @After
    fun tearDown() {
        bridge.close()
    }

    @Test
    fun shouldCreateBridgeWithSinkAndSource() {
        assertTrue(bridge.getAsDataSinkPointer() > 0)
        assertTrue(bridge.getAsDataSourcePointer() > 0)
    }

    @Test
    fun shouldForwardDataReceivedOnReceiveCharacteristicToRxSource() {

        receiveSubject.onNext(data)

        verify(mockRxSource).receiveData(data)
    }

    @Test
    fun shouldForwardDataReceivedOnTxSinkToTransmitCharacteristic() {
        transmitSubject.onNext(data)

        verify(mockDataSender).send(data)
    }

    @Test
    fun shouldNotForwardDataReceivedIfBridgeIsClosed(){
        bridge.close()
        receiveSubject.onNext(data)

        verify(mockRxSource).close()
        verify(mockTxSink).close()
        verify(mockRxSource,times(0)).receiveData(data)
    }

    @Test
    fun shouldNotForwardDataReceivedIfBridgeIsNotInitialised(){
        bridge.close()
        receiveSubject.onNext(data)

        verify(mockRxSource,times(0)).receiveData(data)
    }
}
