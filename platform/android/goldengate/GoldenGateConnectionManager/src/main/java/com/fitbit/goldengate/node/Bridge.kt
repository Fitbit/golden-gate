// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.io.RxSource
import com.fitbit.goldengate.bindings.io.TxSink
import com.fitbit.goldengate.bindings.util.hexString
import io.reactivex.BackpressureStrategy
import io.reactivex.Completable
import io.reactivex.disposables.CompositeDisposable
import timber.log.Timber
import java.io.Closeable

/**
 * Establishes a bidirectional transport bridge between cross platform stack and native/android bluetooth services.
 *
 * Bridge has a TOP of top of DataSink and DataSource that will be used to attach to bottom of cross platform stack
 *
 * It is expected that before creating any Bridge GoldenGate module is initialized ([GoldenGate.init()]) and
 * Gattlink service and GATT listeners are registered (see GattServerListenerRegistrar)
 *
 * Note: There should only be one bridge per Node. It is expected that before a bridge is created
 * all initialization is complete and GATT services and its listeners are setup
 *
 * +-------------------+
 * |                   |
 * |     GG Stack      |
 * |                   |
 * |  Cross Platform   |
 * +----|---------^----+
 *      |         |
 *      |         |
 * +----v---+-+---|----+
 * |TxSink  | |RxSource|
 * +--------+ +--------+
 * |                   |
 * |      Bridge       |
 * +----|---------^----+
 *      |         |
 *      |         |
 *      |         |
 * +----v---+-+---|----+
 * |  Tx    | |   Rx   |
 * +--------+ +--------+
 * |   Gatt Service    |
 * |                   |
 * |    Android BT     |
 * +-------------------+
 *
 */
class Bridge internal constructor(
    private val connection: GattConnection,
    private val txSink: TxSink = TxSink(),
    private val rxSource: RxSource = RxSource(),
    private val dataReceiverProvider: NodeDataReceiverProvider = NodeDataReceiverProvider(),
    private val dataSenderProvider: NodeDataSenderProvider = NodeDataSenderProvider()
) : DataSinkDataSource, Closeable {

    private val disposeBag: CompositeDisposable = CompositeDisposable()

    private val dataReceiver: NodeDataReceiver by lazy { dataReceiverProvider.provide(connection) }
    private val dataSender: NodeDataSender by lazy { dataSenderProvider.provide(connection) }
    private var isBridgeReady: Boolean = false

    /**
     * Starts listening and transmitting data to/from Node.
     *
     * Bridge will not be established until this method is called, this method should be called
     * once link between Hub and Node is establish
     */
    @Synchronized
    fun start() {
        Timber.d("Starting bridge")
        bridgeReceive()
        bridgeTransmit()
        isBridgeReady = true
    }

    @Synchronized
    fun suspend() {
        isBridgeReady = false
        Timber.d("Stop bridge from receiving or sending new data")
    }

    /**
     * This method will listen to the [NodeDataReceiver.receive] which is posted to from the system binder thread.
     * It will then call [RxSource.receiveData] which will switch to the GG thread.
     */
    private fun bridgeReceive() {
        // will only get data over Rx if there a connection with Node
        disposeBag.add(dataReceiver.receive()
            .doOnNext { data -> Timber.d("Received data from connected Node(${connection.device.address}): ${data.hexString()}") }
            .flatMapCompletable { data -> sendToStack(data) }
            .subscribe(
                { Timber.d("Subscribed to GattLink Rx") },
                { Timber.e(it, "Error receiving data from connected Node") }
            ))
    }

    private fun sendToStack(data: ByteArray): Completable {
        return synchronized(this@Bridge) {
            if (isBridgeReady) {
                rxSource.receiveData(data)
            } else {
                Timber.w("Bridge is not initialised, can't receive data")
                Completable.complete()
            }
        }
    }

    /**
     * This method will listen to the [TxSink.dataObservable] which is posted to from the GG thread.
     * It will then call [NodeDataSender.send] which will switch to a dedicated singleton thread.
     * If another implementation of [NodeDataSender] is used, it should switch threads to avoid blocking the GG thread.
     */
    private fun bridgeTransmit() {
        // if there is no connection transmitting data will fail
        disposeBag.add(txSink.dataObservable
            .doOnNext { data -> Timber.d("Got data for transmission to connected Node: ${data.hexString()}") }
            .toFlowable(BackpressureStrategy.BUFFER)
            .doOnNext { data -> Timber.d("Transmitting data to connected Node(${connection.device.address}): ${data.hexString()}") }
            .flatMapCompletable { data -> sendToNode(data) }
            .subscribe(
                { Timber.d("Subscribed to Stack Tx") },
                { Timber.e(it, "Error transmitting data to connected Node(${connection.device.address})") }
            )
        )
    }

    private fun sendToNode(data: ByteArray): Completable {
        return synchronized(this@Bridge) {
            if (isBridgeReady) {
                dataSender.send(data)
            } else {
                Timber.w("Bridge is not initialised, can't send data")
                Completable.complete()
            }
        }
    }

    override fun getAsDataSinkPointer(): Long = txSink.thisPointer

    override fun getAsDataSourcePointer(): Long = rxSource.thisPointer

    @Synchronized
    override fun close() {
        isBridgeReady = false
        disposeBag.dispose()
        rxSource.close()
        txSink.close()
    }

}
