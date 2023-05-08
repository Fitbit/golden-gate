// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.stackservice

import com.fitbit.goldengate.bindings.node.NodeKey
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.NodeMapper
import com.fitbit.goldengate.node.Peer
import com.fitbit.goldengate.node.PeerBuilder
import com.fitbit.goldengate.node.stack.StackPeer
import kotlin.test.assertEquals
import org.junit.Test
import org.mockito.kotlin.doReturn
import org.mockito.kotlin.mock
import org.mockito.kotlin.verify

class StackServiceMapperTest {

  private val nodeKey = mock<NodeKey<Any>>()
  private val nodeBuilder = mock<PeerBuilder<StackService, NodeKey<Any>>>()
  private val stackService = mock<StackService>()
  private val node = mock<StackPeer<StackService>> { on { stackService } doReturn stackService }
  private val nodeMapper = mock<NodeMapper> { on { get(nodeKey, nodeBuilder) } doReturn node }
  private val getStackService =
    mock<(Peer<*>) -> StackService> { on { invoke(node) } doReturn stackService }

  private val stackServiceMapper = StackServiceMapper(getStackService, nodeBuilder, nodeMapper)

  @Test
  fun get() {
    assertEquals(stackService, stackServiceMapper.get(nodeKey))

    verify(nodeMapper).get(nodeKey, nodeBuilder)
    verify(getStackService).invoke(node)
  }
}
