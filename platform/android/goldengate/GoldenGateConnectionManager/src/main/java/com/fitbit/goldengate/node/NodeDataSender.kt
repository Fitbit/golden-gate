package com.fitbit.goldengate.node

import io.reactivex.Completable

/**
 * Sends data from connected Node
 */
interface NodeDataSender {

    /**
     * sends given [data] to connected Node
     */
    fun send(data: ByteArray): Completable

}
