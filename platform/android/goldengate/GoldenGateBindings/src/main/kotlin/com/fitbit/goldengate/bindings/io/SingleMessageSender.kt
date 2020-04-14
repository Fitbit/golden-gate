package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bindings.util.hexString
import io.reactivex.Completable

/**
 * StackService for sending single message over attached GG stack
 */
class SingleMessageSender : StackService {

    private var stackPtr: Long? = null

    /**
     * Send a single message to attached Node. It is required to call attach before calling this method
     */
    fun send(message: ByteArray): Completable {
        return Completable.create { emitter ->
            stackPtr?.let {
                send(it, message)
                emitter.onComplete()
            }?: emitter.onError(Exception("Cannot send message as stack is not attached: ${message.hexString()}"))
        }
    }

    override fun attach(dataSinkDataSource: DataSinkDataSource) {
        stackPtr = attach(sinkPtr = dataSinkDataSource.getAsDataSinkPointer())
    }

    override fun detach() {
        stackPtr?.let {
            destroy(it)
            stackPtr = null
        }
    }

    override fun close() {
        detach()
    }

    private external fun attach(sinkPtr: Long): Long
    private external fun send(selfPtr: Long, message: ByteArray): Long
    private external fun destroy(selfPtr: Long)
}
