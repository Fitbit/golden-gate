// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.stack

import androidx.annotation.Keep
import androidx.annotation.VisibleForTesting
import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.GoldenGateNativeException
import com.fitbit.goldengate.bindings.GoldenGateNativeResult
import com.fitbit.goldengate.bindings.NativeReference
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.dtls.TlsKeyResolver
import com.fitbit.goldengate.bindings.dtls.TlsKeyResolverRegistry
import com.fitbit.goldengate.bindings.node.NodeKey
import io.reactivex.Observable
import io.reactivex.subjects.BehaviorSubject
import timber.log.Timber
import java.io.Closeable
import java.net.Inet4Address
import java.util.concurrent.atomic.AtomicBoolean


const val GATT_HEADER_SIZE = 3

/**
 * A Kotlin representation of a Golden Gate stack. The data sink and data source exposed through
 * [getAsDataSinkPointer] and [getAsDataSourcePointer] are the data sink/source on the "top" of the stack.
 *
 * Appropriate things to attach to those are [StackService] like [CoapEndpoint] and [Blaster]
 *
 * @param nodeKey node identifier this stack is attached to
 * @param transportSinkPtr Bottom of stack sink (data will be transmitted on this sink reference)
 * @param transportSourcePtr Bottom of stack (data will be received from this source reference)
 * @param stackConfig configuration in which this stack should be created (Default is [DtlsSocketNetifGattlink])
 * @param isNode if stack should be configured as Node or Hub (default is false)
 */
class Stack constructor(
        private val nodeKey: NodeKey<*>,
        private val transportSinkPtr: Long,
        private val transportSourcePtr: Long,
        private val stackConfig: StackConfig = DtlsSocketNetifGattlink(),
        private val isNode: Boolean = false
) : NativeReference, DataSinkDataSource, Closeable {

    private val stackStarted = AtomicBoolean(false)
    private val stackClosed = AtomicBoolean(false) //Not the same as !stackStarted.get() because Stacks aren't reusable.
    private val dtlsEventSubject = BehaviorSubject.create<DtlsProtocolStatus>()
    private val stackEventSubject = BehaviorSubject.create<StackEvent>()

    override val thisPointer: Long

    init {
        thisPointer = createStack()
        attachEventListener()
    }

    private fun createStack(): Long {
        val stackCreationResult = create(
            nodeKey,
            stackConfig.configDescriptor,
            isNode,
            transportSinkPtr,
            transportSourcePtr,
            stackConfig.localAddress,
            stackConfig.localPort,
            stackConfig.remoteAddress,
            stackConfig.remotePort,
            TlsKeyResolverRegistry.resolvers
        )
        if(stackCreationResult.result < 0 ) {
            val error = GoldenGateNativeResult.getNativeResultFrom(stackCreationResult.result)
            throw GoldenGateNativeException(error.title, stackCreationResult.result)
        } else {
            return stackCreationResult.stackPointer
        }
    }

    private fun attachEventListener() {
        val attachResult = attachEventListener(selfPtr = thisPointer)
        if(attachResult < 0) {
            Timber.w("Failed to attach DTLS event listener to stack $attachResult")
        }
    }

    /**
     * Observable on which DTLS event changes are available. This observable is only applicable
     * when stack is configured with DTLS
     */
    val dtlsEventObservable: Observable<DtlsProtocolStatus>
        get() = dtlsEventSubject

    val stackEventObservable: Observable<StackEvent>
        get() = stackEventSubject

    /**
     * Start the stack must be called as soon as Node is connected
     */
    fun start() {
        if (!stackStarted.getAndSet(true)) {
            start(thisPointer)
        } else {
            Timber.w("Ignoring as Stack is already started")
        }
    }

    /**
     * Update MTU for this stack
     *
     * TODO do we want to be able to set an MTU on a not-yet-started stack? Will that work or crash?
     *
     * @return false if the stack is closed or if mtu otherwise fails. true if it succeeds
     */
    fun updateMtu(mtu: Int): Boolean {
        if (stackClosed.get()) {
            Timber.w("Not updating MTU. Stack started?: ${stackStarted.get()} Stack closed?: ${stackClosed.get()}")
            return false
        }
        val success = updateMtu(mtu - GATT_HEADER_SIZE, thisPointer)
        Timber.d("Updated stack MTU. Success?: $success new stack mtu: $mtu - $GATT_HEADER_SIZE")
        return success
    }

    /**
     * There's no safe "default" we can return for the Sink and Source pointer, so we should crash here so that we can
     * have the best chance of debugging what happened rather than a null pointer dereference or use-after-free
     * somewhere else.
     *
     * Theoretically the values of stackClosed can change from when we check it in the conditional
     * to where we construct the string in this function. This is not currently mitigated as it is expected to be
     * exceedingly rare and it only impacts logging and not behavior.
     */
    private fun throwIfClosed() {
        if (stackClosed.get()) {
            throw IllegalStateException("This stack is either closed or not started. Started?: ${stackStarted.get()} Closed:? ${stackClosed.get()}")
        }
    }

    override fun getAsDataSinkPointer(): Long {
        throwIfClosed()
        return getTopPortAsDataSink(thisPointer)
    }

    override fun getAsDataSourcePointer(): Long {
        throwIfClosed()
        return getTopPortAsDataSource(thisPointer)
    }

    override fun close() {
        if (stackClosed.getAndSet(true)) { //This prevents a double-free in addition to letting us check from Kotlin if the stack is running.
            Timber.i("Stack already closed")
            return
        }
        Timber.d("Closing stack")
        destroy(thisPointer)
    }

    /*
     * This method gets called into from JNI and pushes its event to a subject.
     */
    @VisibleForTesting
    @Keep
    fun onDtlsStatusChange(tlsState: Int, tlsLastError: Int, pskIdentity: ByteArray) {
        Timber.d("DTLS state change received tlsState: $tlsState, tlsLastError: $tlsLastError, pskIdentity: $pskIdentity")
        val dtlsProtocolStatus = DtlsProtocolStatus(tlsState, tlsLastError, String(pskIdentity))
        dtlsEventSubject.onNext(dtlsProtocolStatus)
    }

    @VisibleForTesting
    @Keep
    fun onStackEvent(eventId: Int, data: Int) {
        Timber.d("Received StackEvent $eventId")
        val stackEvent = StackEvent.parseStackEvent(eventId, data)
        stackEventSubject.onNext(stackEvent)
    }

    private external fun create(
            nodeKey: NodeKey<*>,
            descriptor: String,
            isNode: Boolean,
            transportSinkPtr: Long,
            transportSourcePtr: Long,
            localAddress: Inet4Address,
            localPort: Int,
            remoteAddress: Inet4Address,
            remotePort: Int,
            tlsKeyResolver: TlsKeyResolver
    ): StackCreationResult

    private external fun destroy(selfPtr: Long)
    private external fun start(selfPtr: Long)
    private external fun attachEventListener(clazz: Class<Stack> = Stack::class.java, selfPtr: Long): Int
    private external fun getTopPortAsDataSink(selfPtr: Long): Long
    private external fun getTopPortAsDataSource(selfPtr: Long): Long

    private external fun updateMtu(mtu: Int, selfPtr: Long): Boolean
}
