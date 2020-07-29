package com.fitbit.goldengate.bindings.sinksourceadapter

import com.fitbit.goldengate.bindings.BaseTest
import com.fitbit.goldengate.bindings.io.RxSource
import com.fitbit.goldengate.bindings.io.TxSink
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

import org.junit.Assert.*
import org.junit.Test

class SinkSourceAdapterTest: BaseTest()  {

    private val data = byteArrayOf(0x01)

    // transmit mock
    private val transmitSubject = PublishSubject.create<ByteArray>()
    private val mockTxSink = mock<TxSink> {
        on { thisPointer } doReturn 1
        on { dataObservable } doReturn transmitSubject
    }
    private val mockDataSender: SinkSourceAdapterDataSender = mock {
        on { putData(com.nhaarman.mockitokotlin2.any()) } doReturn Completable.complete()
    }
    private val mockDataSenderProvider: SinkSourceAdapterSendProvider = mock {
        on { provide() } doReturn mockDataSender
    }

    // receive mock
    private val receiveSubject = PublishSubject.create<ByteArray>()
    private val mockRxSource = mock<RxSource> {
        on { thisPointer } doReturn 2
    }
    private val mockDataReceiver: SinkSourceAdapterDataReceiver = mock {
        on { receive() } doReturn receiveSubject
    }
    private val mockDataReceiverProvider: SinkSourceAdapterReceiveProvider = mock {
        on { provide() } doReturn mockDataReceiver
    }

    private val sinksourceadapter = SinkSourceAdapter(
        txSink = mockTxSink,
        rxSource = mockRxSource,
        dataReceiveProvider = mockDataReceiverProvider,
        dataSenderProvider = mockDataSenderProvider
    )

    @Before
    fun setUp() {
        RxJavaPlugins.setIoSchedulerHandler { Schedulers.trampoline() }
        sinksourceadapter.start()
    }

    @After
    fun tearDown() {
        sinksourceadapter.close()
    }

    @Test
    fun shouldCreateAdapterWithSinkAndSource() {
        kotlin.test.assertTrue(sinksourceadapter.getAsDataSinkPointer() > 0)
        kotlin.test.assertTrue(sinksourceadapter.getAsDataSourcePointer() > 0)
    }

    @Test
    fun shouldForwardDataReceivedOnReceiveCharacteristicToRxSource() {

        receiveSubject.onNext(data)

        verify(mockRxSource).receiveData(data)
    }

    @Test
    fun shouldForwardDataReceivedOnTxSinkToTransmitCharacteristic() {
        transmitSubject.onNext(data)

        verify(mockDataSender).putData(data)
    }

    @Test
    fun shouldNotForwardDataReceivedIfBridgeIsClosed(){
        sinksourceadapter.close()
        receiveSubject.onNext(data)

        verify(mockRxSource).close()
        verify(mockTxSink).close()
        verify(mockRxSource, times(0)).receiveData(data)
    }

    @Test
    fun shouldNotForwardDataReceivedIfBridgeIsNotInitialised(){
        sinksourceadapter.close()
        receiveSubject.onNext(data)

        verify(mockRxSource,times(0)).receiveData(data)
    }
}
