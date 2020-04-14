package com.fitbit.goldengate.bindings.io

import com.fitbit.goldengate.bindings.DataSinkDataSource
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bindings.util.PerfDataStats
import io.reactivex.Maybe
import timber.log.Timber

const val BLASTER_DEFAULT_PACKET_SIZE = 1024

const val EXTRA_BLAST_PACKET_SIZE = "extra_blast_packet_size"

/**
 * Stack service for supporting blasting random packets over attached GG stack.
 *
 * To start blasting call [start] method, blasting will only be stopped after calling [stop]
 *
 * @param lwipBased  true is configured with a Network Interface, false otherwise (default is true)
 * @param packetSize Size of each packet to send.
 * @param maxPacketCount Number of packets to send, or 0 for unlimited.
 * @param sendInterval Time interval between packets, in milliseconds, or 0 for max speed.
 */
class Blaster(
        private val lwipBased: Boolean = true,
        private val packetSize: Int = BLASTER_DEFAULT_PACKET_SIZE,
        private val maxPacketCount: Int = 0,
        private val sendInterval: Long = 0L
) : StackService {

    private var stackPtr: Long? = null

    /**
     * Start Blasting data packets
     *
     * @throws IllegalStateException if called without attaching a stack
     */
    fun start() {
        stackPtr?.let {
            start(it)
        } ?: throw IllegalStateException("Cannot start blast as stack is not attached")
    }

    /**
     * Stop Blasting data packets
     *
     * @throws IllegalStateException if called without attaching a stack
     */
    fun stop() {
        stackPtr?.let {
            stop(it)
        } ?: throw IllegalStateException("Cannot stop blast as stack is not attached")
    }

    /**
     * Get stats for ongoing blast operation
     *
     * @throws IllegalStateException if called without attaching a stack
     */
    fun getStats(): Maybe<PerfDataStats> {
        return Maybe.create { emitter ->
            stackPtr?.let {
                val stats = getStats(selfPtr = it)
                emitter.onSuccess(stats)
            }?: emitter.onError(IllegalStateException("Cannot start stats as stack is not attached"))
        }
    }

    override fun attach(dataSinkDataSource: DataSinkDataSource) {
        stackPtr = attach(
                sinkPtr = dataSinkDataSource.getAsDataSinkPointer(),
                sourcePtr = dataSinkDataSource.getAsDataSourcePointer(),
                lwipBased = lwipBased,
                packetSize = packetSize,
                maxPacketCount = maxPacketCount,
                sendInterval = sendInterval
        )

        Timber.i("Blast packet size: $packetSize")
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

    private external fun attach(
            sinkPtr: Long,
            sourcePtr: Long,
            lwipBased: Boolean,
            packetSize: Int,
            maxPacketCount: Int,
            sendInterval: Long
    ): Long

    private external fun start(selfPtr: Long): Long
    private external fun stop(selfPtr: Long): Long
    private external fun getStats(selfPtr: Long, clazz: Class<PerfDataStats> = PerfDataStats::class.java): PerfDataStats
    private external fun destroy(selfPtr: Long)

}

