// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node.stack

import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.node.NodeMapper

/**
 * Utility object for instantiating a [StackPeerBuilder]<[CoapEndpoint], [BluetoothAddressNodeKey]>
 */
class CoapNodeProvider(
  shouldSetStartMtuChecker: () -> Boolean = { true },
  private val nodeMapper: NodeMapper = NodeMapper.instance,
  private val stackPeerBuilder: StackPeerBuilder<CoapEndpoint> =
    StackPeerBuilder(
      CoapEndpoint::class.java,
      PeerRole.Peripheral,
      CoapEndpointBuilder,
      DtlsSocketNetifGattlink(),
      shouldSetStartMtuChecker
    ),
  private val bluetoothAddressNodeKeyProvider: (String) -> BluetoothAddressNodeKey = {
    BluetoothAddressNodeKey(it)
  }
) {

  /** @return a [StackPeer]<[CoapEndpoint]> */
  fun getNode(bluetoothAddress: String) =
    nodeMapper.get(bluetoothAddressNodeKeyProvider(bluetoothAddress), stackPeerBuilder)

  /**
   * Removes a node from the mapper, this will close the StackNode
   *
   * @param bluetoothAddress the MAC address of the node we want to remove
   */
  fun removeNode(bluetoothAddress: String) =
    nodeMapper.removeNode(bluetoothAddressNodeKeyProvider(bluetoothAddress))
}
