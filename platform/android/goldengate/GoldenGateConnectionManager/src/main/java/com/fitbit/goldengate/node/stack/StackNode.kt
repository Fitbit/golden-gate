// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node.stack

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionChangeListener
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.bluetooth.fbgatt.rx.PeripheralDisconnector
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerMtuChangeListener
import com.fitbit.goldengate.bindings.coap.CoapGroupRequestFilterMode
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus.TlsProtocolState.TLS_STATE_ERROR
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus.TlsProtocolState.TLS_STATE_SESSION
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackEvent
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bt.PeripheralConnector
import com.fitbit.goldengate.node.Bridge
import com.fitbit.goldengate.node.LinkupHandler
import com.fitbit.goldengate.node.MtuChangeRequester
import com.fitbit.goldengate.node.Node
import com.fitbit.goldengate.node.NodeConnectionStatus
import com.fitbit.goldengate.node.StackEventHandler
import com.fitbit.goldengate.peripheral.NodeDisconnectedException
import io.reactivex.Completable
import io.reactivex.Observable
import io.reactivex.Single
import io.reactivex.disposables.Disposable
import io.reactivex.functions.BiFunction
import io.reactivex.functions.Function
import io.reactivex.schedulers.Schedulers
import io.reactivex.subjects.PublishSubject
import timber.log.Timber
import java.util.concurrent.TimeUnit

const val STACK_NODE_DEFAULT_MTU = 185
const val STACK_NODE_CONNECTION_TIMEOUT_SECONDS: Long = 60

/**
 * A [Node] that encapsulates a connection via a [Stack] including the [StackConfig],
 * [StackService], [Stack], and [Bridge]
 *
 * @param key the [BluetoothAddressNodeKey] to use for this [StackNode]
 * @param stackConfig the [StackConfig] of the [Stack]
 * @param stackService the [StackService] at the top of the [Stack]
 * @param linkupHandler the [LinkupHandler] used to link up the stack to golden gate stack services
 * @param peripheralConnector the [PeripheralConnector] used to connect the stack to the gatt layer
 * @param connectionStatusProvider provides an Observable stream which emits [PeripheralConnectionStatus] updates when provided a [GattConnection]
 * @param dtlsEventObservableProvider provides an Observable stream which emits [DtlsProtocolStatus] updates when provided a [Stack]
 * @param peripheralProvider a function which returns a [BitGattPeripheral] for a given [GattConnection]
 * @param mtuChangeRequesterProvider provides and [MtuChangeRequester] when provided an [Int] and a [Stack]
 * @param startMtu the default mtu used when connecting
 * @param connectTimeout timeout in [TimeUnit.SECONDS] when connecting
 * @param buildStack builds a [Stack] when provided a [Bridge]
 * @param buildBridge builds a [Bridge] when provided a [BluetoothDevice]
 * @param mtuRequestSubject used to inform an active connection of a desired mtu
 * @param stackEventObservableProvider provides an Observable stream which emits [StackEvent] updates when provided a [Stack]
 * @param stackEventHandlerProvider provides a [StackEvent] dispatcher for a given [Stack] and [GattConnection]
 */
class StackNode<T: StackService>internal constructor(
        val key: BluetoothAddressNodeKey,
        val stackConfig: StackConfig,
        stackService: T,
        private val linkupHandler: LinkupHandler = LinkupHandler(),
        private val peripheralConnector: PeripheralConnector = PeripheralConnector(key.value),
        private val connectionStatusProvider: (GattConnection) -> Observable<PeripheralConnectionStatus> = { PeripheralConnectionChangeListener().register(it) },
        private val dtlsEventObservableProvider: (stack: Stack) -> Observable<DtlsProtocolStatus> = { it.dtlsEventObservable },
        private val peripheralProvider: (gattConnection: GattConnection) -> BitGattPeripheral = { gattConnection -> BitGattPeripheral(gattConnection) },
        private val mtuChangeRequesterProvider: (Stack) -> MtuChangeRequester = { stack -> MtuChangeRequester(key.value, stack) },
        private val startMtu: Int = STACK_NODE_DEFAULT_MTU,
        private val connectTimeout: Long = STACK_NODE_CONNECTION_TIMEOUT_SECONDS,
        private val buildStack: (Bridge) -> Stack = {
            Stack(
                key,
                it.getAsDataSinkPointer(),
                it.getAsDataSourcePointer(),
                stackConfig
            )
        },
        private val buildBridge: (GattConnection) -> Bridge = { Bridge(it) },
        private val mtuRequestSubject: PublishSubject<Int> = PublishSubject.create(),
        private val gattServerMtuChangeListener: GattServerMtuChangeListener = GattServerMtuChangeListener(),
        private val stackEventHandlerProvider: (Observable<StackEvent>, GattConnection) -> StackEventHandler = { obs, conn -> StackEventHandler(obs, conn) },
        private val stackEventObservableProvider: (Stack) -> Observable<StackEvent> = { it.stackEventObservable },
        fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
        private val peripheralDisconnector: PeripheralDisconnector = PeripheralDisconnector(fitbitGatt)
): Node<T>(stackService) {

    /**
     * Utility constructor for injecting [connectionStatusProvider] and [dtlsEventObservableProvider]
     * used for status messages in the Host app
     */
    constructor(
            key: BluetoothAddressNodeKey,
            stackConfig: StackConfig,
            stackService: T,
            connectionStatusProvider: (GattConnection) -> Observable<PeripheralConnectionStatus>,
            dtlsEventObservableProvider: (stack: Stack) -> Observable<DtlsProtocolStatus>
    ): this(
            key,
            stackConfig,
            stackService,
            LinkupHandler(),
            PeripheralConnector(key.value),
            connectionStatusProvider,
            dtlsEventObservableProvider
    )

    private var nodeConnectionObservable: Observable<NodeConnectionStatus>? = null
    private var disposable: Disposable? = null

    @Synchronized
    override fun connection(): Observable<NodeConnectionStatus> =
            nodeConnectionObservable?.also {
                Timber.i("StackNode with key $key has a connection")
            }?: connect().also {
                Timber.i("StackNode with key $key does not have a connection")
            }

    /**
     * Requests an mtu change
     */
    @Synchronized
    fun requestMtu(mtu: Int) = mtuRequestSubject.onNext(mtu)

    @Synchronized
    override fun disconnect() {
        Timber.i("StackNode with key $key connection is disconnecting")
        disposable?.dispose()
        disconnectPeripheral()
    }

    private fun connect(): Observable<NodeConnectionStatus> {
        Timber.i("StackNode with key $key connection is creating a new connection")
        //Create an observable that can complete when disposed. Allows disconnect to trigger onComplete
        return Observable.create<NodeConnectionStatus> { emitter ->
            Timber.i("StackNode with key $key connection is starting the connection")
            //1. Connect the device
            peripheralConnector.connect().flatMapObservable {
                //2. Build the stack, bridge, and attach the service
                buildStack(it)
            }.flatMapSingle { state ->
                //3. Update MTU
                mtuChangeRequesterProvider(state.stack)
                    .requestMtu(state.peripheral, startMtu)
                    .map { state }
            }.flatMapSingle { state ->
                //4. Subscribe to GG services
                Timber.i("StackNode with key $key subscribing to Gattlink service")
                linkupHandler.link(state.peripheral).andThen(Single.just(state))
            }.flatMap { state ->
                //5. Start stack
                Timber.i("StackNode with key $key starting stack and bridge")
                state.bridge.start()
                state.stack.start()
                //6. Wait for connected events
                waitForConnectedState(state).map { state to it }
            }.flatMap { (state, status) ->
                listenToConnectionMtuChanges(state).startWith(Observable.just(state to status))
            }
            //Once connected, listen to mtu change requests
            .flatMap { (state, status) ->
                listenToMtuChangeRequests(state).startWith(Observable.just(state to status))
            }
            //Listen to stack events and react to it
            .flatMap { (state, status) -> listenToStackEvents(state).startWith(Observable.just(status)) }
            .doOnDispose {
                Timber.i("StackNode with key $key connection is being disposed.")
                emitter.onComplete()
            }.doFinally {
                removeActiveConnection()
            }.subscribe({
                Timber.i("StackNode with key $key connection has updated its status to $it")
                emitter.onNext(it)
            }, {
                Timber.e(it, "StackNode with key $key connection has thrown an error")
                emitter.onError(it)
            }).also {
                Timber.i("StackNode with key $key connection is saving a reference to its disposable to disconnect later")
                disposable = it
            }
        }
        //Use the connection to a single observable for any future subscriptions to avoid race conditions
        .replay()
        .autoConnect()
        .also {
            Timber.i("StackNode with key $key connection is saving a reference to its observable share later")
            nodeConnectionObservable = it
        }
    }

    private fun buildStack(connection: GattConnection): Observable<ConnectionState> {
        return Observable.create<ConnectionState> { stackEmitter ->
            synchronized(this@StackNode) {
                Timber.i("StackNode with key $key building stack and bridge and is attaching it to the service")
                val bridge = buildBridge(connection)
                val stack = buildStack(bridge)
                stackService.attach(stack)
                val peripheral = peripheralProvider(connection)
                val connectionState = ConnectionState(bridge, stack, peripheral, connection)
                stackEmitter.setCancellable { tearDown(connectionState) }
                connectionState
            }.let { stackEmitter.onNext(it) }
        }
    }

    /**
     * Perform MTU change for Stack only when BT connection MTU changes. Connection MTU can be
     * changed by connected Node in which case we need to update the stack MTU
     */
    private fun listenToConnectionMtuChanges(connectionState: ConnectionState): Completable {
        return gattServerMtuChangeListener.register(
            connectionState.gattConnection.device.btDevice
        ).flatMapSingle { mtu ->
            mtuChangeRequesterProvider(connectionState.stack).updateStackMtu(mtu)
        }.ignoreElements()
    }

    /**
     * Perform MTU change for complete connection for request made via [requestMtu] call
     */
    private fun listenToMtuChangeRequests(connectionState: ConnectionState): Completable {
        return mtuRequestSubject.flatMapSingle { mtu ->
            Timber.i("StackNode with key $key received an mtu change request")
            mtuChangeRequesterProvider(connectionState.stack).requestMtu(connectionState.peripheral, mtu)
        }.ignoreElements()
    }

    private fun listenToStackEvents(connectionState: ConnectionState): Completable =
        stackEventHandlerProvider(
            stackEventObservableProvider(connectionState.stack),
            connectionState.gattConnection
        ).dispatchEvents()

    /**
     * Emits [NodeConnectionStatus.CONNECTED] when any relevant event streams all emit
     * [NodeConnectionStatus.CONNECTED]. Only [NodeConnectionStatus.CONNECTED] is the only value
     * emitted from any stream; so when all emit a single value then it will be connected.
     */
    private fun waitForConnectedState(connectionState: ConnectionState): Observable<NodeConnectionStatus> {
        val connectionStatusObservable = getConnectionStatusObservable(connectionState)
        return if (stackConfig is DtlsSocketNetifGattlink) {
            //Gattlink and tls events combined
            Observable.zip(
                    connectionStatusObservable,
                    getDtlsEventObservable(connectionState),
                    BiFunction { _, _ -> NodeConnectionStatus.CONNECTED }
            )
        } else {
            //Just Gattlink events
            connectionStatusObservable
        }.timeout(
            // Timeout for first emission only
            Observable.empty<NodeConnectionStatus>().delay(connectTimeout, TimeUnit.SECONDS),
            Function { _: NodeConnectionStatus -> Observable.never<Any>() }
        )
    }

    /**
     * Only emits [NodeConnectionStatus.CONNECTED] on [TLS_STATE_SESSION] and a
     * [NodeDisconnectedException] if there was a [TLS_STATE_ERROR]
     */
    private fun getDtlsEventObservable(connectionState: ConnectionState): Observable<NodeConnectionStatus> =
            Observable.defer {
                Timber.i("StackNode with key $key listening to tls events")
                dtlsEventObservableProvider(connectionState.stack)
                    .doOnNext { Timber.d("StackNode with key $key Tls status changed to ${it.state}") }
                    .filter { tlsStatus ->
                        /*
                         * Only react to TLS_STATE_SESSION, exclude all other states as they
                         * don't matter when emitting a true connection and there are
                         * retries build in XP library for TLS_STATE_ERROR state
                         */
                        tlsStatus.state == TLS_STATE_SESSION
                    }.map { tlsStatus ->
                        when(tlsStatus.state) {
                            TLS_STATE_SESSION -> {
                                /*
                                 * If DTLS connection is not using the public keys, update the filter group.
                                 */
                                if (tlsStatus.pskIdentity != "BOOTSTRAP" &&
                                    tlsStatus.pskIdentity != "hello") {
                                    stackService.setFilterGroup(CoapGroupRequestFilterMode.GROUP_0, key)
                                }
                                NodeConnectionStatus.CONNECTED
                            }
                            //This will never happen
                            else -> throw IllegalStateException()
                        }
                    }
            }

    /**
     * Only emits [NodeConnectionStatus.CONNECTED] on [PeripheralConnectionStatus.CONNECTED] and
     * [NodeDisconnectedException] if there was a [PeripheralConnectionStatus.DISCONNECTED]
     */
    private fun getConnectionStatusObservable(connectionState: ConnectionState): Observable<NodeConnectionStatus> =
        Observable.defer {
            Timber.i("StackNode with key $key listening to peripheral events")
            Observable.just(PeripheralConnectionStatus.CONNECTED)
                .filter { connectionState.gattConnection.isConnected }
                .concatWith(connectionStatusProvider(connectionState.gattConnection))
                .map { status ->
                    Timber.i("StackNode with key $key received connection change status: $status")
                    when (status) {
                        PeripheralConnectionStatus.CONNECTED -> NodeConnectionStatus.CONNECTED
                        PeripheralConnectionStatus.DISCONNECTED -> throw NodeDisconnectedException()
                    }
                }
        }

    @Synchronized
    private fun removeActiveConnection() {
        Timber.i("StackNode with key $key is removing the active connection")
        disposable = null
        nodeConnectionObservable = null
    }

    @Synchronized
    private fun tearDown(connectionState: ConnectionState) {
        Timber.i("StackNode with key $key is detaching its service and tearing down its stack, bridge")
        connectionState.bridge.suspend()
        stackService.detach()
        stackService.setFilterGroup(CoapGroupRequestFilterMode.GROUP_1, key)
        connectionState.stack.close()
        connectionState.bridge.close()
    }

    @SuppressLint("CheckResult")
    private fun disconnectPeripheral() {
        // This is a fire-and-forget call
        peripheralDisconnector.disconnect(key.value)
                .doOnSubscribe { Timber.d("Disconnecting from peripheral $key") }
                .subscribeOn(Schedulers.io())
                .subscribe(
                        { Timber.d("We disconnected from the device") },
                        { Timber.w(it, "Error disconnecting from device") }
                )
    }

    private data class ConnectionState(val bridge: Bridge, val stack: Stack, val peripheral: BitGattPeripheral, val gattConnection: GattConnection)
}
