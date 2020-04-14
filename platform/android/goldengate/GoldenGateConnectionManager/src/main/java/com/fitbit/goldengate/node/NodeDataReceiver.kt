package com.fitbit.goldengate.node

import io.reactivex.Observable

/**
 * Receives data from connected Node
 */
interface NodeDataReceiver {

    /**
     * Observer to receive data from connected Node
     */
    fun receive(): Observable<ByteArray>

}
