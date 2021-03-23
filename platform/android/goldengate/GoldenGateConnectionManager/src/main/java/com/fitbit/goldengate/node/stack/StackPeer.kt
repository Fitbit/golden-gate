// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node.stack

import android.annotation.SuppressLint
import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.MtuUpdateListener
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.bluetooth.fbgatt.rx.PeripheralDisconnector
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.listeners.GattClientConnectionChangeListener
import com.fitbit.bluetooth.fbgatt.rx.getGattConnection
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
import com.fitbit.goldengate.bt.PeerConnector
import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.node.Bridge
import com.fitbit.goldengate.node.Linkup
import com.fitbit.goldengate.node.LinkupHandlerProvider
import com.fitbit.goldengate.node.LinkupWithPeerNodeHandler
import com.fitbit.goldengate.node.MtuChangeRequester
import com.fitbit.goldengate.node.Peer
import com.fitbit.goldengate.node.PeerConnectionStatus
import com.fitbit.goldengate.node.StackEventHandler
import com.fitbit.goldengate.peripheral.NodeDisconnectedException
import com.fitbit.linkcontroller.LinkController
import com.fitbit.linkcontroller.LinkControllerProvider
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCode
import io.reactivex.Completable
import io.reactivex.Observable
import io.reactivex.Single
import io.reactivex.disposables.Disposable
import io.reactivex.functions.BiFunction
import io.reactivex.functions.Function
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.concurrent.TimeUnit

const val STACK_NODE_DEFAULT_MTU = 185
const val STACK_NODE_CONNECTION_TIMEOUT_SECONDS: Long = 60

/**
 * A [Peer] (hub or node) that encapsulates a connection via a [Stack] including the [StackConfig],
 * [StackService], [Stack], and [Bridge]
 *
 * @param key the [BluetoothAddressNodeKey] to use for this [StackPeer]
 * @param stackConfig the [StackConfig] of the [Stack]
 * @param stackService the [StackService] at the top of the [Stack]
 * @param linkupHandler the [LinkupWithPeerNodeHandler] used to link up the stack to golden gate stack services
 * @param peerConnector the [PeerConnector] used to connect the stack to the gatt layer
 * @param connectionStatusProvider provides an Observable stream which emits [PeripheralConnectionStatus] updates when provided a [GattConnection]
 * @param dtlsEventObservableProvider provides an Observable stream which emits [DtlsProtocolStatus] updates when provided a [Stack]
 * @param peerProvider a function which returns a [BitGattPeer] for a given [GattConnection]
 * @param mtuChangeRequesterProvider provides and [MtuChangeRequester] when provided an [Int] and a [Stack]
 * @param startMtu the default mtu used when connecting
 * @param connectTimeout timeout in [TimeUnit.SECONDS] when connecting
 * @param buildStack builds a [Stack] when provided a [Bridge]
 * @param buildBridge builds a [Bridge] when provided a [BluetoothDevice]
 * @param mtuUpdateListenerProvider used to inform an active connection of a desired mtu
 * @param stackEventObservableProvider provides an Observable stream which emits [StackEvent] updates when provided a [Stack]
 * @param stackEventHandlerProvider provides a [StackEvent] dispatcher for a given [Stack] and [GattConnection]
 */
class StackPeer<T: StackService> internal constructor(
    val key: BluetoothAddressNodeKey,
    peerRole: PeerRole,
    val stackConfig: StackConfig,
    stackService: T,
    private val linkupHandler: Linkup = LinkupHandlerProvider.getHandler(peerRole),
    private val peerConnector: PeerConnector = PeerConnector(key.value),
    private val connectionStatusProvider: (GattConnection) -> Observable<PeripheralConnectionStatus> = { GattClientConnectionChangeListener().register(it) },
    private val dtlsEventObservableProvider: (stack: Stack) -> Observable<DtlsProtocolStatus> = { it.dtlsEventObservable },
    private val peerProvider: (gattConnection: GattConnection) -> BitGattPeer = { gattConnection -> BitGattPeer(gattConnection) },
    private val mtuChangeRequesterProvider: (Stack) -> MtuChangeRequester = { stack -> MtuChangeRequester(key.value, stack) },
    private val startMtu: Int = STACK_NODE_DEFAULT_MTU,
    private val connectTimeout: Long = STACK_NODE_CONNECTION_TIMEOUT_SECONDS,
    private val buildStack: (Bridge) -> Stack = {
        Stack(
            key,
            it.getAsDataSinkPointer(),
            it.getAsDataSourcePointer(),
            stackConfig,
            isNode = (peerRole == PeerRole.Central)
        )
    },
    private val buildBridge: (GattConnection) -> Bridge = { Bridge(it) },
    private val mtuUpdateListenerProvider: (GattConnection) -> Observable<Int> = { MtuUpdateListener().register(it) },
    private val stackEventHandlerProvider: (Observable<StackEvent>, GattConnection) -> StackEventHandler = { obs, conn -> StackEventHandler(obs, conn) },
    private val stackEventObservableProvider: (Stack) -> Observable<StackEvent> = { it.stackEventObservable },
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance(),
    private val peripheralDisconnector: PeripheralDisconnector = PeripheralDisconnector(fitbitGatt),
    private val linkControllerProvider: (GattConnection) -> LinkController = { LinkControllerProvider.INSTANCE.getLinkController(it) }
): Peer<T>(stackService, peerRole) {

    /**
     * Utility constructor for injecting [connectionStatusProvider] and [dtlsEventObservableProvider]
     * used for status messages in the Host app
     */
    constructor(
        key: BluetoothAddressNodeKey,
        peerRole: PeerRole,
        stackConfig: StackConfig,
        stackService: T,
        connectionStatusProvider: (GattConnection) -> Observable<PeripheralConnectionStatus>,
        dtlsEventObservableProvider: (stack: Stack) -> Observable<DtlsProtocolStatus>
    ): this(
        key,
        peerRole,
        stackConfig,
        stackService,
        LinkupHandlerProvider.getHandler(peerRole),
        PeerConnector(key.value),
        connectionStatusProvider,
        dtlsEventObservableProvider
    )

    private var peerConnectionObservable: Observable<PeerConnectionStatus>? = null
    private var disposable: Disposable? = null

    @Synchronized
    override fun connection(): Observable<PeerConnectionStatus> =
        peerConnectionObservable?.also {
            Timber.i("StackPeer with key $key has a connection")
        }?: connect().also {
            Timber.i("StackPeer with key $key does not have a connection")
        }

    @Synchronized
    override fun disconnect(notifyPeerToDisconnect: Boolean) {
        Timber.i("StackPeer with key $key connection is disconnecting")
        if (notifyPeerToDisconnect) {
            Timber.i("notify peer ${key.value} to trigger BLE disconnection")
            requestTrackerDisconnect(key.value)
                .toObservable<Nothing>()
                .publish()
                .connect()
        }

        disposable?.dispose()
        disconnectPeripheral()
    }

    private fun connect(): Observable<PeerConnectionStatus> {
        Timber.i("StackPeer with key $key connection is creating a new connection")
        //Create an observable that can complete when disposed. Allows disconnect to trigger onComplete
        return Observable.create<PeerConnectionStatus> { emitter ->
            Timber.i("StackPeer with key $key connection is starting the connection")
            //1. Connect the device
            peerConnector.connect().flatMapObservable {
                //2. Build the stack, bridge, and attach the service
                setupStack(it)
            }.flatMap { state ->
                Timber.i("StackPeer with key $key mtuChangedHandler $state")
                //3. subscribe to MTU update observable
                mtuUpdateHandler(state)
                    .startWith(Observable.just(state))
            }.flatMapSingle { state ->
                //4. Send MTU update request
                if (peerRole == PeerRole.Peripheral) {
                    mtuChangeRequesterProvider(state.stack)
                        .requestMtu(state.peer, startMtu)
                        .map { state }
                } else {
                    Single.just(state)
                }
            }.flatMapSingle { state ->
                //5. start link-up procedure
                Timber.i("StackPeer with key $key start link up handler")
                linkupHandler.link(state.peer).andThen(Single.just(state))
            }.flatMap { state ->
                //6. Start stack
                Timber.i("StackPeer with key $key starting stack and bridge")
                state.bridge.start()
                state.stack.start()
                //7. Wait for connected events
                waitForConnectedState(state).map { state to it }
            }.flatMap { (state, status) ->
                //8. Listen to stack events and react to it
                listenToStackEvents(state).startWith(Observable.just(status))
            }.doOnDispose {
                Timber.i("StackPeer with key $key connection is being disposed.")
                emitter.onComplete()
            }.doFinally {
                removeActiveConnection()
            }.subscribe({
                Timber.i("StackPeer with key $key connection has updated its status to $it")
                emitter.onNext(it)
            }, {
                Timber.e(it, "StackPeer with key $key connection has thrown an error")
                emitter.onError(it)
            }).also {
                Timber.i("StackPeer with key $key connection is saving a reference to its disposable to disconnect later")
                disposable = it
            }
        }
        //Use the connection to a single observable for any future subscriptions to avoid race conditions
        .replay()
        .autoConnect()
        .also {
            Timber.i("StackPeer with key $key connection is saving a reference to its observable share later")
            peerConnectionObservable = it
        }
    }

    private fun setupStack(connection: GattConnection): Observable<ConnectionState> {
        return Observable.create<ConnectionState> { stackEmitter ->
            synchronized(this@StackPeer) {
                Timber.i("StackPeer with key $key building stack and bridge and is attaching it to the service")
                val bridge = buildBridge(connection)
                val stack = buildStack(bridge)
                stackService.attach(stack)
                val peer = peerProvider(connection)
                val connectionState = ConnectionState(bridge, stack, peer, connection)
                stackEmitter.setCancellable { tearDown(connectionState) }
                connectionState
            }.let { stackEmitter.onNext(it) }
        }
    }

    /**
     * Perform MTU change for Stack only when BT connection MTU changes. Connection MTU can be
     * changed by connected peer in which case we need to update the stack MTU
     */
    private fun mtuUpdateHandler(connectionState: ConnectionState): Completable {
        return Observable.defer {
            mtuUpdateListenerProvider(connectionState.gattConnection)
        }.flatMapSingle { mtu ->
            Timber.d("received new mtu: $mtu")
            mtuChangeRequesterProvider(connectionState.stack).updateStackMtu(mtu)
        }.ignoreElements()
    }

    private fun listenToStackEvents(connectionState: ConnectionState): Completable =
        stackEventHandlerProvider(
            stackEventObservableProvider(connectionState.stack),
            connectionState.gattConnection
        ).dispatchEvents()

    /**
     * Emits [PeerConnectionStatus.CONNECTED] when any relevant event streams all emit
     * [PeerConnectionStatus.CONNECTED]. Only [PeerConnectionStatus.CONNECTED] is the only value
     * emitted from any stream; so when all emit a single value then it will be connected.
     */
    private fun waitForConnectedState(connectionState: ConnectionState): Observable<PeerConnectionStatus> {
        val connectionStatusObservable = getConnectionStatusObservable(connectionState)
        return if (stackConfig is DtlsSocketNetifGattlink) {
            //Gattlink and tls events combined
            Observable.zip(
                connectionStatusObservable,
                getDtlsEventObservable(connectionState),
                BiFunction { _, _ -> PeerConnectionStatus.CONNECTED }
            )
        } else {
            //Just Gattlink events
            connectionStatusObservable
        }.timeout(
            // Timeout for first emission only
            Observable.empty<PeerConnectionStatus>().delay(connectTimeout, TimeUnit.SECONDS),
            Function { _: PeerConnectionStatus -> Observable.never<Any>() }
        )
    }

    /**
     * Only emits [PeerConnectionStatus.CONNECTED] on [TLS_STATE_SESSION] and a
     * [NodeDisconnectedException] if there was a [TLS_STATE_ERROR]
     */
    private fun getDtlsEventObservable(connectionState: ConnectionState): Observable<PeerConnectionStatus> =
        Observable.defer {
            Timber.i("StackPeer with key $key listening to tls events")
            dtlsEventObservableProvider(connectionState.stack)
                .doOnNext { Timber.d("StackPeer with key $key Tls status changed to ${it.state}") }
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
                            PeerConnectionStatus.CONNECTED
                        }
                        //This will never happen
                        else -> throw IllegalStateException()
                    }
                }
        }

    /**
     * Only emits [PeerConnectionStatus.CONNECTED] on [PeripheralConnectionStatus.CONNECTED] and
     * [NodeDisconnectedException] if there was a [PeripheralConnectionStatus.DISCONNECTED]
     */
    private fun getConnectionStatusObservable(connectionState: ConnectionState): Observable<PeerConnectionStatus> =
        Observable.defer {
            Timber.i("StackPeer with key $key listening to peripheral events")
            Observable.just(PeripheralConnectionStatus.CONNECTED)
                .filter { connectionState.gattConnection.isConnected }
                .concatWith(connectionStatusProvider(connectionState.gattConnection))
                .map { status ->
                    Timber.i("StackPeer with key $key received connection change status: $status")
                    when (status) {
                        PeripheralConnectionStatus.CONNECTED -> PeerConnectionStatus.CONNECTED
                        PeripheralConnectionStatus.DISCONNECTED -> throw NodeDisconnectedException()
                    }
                }
        }

    @Synchronized
    private fun removeActiveConnection() {
        Timber.i("StackPeer with key $key is removing the active connection")
        disposable = null
        peerConnectionObservable = null
    }

    @Synchronized
    private fun tearDown(connectionState: ConnectionState) {
        Timber.i("StackPeer with key $key is detaching its service and tearing down its stack, bridge")
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

    private fun requestTrackerDisconnect(bluetoothAddress: String): Completable {
        return fitbitGatt.getGattConnection(bluetoothAddress)
            .toSingle()
            .flatMapCompletable {
                linkControllerProvider(it)
                    .setGeneralPurposeCommand(GeneralPurposeCommandCode.DISCONNECT)
                    .onErrorComplete()
            }.onErrorComplete()
    }

    private data class ConnectionState(val bridge: Bridge, val stack: Stack, val peer: BitGattPeer, val gattConnection: GattConnection)
}
