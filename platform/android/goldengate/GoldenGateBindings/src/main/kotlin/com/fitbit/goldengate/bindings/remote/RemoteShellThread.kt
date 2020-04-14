package com.fitbit.goldengate.bindings.remote

import androidx.annotation.VisibleForTesting
import androidx.annotation.WorkerThread
import com.fitbit.goldengate.bindings.NativeReference
import timber.log.Timber
import java.lang.IllegalStateException

class RemoteShellThread(val webSocketTransport: WebSocketTransport) : NativeReference, Thread() {

    override val thisPointer: Long

    private val handlers = mutableSetOf<Long>()

    @Volatile
    private var shutDown = false

    init {
        thisPointer = createJNI(webSocketTransport.ptr)
        Timber.d("RemoteShell native side created.")
    }

    override fun run() {
        super.run()
        if (shutDown) {
            throw IllegalStateException("Cannot start the same RemoteShellThread twice.")
        }
        Timber.d("About to run JNI method for RemoteShellThread")
        runJNI()
        Timber.d("Remote Shell JNI method returned.")
        shutDown = true
        webSocketTransport.cleanup()
        handlers.forEach({freeHandlerJNI(it)})
        handlers.clear()
        Timber.d("Cleaned up handlers and websocket. Exiting RemoteShellThread.")
    }

    fun registerHandler(handler: CborHandler) {
        if (!shutDown) {
            Timber.d("Registering Handler ${handler.name}")
            handlers.add(registerHandlerJNI(thisPointer, handler.name, handler))
        } else {
            Timber.d("Not registering handler as RemoteShellThread has shut down.")
        }
    }

    @VisibleForTesting
    internal fun getHandlers() : Set<Long> {
        return handlers.toSet() //This should be immutable.
    }

    private external fun createJNI(transportPtr: Long): Long

    // We don't expect this method to return; it should be called on a bg thread
    @WorkerThread
    private external fun runJNI(shellPtr: Long = thisPointer)

    private external fun registerHandlerJNI(shellPtr: Long, handlerName: String, handler: CborHandler): Long

    private external fun freeHandlerJNI(handlerPtr: Long)
}
