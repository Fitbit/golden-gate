package com.fitbit.goldengate.bindings.sinksourceadapter

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.io.RxSource
import com.fitbit.goldengate.bindings.io.TxSink
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bindings.util.hexString
import io.reactivex.BackpressureStrategy
import io.reactivex.Completable
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.io.Closeable

/**
 * SinkSourceAdaptor is a bridge that transfers data back and forth between
 * a) the Stack and CoapRouter on the Service App
 * b) the CoapEndPoint and Binder Interface on the Client App
 *
 * It is expected that before creating any SinkSourceAdapter GoldenGate module is initialized ([GoldenGate.init()])
 *
 *   Service App                        Client App
 * +-------------------+            +-------------------+
 * |   CoapRouter      |            |                   |
 * |                   |            |                   |
 * |                   |            |   CoapEndPoint    |
 * +-------------------+            |                   |
 * |  Tx    | |   Rx   |            |                   |
 * +----^---+ +---|----+            +----|---+ +---^----+
 *      |         |                      |         |
 *      |         |                      |         |
 *      |         |                      |         |
 * +--------+ +---v----+            +----v---+ +---|----+
 * |                   |            |TxSink  | |RxSource|
 * | SinkSourceAdapter |            +-------------------+
 * +-------------------+            | SinkSourceAdapter |
 * |TxSink  | |RxSource|            |                   |
 * +----^---+ +---|----+            +----^---+ +---^----+
 *      |         |                      |         |
 *      |         |                      |         |
 * +----|---------v----+            +----v---------|---+
 * |                   |            |  Tx    | |   Rx   |
 * |     GG Stack      |            +-------------------+
 * |                   |            |  Binder Interface |
 * |  Cross Platform   |            |                   |
 * +-------------------+            +-------------------+
 *
 */
class SinkSourceAdapter(
    private val txSink: TxSink = TxSink(),
    private val rxSource: RxSource = RxSource(),
    private val dataSenderProvider: SinkSourceAdapterSendProvider,
    private val dataReceiveProvider: SinkSourceAdapterReceiveProvider

) : StackService, DataSinkDataSource, Closeable {

    private val disposeBag: CompositeDisposable = CompositeDisposable()
    internal var dataSinkDataSource: DataSinkDataSource? = null

    private val dataSender: SinkSourceAdapterDataSender by lazy { dataSenderProvider.provide() }
    private val dataReceiver: SinkSourceAdapterDataReceiver by lazy { dataReceiveProvider.provide() }

    private var isSinkSourceAdapterReady: Boolean = false


    /**
     * Starts listening and transmitting data to/from Stack/CoapEndPoint.
     *
     * SinkSourceAdapter will not be established until this method is called, this method should be called
     * once link between Hub and Node is establish
     */
    @Synchronized
    fun start() {
        Timber.d("Starting sinkSourceAdapter")
        sinkSourceAdapterReceive()
        sinkSourceAdapterTransmit()
        isSinkSourceAdapterReady = true
    }

    @Synchronized
    fun suspend() {
        isSinkSourceAdapterReady = false
        Timber.d("Stop adapter from receiving or sending new data")
    }

    override fun attach(dataSinkDataSource: DataSinkDataSource) {
        if (this.dataSinkDataSource == null) {
            this.dataSinkDataSource = dataSinkDataSource
            attach(
                sourcePtr = dataSinkDataSource.getAsDataSourcePointer(),
                sinkPtr = dataSinkDataSource.getAsDataSinkPointer()
            )
        } else {
            throw RuntimeException("Please detach before re-attaching")
        }
    }

    override fun detach() {
        this.dataSinkDataSource?.let {
            detach(
                sourcePtr = this.dataSinkDataSource!!.getAsDataSourcePointer()
            )
            this.dataSinkDataSource = null
        }
    }

    /**
     * This method will listen to the [SinkSourceAdapterDataReceiver.observe] which is posted to from the system binder thread.
     * It will then call [RxSource.receiveData] which will switch to the GG thread.
     */
    private fun sinkSourceAdapterReceive() {
        // will only get data over Rx if there a connection with Node
        disposeBag.add(dataReceiver.observe()
            .doOnNext { data -> Timber.d("Received data SinkSourceAdapter ${data.hexString()}") }
            .flatMapCompletable { data -> sendToJNI(data) }
            .subscribeOn(Schedulers.io())
            .subscribe(
                { Timber.d("Subscribed to JNI Rx") },
                { Timber.e(it, "Error receiving data") }
            ))
    }

    private fun sendToJNI(data: ByteArray): Completable {
        return synchronized(this@SinkSourceAdapter) {
            if (isSinkSourceAdapterReady) {
                rxSource.receiveData(data)
            } else {
                Timber.w("SinkSource is not initialised, can't receive data")
                Completable.complete()
            }
        }
    }

    /**
     * This method will listen to the [TxSink.dataObservable] which is posted to from the GG thread.
     * TODO: look out for flatMapCompletable reordering packets
     */
    private fun sinkSourceAdapterTransmit() {
        // if there is no connection transmitting data will fail
        disposeBag.add(txSink.dataObservable
            .doOnNext { data -> Timber.d("Data to transmit from SinkSourceAdapter: ${data.hexString()}") }
            .toFlowable(BackpressureStrategy.BUFFER)
            .doOnNext { data -> Timber.d("Transmitting data from SinkSourceAdapter") }
            .flatMapCompletable { data -> sendToJVM(data) }
            .subscribeOn(Schedulers.io())
            .subscribe(
                { Timber.d("Subscribed to Stack Tx") },
                { Timber.e(it, "Error transmitting data to connected Router()") }
            )
        )
    }

    private fun sendToJVM(data: ByteArray): Completable {
        return synchronized(this@SinkSourceAdapter) {
            if (isSinkSourceAdapterReady) {
                dataSender.send(data)
            } else {
                Timber.w("SinkSource is not initialised, can't send data")
                Completable.complete()
            }
        }
    }


    override fun getAsDataSinkPointer(): Long = txSink.thisPointer

    override fun getAsDataSourcePointer(): Long = rxSource.thisPointer

    @Synchronized
    override fun close() {
        isSinkSourceAdapterReady = false
        disposeBag.dispose()
        detach()
        rxSource.close()
        txSink.close()

    }

    private external fun attach(
        rxSourcePtr: Long = getAsDataSourcePointer(),
        txSinkPtr: Long = getAsDataSinkPointer(),
        sourcePtr: Long,
        sinkPtr: Long
    )

    private external fun detach(
        rxSourcePtr: Long = getAsDataSourcePointer(),
        sourcePtr: Long
    )
}
