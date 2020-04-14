package com.fitbit.goldengate

import android.content.Context
import com.fitbit.goldengate.bindings.GoldenGate
import com.fitbit.goldengate.bt.GlobalBluetoothGattInitializer

object GoldenGateConnectionManagerModule {

    private val globalBluetoothGattInitializer = GlobalBluetoothGattInitializer()

    /**
     * Initialized GG module.
     */
    @JvmStatic
    fun init(applicationContext: Context, loggingEnabled: Boolean = true) {
        GoldenGate.init(loggingEnabled)
        globalBluetoothGattInitializer.start(applicationContext)
    }
}
