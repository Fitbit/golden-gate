package com.fitbit.goldengate.bindings.services

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.NativeReference
import com.fitbit.goldengate.bindings.remote.RemoteShellThread
import com.fitbit.goldengate.bindings.stack.StackService

/**
 * Service to automate sending and receiving packets, this service can be exposed via remote api
 */
class BlastService private constructor(
        private val remoteShellNativeReference: NativeReference
): StackService {

    class Provider(private val remoteShellThread: RemoteShellThread) : StackService.Provider {
        override fun get() : BlastService {
            return BlastService(remoteShellThread)
        }
    }

    // Reference to native blast service
    private val thisPointer: Long

    init {
        thisPointer = create()
        // register during init as we are only going to listen on remote shell
        register()
    }

    override fun attach(dataSinkDataSource: DataSinkDataSource) {
        attach(
                sourcePtr = dataSinkDataSource.getAsDataSourcePointer(),
                sinkPtr = dataSinkDataSource.getAsDataSinkPointer()
        )
    }

    override fun detach() {
        detach(selfPtr = thisPointer)
    }

    override fun close() {
        destroy()
    }

    private external fun create(): Long

    private external fun register(selfPtr: Long = thisPointer, shellPtr: Long = remoteShellNativeReference.thisPointer)

    private external fun attach(selfPtr: Long = thisPointer, sourcePtr: Long, sinkPtr: Long)

    private external fun detach(selfPtr: Long = thisPointer)

    private external fun destroy(selfPtr: Long = thisPointer)
}