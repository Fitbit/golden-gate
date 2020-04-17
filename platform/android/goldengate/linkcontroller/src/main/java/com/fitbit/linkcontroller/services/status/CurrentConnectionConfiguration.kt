// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller.services.status

import java.nio.ByteBuffer
import java.nio.ByteOrder

/**
 * This class has data about theCurrent Connection Configuration.
 * It contains the connection interval, slave latency, supervision timeout, mtu and connection Mode.
 */
data class CurrentConnectionConfiguration(
    //in miliseconds
    val interval:Float,
    //in number of connectionEvents
    val slaveLatency:Short,
    // in miliseconds
    val supervisionTimeout:Short,
    val mtu:Short,
    val connectionMode: ConnectionMode
) {


    companion object {
        // interval for connection interval in msec
        private const val CONNECTION_INTERVAL_VALUE = 1.25F
        // interval for supervision timeout in msec
        private const val SUPERVISION_TIMEOUT_INTERVAL = 10

        fun parseFromByteArray(byteArray: ByteArray): CurrentConnectionConfiguration {

            val buffer=ByteBuffer.wrap(byteArray).order(ByteOrder.LITTLE_ENDIAN)
            val interval = CONNECTION_INTERVAL_VALUE * buffer.short
            val slaveLatency = buffer.short
            val supervisionTimeout = (SUPERVISION_TIMEOUT_INTERVAL * buffer.short).toShort()
            val mtu = buffer.short
            val mode = buffer.get()
            if (mode > 2){
                throw ConnectionConfigurationParseError()
            }
            val connectionMode= ConnectionMode.values()[mode.toInt()]
            return CurrentConnectionConfiguration(
                interval,
                slaveLatency,
                supervisionTimeout,
                mtu,
                connectionMode
            )
        }
        fun getDefaultConfiguration(): CurrentConnectionConfiguration {
            //TODO setup default configuration parameters
            return CurrentConnectionConfiguration(
                10F,
                0,
                100,
                185,
                ConnectionMode.DEFAULT
            )
        }
    }


}

class ConnectionConfigurationParseError:Exception()
