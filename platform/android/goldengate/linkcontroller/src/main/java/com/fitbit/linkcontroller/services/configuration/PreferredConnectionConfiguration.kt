package com.fitbit.linkcontroller.services.configuration

import java.nio.ByteBuffer
import java.nio.ByteOrder
import kotlin.experimental.and
import kotlin.experimental.inv
import kotlin.experimental.or

private const val CONNECTION_INTERVAL = 1.25
private const val SUPERVISION_TIMEOUT_INTERVAL = 100

private const val FAST_MODE_FLAG: Byte = 0x01
private const val SLOW_MODE_FLAG: Byte = 0x02
private const val DLE_CONFIG_FLAG: Byte = 0x04
private const val MTU_FLAG: Byte = 0x08
private const val DISCONNECT_FLAG: Byte = 0x10

/**
 * This is used to set a Preferred Connection Configuration with the peripheral.
 * The client can specify configuration for a single mode, or for both (fast and slow) at the same time.
 * The client may also setup dle parameters and preferred mtu.
 * It is used within a LinkController that handles ble characteristic communication
 */
data class PreferredConnectionConfiguration(
    val requestedMode: Byte = 0,
    // valid if fast mode set
    val fastModeMinConnectionInterval: Short = 0,
    val fastModeMaxConnectionInterval: Short = 0,
    val fastModeSlaveLatency: Byte = 0,
    val fastModeSupervisionTimeout: Byte = 0,
    //valid if slow mode set
    val slowModeMinConnectionInterval: Short = 0,
    val slowModeMaxConnectionInterval: Short = 0,
    val slowModeSlaveLatency: Byte = 0,
    val slowModeSupervisionTimeout: Byte = 0,
    //valid if dle config flag set
    val dleMaxTxPDUSize: Byte = 0,
    val dleMaxTxTime: Short = 0,  // microseconds
    val mtu: Short = 0  //valid range 23-56
) {
    fun toByteArray(): ByteArray {
        val byteArray = ByteArray(18)
        val byteBuffer = ByteBuffer.wrap(byteArray).order(ByteOrder.LITTLE_ENDIAN)
        byteBuffer.put(requestedMode)

        byteBuffer.putShort(fastModeMinConnectionInterval)
        byteBuffer.putShort(fastModeMaxConnectionInterval)
        byteBuffer.put(fastModeSlaveLatency)
        byteBuffer.put(fastModeSupervisionTimeout)

        byteBuffer.putShort(slowModeMinConnectionInterval)
        byteBuffer.putShort(slowModeMaxConnectionInterval)
        byteBuffer.put(slowModeSlaveLatency)
        byteBuffer.put(slowModeSupervisionTimeout)

        byteBuffer.put(dleMaxTxPDUSize)
        byteBuffer.putShort(dleMaxTxTime)

        byteBuffer.putShort(mtu)
        return byteArray
    }

    /**
     * A builder used to create a [PreferredConnectionConfiguration]
     */
    class Builder constructor(


        private var requestedMode: Byte = 0,
        // valid if fast mode set
        private var fastModeMinConnectionInterval: Short = 0,
        private var fastModeMaxConnectionInterval: Short = 0,
        private var fastModeSlaveLatency: Byte = 0,
        private var fastModeSupervisionTimeout: Byte = 0,
        //valid if slow mode set
        private var slowModeMinConnectionInterval: Short = 0,
        private var slowModeMaxConnectionInterval: Short = 0,
        private var slowModeSlaveLatency: Byte = 0,
        private var slowModeSupervisionTimeout: Byte = 0,
        //valid if dle config flag set
        private var dleMaxTxPDUSize: Byte = 0,
        private var dleMaxTxTime: Short = 0,
        private var mtu: Short = 0 //valid range 23-56
    ) {


        private fun validateConnectionModeConfig(
            connectionModeMinConnectionInterval: Float,
            connectionModeMaxConnectionInterval: Float,
            connectionModeSlaveLatency: Int,
            connectionModeSupervisionTimeout: Int
        ) {
            if (connectionModeMaxConnectionInterval > 2000 || connectionModeMaxConnectionInterval < 15) {
                throw IllegalArgumentException("Invalid Max Connection Interval, expected range 15 msec - 2 sec, actual value $connectionModeMaxConnectionInterval msec")
            }
            if (connectionModeMinConnectionInterval > 2000 || connectionModeMinConnectionInterval < 15) {
                throw IllegalArgumentException("Invalid Min Connection Interval, expected range 15 msec - 2 sec, actual value $connectionModeMinConnectionInterval msec")
            }
            if (connectionModeMinConnectionInterval > connectionModeMaxConnectionInterval) {
                throw IllegalArgumentException("Invalid Connection interval, min is larger than max")
            }
            if (connectionModeSlaveLatency < 0 || connectionModeSlaveLatency > 30) {
                throw IllegalArgumentException("Invalid Connection Slave Latency, expected range 0-30, actual value $connectionModeSlaveLatency")
            }
            if (connectionModeSupervisionTimeout < 2000 || connectionModeSupervisionTimeout > 6000) {
                throw IllegalArgumentException("Invalid Connection Supervision timeout, expected range 2000 - 6000 msec, actual value $connectionModeSupervisionTimeout")
            }
            if (connectionModeMaxConnectionInterval * (connectionModeSlaveLatency + 1) > 2000) {
                throw IllegalArgumentException("Invalid Connection parameters: Interval Max * (Slave Latency + 1) ≤ 2 seconds")
            }
            if (connectionModeMaxConnectionInterval * (connectionModeSlaveLatency + 1) * 3 >= connectionModeSupervisionTimeout) {
                throw IllegalArgumentException("Invalid Connection parameters: Interval Max * (Slave Latency + 1) * 3 < connSupervisionTimeout\n")
            }

        }

        /**
         * This is used to set Configuration parameters for fast mode
         */
        fun setFastModeConfig(
            fastModeMinConnectionInterval: Float,
            fastModeMaxConnectionInterval: Float,
            fastModeSlaveLatency: Int,
            fastModeSupervisionTimeout: Int
        ) = apply {
            validateConnectionModeConfig(
                fastModeMinConnectionInterval,
                fastModeMaxConnectionInterval,
                fastModeSlaveLatency,
                fastModeSupervisionTimeout
            )
            this.fastModeMaxConnectionInterval =
                    (fastModeMaxConnectionInterval / CONNECTION_INTERVAL).toShort()
            this.fastModeMinConnectionInterval =
                    (fastModeMinConnectionInterval / CONNECTION_INTERVAL).toShort()
            this.fastModeSlaveLatency = fastModeSlaveLatency.toByte()
            this.fastModeSupervisionTimeout = (fastModeSupervisionTimeout /
                    SUPERVISION_TIMEOUT_INTERVAL).toByte()
            requestedMode = requestedMode or FAST_MODE_FLAG
        }
        /**
         * This is used to set preferred configuration parameters for slow mode
         */
        fun setSlowModeConfig(
            slowModeMinConnectionInterval: Float,
            slowModeMaxConnectionInterval: Float,
            slowModeSlaveLatency: Int,
            slowModeSupervisionTimeout: Int
        ) = apply {
            validateConnectionModeConfig(
                slowModeMinConnectionInterval,
                slowModeMaxConnectionInterval,
                slowModeSlaveLatency,
                slowModeSupervisionTimeout
            )
            this.slowModeMaxConnectionInterval =
                    (slowModeMaxConnectionInterval / CONNECTION_INTERVAL).toShort()
            this.slowModeMinConnectionInterval =
                    (slowModeMinConnectionInterval / CONNECTION_INTERVAL).toShort()
            this.slowModeSlaveLatency = slowModeSlaveLatency.toByte()
            this.slowModeSupervisionTimeout = (slowModeSupervisionTimeout /
                    SUPERVISION_TIMEOUT_INTERVAL).toByte()
            requestedMode = requestedMode or SLOW_MODE_FLAG
        }

        /**
         * This is used to set preferred configuration parameters for dle
         * @param maxPDU Valid range: 0x1B - 0xFB
         * @param maxTxTime Valid range: 0x0148 - 0x0848
         */
        fun setDLEConfig(
            maxPDU: Int,
            maxTxTime: Int
        ) = apply {
            if (maxPDU < 0x1B || maxPDU > 0xFB) {
                throw IllegalArgumentException("Invalid dle max PDU size. Valid range: 0x1B - 0xFB. Actual Value: $maxPDU")
            }
            if (maxTxTime < 0x0148 || maxTxTime > 0x0848) {
                throw IllegalArgumentException("Invalid dle max tx time. Valid range: 0x0148 - 0x0848. Actual value: $maxTxTime ")
            }
            dleMaxTxPDUSize = maxPDU.toByte()
            dleMaxTxTime = maxTxTime.toShort()
            requestedMode = requestedMode or DLE_CONFIG_FLAG
        }

        /**
         * This is used to set preferred mtu.
         * @param mtuValue Valid range: 23-506
         */
        fun setMtu(mtuValue: Short) = apply {
            if (mtuValue < 23 || mtuValue > 506) {
                throw IllegalArgumentException("Invalid mtu value. Valid range: 23-506. Actual value: $mtuValue ")
            }
            mtu = mtuValue
            requestedMode = requestedMode or MTU_FLAG
            return this
        }

        /**
         * Request the peripheral to disconnect
         */
        fun requestDisconnect() = apply {
            requestedMode = requestedMode or DISCONNECT_FLAG
        }
        fun requestDisconnect(enabled:Boolean) = apply {
            requestedMode = if(enabled){
                requestedMode or DISCONNECT_FLAG
            }else {
                requestedMode and DISCONNECT_FLAG.inv()
            }
        }

        /**
         * Creates the configuration with all the parameters
         */
        fun build(): PreferredConnectionConfiguration {
            return PreferredConnectionConfiguration(
                requestedMode,
                fastModeMinConnectionInterval,
                fastModeMaxConnectionInterval,
                fastModeSlaveLatency,
                fastModeSupervisionTimeout,
                slowModeMinConnectionInterval,
                slowModeMaxConnectionInterval,
                slowModeSlaveLatency,
                slowModeSupervisionTimeout,
                dleMaxTxPDUSize,
                dleMaxTxTime,
                mtu
            )
        }

    }
}

class CientPrefferedConfigurationParseError : Exception()
