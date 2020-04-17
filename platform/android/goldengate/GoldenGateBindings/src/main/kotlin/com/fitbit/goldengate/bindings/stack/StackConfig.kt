// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.stack

import java.io.Serializable
import java.net.Inet4Address

/**
 * Configuration in which GG stack should be created
 */
sealed class StackConfig(
    /**
     * Unique Constant descriptor for a known stack configuration
     */
    val configDescriptor: String,
    /**
     * Stack local IP address (use 0.0.0.0 for default)
     */
    open val localAddress: Inet4Address,
    /**
     * Local UDP port number
     */
    open val localPort: Int,
    /**
     * Stack remote IP address (use 0.0.0.0 for default)
     */
    open val remoteAddress: Inet4Address,
    /**
     * Remote UDP port number
     */
    open val remotePort: Int
): Serializable

/**
 * Stack with only a Gattlink element.
 */
object GattlinkStackConfig : StackConfig("G", defaultAddress(), 0, defaultAddress(), 0)

/**
 * Stack with Gattlink, a Network Interface, and a UDP socket
 */
data class SocketNetifGattlink(
    override val localAddress: Inet4Address = defaultAddress(),
    override val localPort: Int = 0,
    override val remoteAddress: Inet4Address = defaultAddress(),
    override val remotePort: Int = 0
) : StackConfig("SNG", localAddress, localPort, remoteAddress, remotePort)

/**
 * Stack with Gattlink, a Network Interface, a UDP socket and DTLS
 */
data class DtlsSocketNetifGattlink(
    override val localAddress: Inet4Address = defaultAddress(),
    override val localPort: Int = 0,
    override val remoteAddress: Inet4Address = defaultAddress(),
    override val remotePort: Int = 0
) : StackConfig("DSNG", localAddress, localPort, remoteAddress, remotePort)

private fun defaultAddress() = Inet4Address.getByAddress(byteArrayOf(0, 0, 0, 0)) as Inet4Address

/**
 * true is configured with a Network Interface, false otherwise (default is true)
 */
fun StackConfig.isLwipBased() = this !is GattlinkStackConfig
