package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import io.reactivex.Completable

/**
 * Interface for Golden Gate link-up process
 */
interface Linkup {
    fun link(peer: BitGattPeer): Completable
}
