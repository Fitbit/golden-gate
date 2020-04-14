package com.fitbit.goldengate.node.stack

import android.bluetooth.BluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitBluetoothDevice
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.bluetooth.fbgatt.rx.PeripheralDisconnector
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.GattServerMtuChangeListener
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.GattlinkStackConfig
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackEvent
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bt.PeripheralConnector
import com.fitbit.goldengate.node.Bridge
import com.fitbit.goldengate.node.LinkupHandler
import com.fitbit.goldengate.node.MtuChangeRequester
import com.fitbit.goldengate.node.NodeConnectionStatus
import com.fitbit.goldengate.peripheral.NodeDisconnectedException
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doAnswer
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.inOrder
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.never
import com.nhaarman.mockitokotlin2.times
import com.nhaarman.mockitokotlin2.verify
import com.nhaarman.mockitokotlin2.whenever
import io.reactivex.Completable
import io.reactivex.Observable
import io.reactivex.Single
import io.reactivex.plugins.RxJavaPlugins
import io.reactivex.schedulers.TestScheduler
import io.reactivex.subjects.BehaviorSubject
import io.reactivex.subjects.PublishSubject
import org.junit.After
import org.junit.Before
import org.junit.Test
import java.util.Random
import java.util.concurrent.TimeUnit
import java.util.concurrent.TimeoutException
import kotlin.test.assertEquals
import kotlin.test.assertNotEquals

class StackNodeTest {

    private val bluetoothAddress = "bluetoothAddress"
    private val pskIdentity = "TEST"
    private val stackConfig = mock<DtlsSocketNetifGattlink>()
    private val stackService = mock<StackService>()
    private val bridge = mock<Bridge>()
    private val stack = mock<Stack> {
        on { stackEventObservable } doReturn Observable.empty<StackEvent>()
    }
    private val device = mock<BluetoothDevice>()
    private val fitbitBluetoothDevice = mock<FitbitBluetoothDevice> {
        on { btDevice } doReturn device
        on { address } doReturn bluetoothAddress
    }
    private val peripheral = mock<BitGattPeripheral> {
        on { fitbitDevice } doReturn fitbitBluetoothDevice
    }
    private val gattConnection = mock<GattConnection> {
        on { device } doReturn fitbitBluetoothDevice
    }
    private val peripheralProvider = mock<(gattConnection: GattConnection) -> BitGattPeripheral> {
        on { invoke(gattConnection) } doReturn peripheral
    }
    private val key = mock<BluetoothAddressNodeKey> {
        on { value } doReturn bluetoothAddress
    }

    private val mtu = STACK_NODE_DEFAULT_MTU
    private val timeout = STACK_NODE_CONNECTION_TIMEOUT_SECONDS

    private val linkupHandler = mock<LinkupHandler> {
        on { link(peripheral) } doReturn Completable.complete()
    }

    private val peripheralConnectionHandler = mock<PeripheralConnector> {
        on { connect() } doReturn Single.just(gattConnection)
    }
    private val connectionStatusProvider = mock<(GattConnection) -> Observable<PeripheralConnectionStatus>> {
        on { invoke(gattConnection) } doReturn BehaviorSubject.createDefault<PeripheralConnectionStatus>(PeripheralConnectionStatus.CONNECTED)
    }
    private val buildBridge = mock<(GattConnection) -> Bridge> {
        on { invoke(gattConnection) } doReturn bridge
    }
    private val buildStack = mock<(Bridge) -> Stack> {
        on { invoke(bridge) } doReturn stack
    }
    private val mtuChangeRequester = mock<MtuChangeRequester> {
        on { requestMtu(peripheral, mtu) } doReturn Single.just(mtu)
        on { updateStackMtu(any()) } doReturn Single.just(mtu)
    }

    private val mtuChangeRequesterProvider = mock<(Stack) -> MtuChangeRequester> {
        on { invoke(stack) } doReturn mtuChangeRequester
    }

    private val dtlsEventProvider = mock<(Stack) -> Observable<DtlsProtocolStatus>> {
        on { invoke(stack) } doReturn PublishSubject.create { emitter ->
            emitter.onNext(DtlsProtocolStatus(DtlsProtocolStatus.TlsProtocolState.TLS_STATE_INIT.ordinal, 0, pskIdentity))
            emitter.onNext(DtlsProtocolStatus(DtlsProtocolStatus.TlsProtocolState.TLS_STATE_HANDSHAKE.ordinal, 0, pskIdentity))
            emitter.onNext(DtlsProtocolStatus(DtlsProtocolStatus.TlsProtocolState.TLS_STATE_SESSION.ordinal, 0, pskIdentity))
        }
    }

    private val mtuRequestSubject = PublishSubject.create<Int>()
    private val mockGattServerMtuChangeListener: GattServerMtuChangeListener = mock {
        on { register(any()) } doReturn Observable.never()
    }
    private val mockPeripheralDisconnector: PeripheralDisconnector = mock {
        on { disconnect(any<String>()) } doReturn Completable.complete()
    }

    private val testScheduler = TestScheduler()
    private val fitbitGatt = mock<FitbitGatt>()

    @Before
    fun setup() {
        RxJavaPlugins.setComputationSchedulerHandler { testScheduler }
    }

    @After
    fun tearDown() {
        RxJavaPlugins.reset()
    }

    @Test
    fun connection() {
        val connection = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        ).connection().test()

        //Connected
        connection.assertValue(NodeConnectionStatus.CONNECTED)
        connection.assertNotComplete()
        verifyConnectionSequence()
    }

    @Test
    fun disconnect() {
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        )

        //Works twice in the same way after disconnecting
        (1..2).forEach { n ->
            //Connecting
            val connection = stackNode.connection().test().assertNotComplete()

            //Disconnecting
            stackNode.disconnect()

            connection.assertValue(NodeConnectionStatus.CONNECTED)
            connection.assertComplete()

            verify(stackService, times(n)).detach()
            verify(bridge, times(n)).close()
            verify(stack, times(n)).close()

            connection.assertComplete()
        }
    }

    @Test
    fun close() {
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                peripheralDisconnector = mockPeripheralDisconnector,
                fitbitGatt = fitbitGatt
        )
        val connection = stackNode.connection().test().assertNotComplete()

        verifyConnectionSequence()

        stackNode.close()

        connection.assertValue(NodeConnectionStatus.CONNECTED).assertComplete()

        //Should disconnect
        verify(stackService).detach()
        verify(bridge).close()
        verify(stack).close()
        verify(mockPeripheralDisconnector).disconnect(key.value)

        //Should close
        verify(stackService).close()
    }

    private fun verifyConnectionSequence(nthTime: Int = 1) {
        val inOrder = inOrder(
                peripheralConnectionHandler,
                buildBridge,
                buildStack,
                stackService,
                mtuChangeRequesterProvider,
                mtuChangeRequester,
                linkupHandler,
                bridge,
                stack,
                connectionStatusProvider,
                dtlsEventProvider,
                mockGattServerMtuChangeListener
        )
        inOrder.verify(peripheralConnectionHandler, times(nthTime)).connect()
        inOrder.verify(buildBridge, times(nthTime)).invoke(gattConnection)
        inOrder.verify(buildStack, times(nthTime)).invoke(bridge)
        inOrder.verify(stackService, times(nthTime)).attach(stack)
        inOrder.verify(mtuChangeRequesterProvider, times(nthTime)).invoke(stack)
        inOrder.verify(mtuChangeRequester, times(nthTime)).requestMtu(peripheral, mtu)
        inOrder.verify(linkupHandler, times(nthTime)).link(peripheral)
        inOrder.verify(bridge, times(nthTime)).start()
        inOrder.verify(stack, times(nthTime)).start()
        inOrder.verify(connectionStatusProvider, times(nthTime)).invoke(gattConnection)
        inOrder.verify(dtlsEventProvider, times(nthTime)).invoke(stack)
        inOrder.verify(mockGattServerMtuChangeListener, times(nthTime)).register(any())
        inOrder.verify(stack, times(nthTime)).stackEventObservable
        inOrder.verifyNoMoreInteractions()
    }

    @Test
    fun closeBeforeConnected() {
        val peripheralConnectionHandler = mock<PeripheralConnector> {
            on { connect() } doReturn Single.never()
        }

        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        )
        val connection = stackNode.connection().test().assertNotComplete()

        stackNode.close()

        connection.assertNoValues().assertComplete()

        verify(peripheralConnectionHandler).connect()
        verify(buildBridge, never()).invoke(gattConnection)
        verify(buildStack, never()).invoke(bridge)
        verify(stackService, never()).attach(stack)

        //Should disconnect
        verify(stackService, never()).detach()
        verify(bridge, never()).close()
        verify(stack, never()).close()

        //Should close
        verify(stackService).close()
    }

    @Test
    fun closeBeforeMtuChange() {
        val mtuChangeRequester = mock<MtuChangeRequester> {
            on { requestMtu(any(), any()) } doReturn Single.never()
        }

        val mtuChangeRequesterProvider = mock<(Stack) -> MtuChangeRequester> {
            on { invoke(stack) } doReturn mtuChangeRequester
        }

        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                peripheralDisconnector = mockPeripheralDisconnector,
                fitbitGatt = fitbitGatt
        )
        val connection = stackNode.connection().test().assertNotComplete()

        stackNode.close()

        //Connecting
        connection.assertNoValues().assertComplete()

        verify(peripheralConnectionHandler).connect()
        verify(buildBridge).invoke(gattConnection)
        verify(buildStack).invoke(bridge)
        verify(stackService).attach(stack)
        verify(mtuChangeRequesterProvider).invoke(stack)
        verify(mtuChangeRequester).requestMtu(peripheral, mtu)
        verify(linkupHandler, never()).link(peripheral)

        //Should disconnect
        verify(stackService).detach()
        verify(bridge).close()
        verify(stack).close()
        verify(mockPeripheralDisconnector).disconnect(key.value)

        //Should close
        verify(stackService).close()
    }

    @Test
    fun disconnectBeforeConnected() {
        val peripheralConnectionHandler = mock<PeripheralConnector> {
            on { connect() } doReturn Single.never()
        }

        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                peripheralDisconnector = mockPeripheralDisconnector,
                fitbitGatt = fitbitGatt
        )

        //Should happen the same after disconnected
        (1..2).forEach { n ->
            val connection = stackNode.connection().test().assertNotComplete()

            verify(peripheralConnectionHandler, times(n)).connect()
            verify(buildBridge, never()).invoke(gattConnection)
            verify(buildStack, never()).invoke(bridge)
            verify(stackService, never()).attach(stack)

            stackNode.disconnect()

            connection.assertNoValues().assertComplete()

            //Should disconnect
            verify(stackService, never()).detach()
            verify(bridge, never()).close()
            verify(stack, never()).close()
            verify(mockPeripheralDisconnector, times(n)).disconnect(key.value)
        }
    }

    @Test
    fun disconnectBeforeMtuChange() {
        val mtuChangeRequester = mock<MtuChangeRequester> {
            on { requestMtu(any(), any()) } doReturn Single.never()
        }

        val mtuChangeRequesterProvider = mock<(Stack) -> MtuChangeRequester> {
            on { invoke(stack) } doReturn mtuChangeRequester
        }

        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                peripheralDisconnector = mockPeripheralDisconnector,
                fitbitGatt = fitbitGatt
        )

        //Should happen the same after disconnected
        (1..2).forEach { n ->
            val connection = stackNode.connection().test().assertNotComplete()

            stackNode.disconnect()

            connection.assertNoValues().assertComplete()

            verify(peripheralConnectionHandler, times(n)).connect()
            verify(buildBridge, times(n)).invoke(gattConnection)
            verify(buildStack, times(n)).invoke(bridge)
            verify(stackService, times(n)).attach(stack)
            verify(mtuChangeRequesterProvider, times(n)).invoke(stack)
            verify(mtuChangeRequester, times(n)).requestMtu(peripheral, mtu)
            verify(linkupHandler, never()).link(peripheral)

            //Should disconnect
            verify(stackService, times(n)).detach()
            verify(bridge, times(n)).close()
            verify(stack, times(n)).close()
            verify(mockPeripheralDisconnector, times(n)).disconnect(key.value)
        }
    }

    @Test
    fun disposeConnectionDoesNotDisconnect() {
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                peripheralDisconnector = mockPeripheralDisconnector,
                fitbitGatt = fitbitGatt
        )
        val connection = stackNode.connection().test().apply { dispose() }

        //Not disconnecting
        connection.assertValue(NodeConnectionStatus.CONNECTED).assertNotComplete()

        verify(stackService, never()).detach()
        verify(bridge, never()).close()
        verify(stack, never()).close()
        verify(mockPeripheralDisconnector, never()).disconnect(key.value)
    }

    @Test
    fun connectionMultipleTimesOnlyConnectsOnce() {
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                peripheralDisconnector = mockPeripheralDisconnector,
                fitbitGatt = fitbitGatt
        )

        //Works twice in the same way after disconnecting
        (1..2).forEach { n ->
            val connections = stackNode.connection() to stackNode.connection()
            assertEquals(connections.first, connections.second)
            val connection1 = connections.first.test()
            val connection2 = connections.second.test()

            connection1.assertValue(NodeConnectionStatus.CONNECTED)
            connection2.assertValue(NodeConnectionStatus.CONNECTED)

            //Connecting only once
            verify(peripheralConnectionHandler, times(n)).connect()
            verify(buildBridge, times(n)).invoke(gattConnection)
            verify(buildStack, times(n)).invoke(bridge)
            verify(stackService, times(n)).attach(stack)
            verify(mtuChangeRequesterProvider, times(n)).invoke(stack)
            verify(mtuChangeRequester, times(n)).requestMtu(peripheral, mtu)
            verify(bridge, times(n)).start()
            verify(stack, times(n)).start()
            verify(linkupHandler, times(n)).link(peripheral)

            stackNode.disconnect()

            //Disconnecting
            verify(stackService, times(n)).detach()
            verify(bridge, times(n)).close()
            verify(stack, times(n)).close()
            verify(mockPeripheralDisconnector, times(n)).disconnect(key.value)

            //Assert both completed
            connection1.assertComplete()
            connection2.assertComplete()
        }

    }

    @Test
    fun connectionThrowsNodeDisconnectedExceptionOnUnexpectedDisconnection() {
        val connectionStatusProvider = mock<(GattConnection) -> Observable<PeripheralConnectionStatus>> {
            val iterator = listOf<Observable<PeripheralConnectionStatus>>(
                    BehaviorSubject.createDefault<PeripheralConnectionStatus>(PeripheralConnectionStatus.DISCONNECTED),
                    BehaviorSubject.createDefault<PeripheralConnectionStatus>(PeripheralConnectionStatus.CONNECTED)
            ).iterator()
            on { invoke(gattConnection) } doAnswer { iterator.next() }
        }

        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        )

        //Connecting
        val connection1 = stackNode.connection()
        connection1.test().assertError(NodeDisconnectedException::class.java)

        //The connection should be a new one when trying again
        val connection2 = stackNode.connection()
        assertNotEquals(connection1, connection2)
        connection2.test().assertValue(NodeConnectionStatus.CONNECTED)
    }

    @Test
    fun connectionWhenGattIsAlreadyConnected() {
        whenever(gattConnection.isConnected).thenReturn(true)
        val connectionStatusProvider = mock<(GattConnection) -> Observable<PeripheralConnectionStatus>> {
            // won't wait for BT connection changes event if already connected
            on { invoke(gattConnection) } doAnswer { Observable.never() }
        }

        val connection = StackNode(
            key,
            stackConfig,
            stackService,
            linkupHandler,
            peripheralConnectionHandler,
            connectionStatusProvider,
            dtlsEventProvider,
            peripheralProvider,
            mtuChangeRequesterProvider,
            mtu,
            timeout,
            buildStack,
            buildBridge,
            mtuRequestSubject,
            mockGattServerMtuChangeListener,
            fitbitGatt = fitbitGatt
        ).connection().test()

        //Connected
        connection.assertValue(NodeConnectionStatus.CONNECTED)
    }

    @Test
    fun connectionWhenGattNotConnected() {
        whenever(gattConnection.isConnected).thenReturn(false)

        val connection = StackNode(
            key,
            stackConfig,
            stackService,
            linkupHandler,
            peripheralConnectionHandler,
            connectionStatusProvider,
            dtlsEventProvider,
            peripheralProvider,
            mtuChangeRequesterProvider,
            mtu,
            timeout,
            buildStack,
            buildBridge,
            mtuRequestSubject,
            mockGattServerMtuChangeListener,
            fitbitGatt = fitbitGatt
        ).connection().test()

        //Connected
        connection.assertValue(NodeConnectionStatus.CONNECTED)
    }

    @Test
    fun connectionTimesOutIfStateRemainsAtTlsError() {
        // mock to get TLS_STATE_ERROR callback
        val dtlsEventProvider = mock<(Stack) -> Observable<DtlsProtocolStatus>> {
            on { invoke(stack) } doReturn PublishSubject.create { emitter ->
                emitter.onNext(DtlsProtocolStatus(DtlsProtocolStatus.TlsProtocolState.TLS_STATE_ERROR.ordinal, 0, pskIdentity))
            }
        }
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        )

        //Connecting
        val connection = stackNode.connection()
        connection.test().also {
            testScheduler.advanceTimeBy(timeout, TimeUnit.SECONDS)
        }.assertError(TimeoutException::class.java)
    }

    @Test
    fun connectionIgnoreTlsErrorAndWaitForTlsSession() {
        // mock to get TLS_STATE_ERROR callback before getting TLS_STATE_SESSION
        val dtlsEventProvider = mock<(Stack) -> Observable<DtlsProtocolStatus>> {
            on { invoke(stack) } doReturn PublishSubject.create { emitter ->
                emitter.onNext(DtlsProtocolStatus(DtlsProtocolStatus.TlsProtocolState.TLS_STATE_ERROR.ordinal, 0, pskIdentity))
                emitter.onNext(DtlsProtocolStatus(DtlsProtocolStatus.TlsProtocolState.TLS_STATE_SESSION.ordinal, 0, pskIdentity))
            }
        }
        val connection = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        ).connection().test()

        //Connected
        connection.assertValue(NodeConnectionStatus.CONNECTED)
        connection.assertNotComplete()
    }

    @Test
    fun connectionThrows() {
        val peripheralConnectionHandler = mock<PeripheralConnector> {
            val iterator = listOf<Single<GattConnection>>(
                    Single.error(RuntimeException()),
                    Single.just(gattConnection)
            ).iterator()
            on { connect() } doAnswer { iterator.next() }
        }
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        )

        //Connecting
        val connection1 = stackNode.connection()
        connection1.test().assertError(RuntimeException::class.java)

        //The connection should be a new one when trying again
        val connection2 = stackNode.connection()
        assertNotEquals(connection1, connection2)
        connection2.test().assertValue(NodeConnectionStatus.CONNECTED)
    }

    @Test
    fun connectionTimesOut() {
        val connectionStatusProvider = mock<(GattConnection) -> Observable<PeripheralConnectionStatus>> {
            val iterator = listOf<Observable<PeripheralConnectionStatus>>(
                Observable.never(),
                Observable.concat(Observable.just(PeripheralConnectionStatus.CONNECTED), Observable.never())
            ).iterator()
            on { invoke(gattConnection) } doAnswer { iterator.next() }
        }
        val timeout = Random().nextLong()
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        )

        //Connecting
        val connection1 = stackNode.connection()
        connection1.test().also {
            testScheduler.advanceTimeBy(timeout, TimeUnit.SECONDS)
        }.assertError(TimeoutException::class.java)

        //The connection should be a new one when trying again
        val connection2 = stackNode.connection()
        assertNotEquals(connection1, connection2)
        connection2.test().assertValue(NodeConnectionStatus.CONNECTED)
    }

    @Test
    fun connectionToNonDtlsStackServiceDoesNotListenToDtlsEvents() {
        val stackConfig = mock<GattlinkStackConfig>()
        val connection = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        ).connection().test()

        //Connected
        connection.assertValue(NodeConnectionStatus.CONNECTED)
        connection.assertNotComplete()
        val inOrder = inOrder(
                peripheralConnectionHandler,
                buildBridge,
                buildStack,
                stackService,
                mtuChangeRequesterProvider,
                mtuChangeRequester,
                linkupHandler,
                connectionStatusProvider,
                bridge,
                stack
        )
        inOrder.verify(peripheralConnectionHandler, times(1)).connect()
        inOrder.verify(buildBridge, times(1)).invoke(gattConnection)
        inOrder.verify(buildStack, times(1)).invoke(bridge)
        inOrder.verify(stackService, times(1)).attach(stack)
        inOrder.verify(mtuChangeRequesterProvider, times(1)).invoke(stack)
        inOrder.verify(mtuChangeRequester, times(1)).requestMtu(peripheral, mtu)
        inOrder.verify(linkupHandler, times(1)).link(peripheral)
        inOrder.verify(bridge, times(1)).start()
        inOrder.verify(stack, times(1)).start()
        inOrder.verify(connectionStatusProvider, times(1)).invoke(gattConnection)
        inOrder.verify(stack, times(1)).stackEventObservable
        inOrder.verifyNoMoreInteractions()
    }

    @Test
    fun requestMtuChange() {
        val newMtu = Random().nextInt()
        val stackNode = StackNode(
                key,
                stackConfig,
                stackService,
                linkupHandler,
                peripheralConnectionHandler,
                connectionStatusProvider,
                dtlsEventProvider,
                peripheralProvider,
                mtuChangeRequesterProvider,
                mtu,
                timeout,
                buildStack,
                buildBridge,
                mtuRequestSubject,
                mockGattServerMtuChangeListener,
                fitbitGatt = fitbitGatt
        )
        val connection = stackNode.connection().test()

        verifyConnectionSequence()

        //Verify request invoked new mtu change
        stackNode.requestMtu(newMtu)
        verify(mtuChangeRequester).requestMtu(peripheral, newMtu)

        //Verify connected only once and not completed
        connection.assertValue(NodeConnectionStatus.CONNECTED)
        connection.assertNotComplete()
    }

    @Test
    fun connectionMtuChange() {
        val mockGattServerMtuChangeListener: GattServerMtuChangeListener = mock {
            on { register(any()) } doReturn PublishSubject.create { emitter ->
                emitter.onNext(200)
                emitter.onNext(300)
                emitter.onNext(400)
            }
        }

        StackNode(
            key,
            stackConfig,
            stackService,
            linkupHandler,
            peripheralConnectionHandler,
            connectionStatusProvider,
            dtlsEventProvider,
            peripheralProvider,
            mtuChangeRequesterProvider,
            mtu,
            timeout,
            buildStack,
            buildBridge,
            mtuRequestSubject,
            mockGattServerMtuChangeListener,
            fitbitGatt = fitbitGatt
        ).connection().test().assertValue(NodeConnectionStatus.CONNECTED)

        verify(mtuChangeRequester).updateStackMtu(200)
        verify(mtuChangeRequester).updateStackMtu(300)
        verify(mtuChangeRequester).updateStackMtu(400)
    }
}
