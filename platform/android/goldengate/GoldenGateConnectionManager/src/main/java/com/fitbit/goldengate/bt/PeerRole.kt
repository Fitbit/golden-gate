package com.fitbit.goldengate.bt

/**
 * presents different roles of BLE peer (remote) devices
 */
sealed class PeerRole(val name: String) {
    object Central: PeerRole("Central")
    object Peripheral: PeerRole("Peripheral")
}
