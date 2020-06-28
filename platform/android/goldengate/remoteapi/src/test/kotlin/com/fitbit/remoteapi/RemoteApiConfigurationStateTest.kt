// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import android.content.Context
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.node.PeerConnectionStatus
import com.fitbit.goldengate.node.NodeMapper
import com.fitbit.goldengate.node.stack.StackPeer
import com.fitbit.goldengate.node.stack.StackPeerBuilder
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.never
import com.nhaarman.mockitokotlin2.verify
import io.reactivex.Observable
import io.reactivex.disposables.Disposable
import io.reactivex.internal.disposables.DisposableContainer
import io.reactivex.schedulers.Schedulers
import org.junit.Test
import java.lang.IllegalStateException
import kotlin.test.assertEquals

class RemoteApiConfigurationStateTest {

    private val peerName = "peerName"
    private val context = mock<Context>()
    private val gattConnection = mock<GattConnection>()
    private val bluetoothProvider = mock<BluetoothProvider> {
        on { bluetoothDevice(context, peerName) } doReturn gattConnection
    }
    private val disposableContainer = mock<DisposableContainer>()
    private val stackNodeBuilder = mock<StackPeerBuilder<*>>()
    private val disposable = mock<Disposable>()
    private val asyncConnection = mock<Observable<PeerConnectionStatus>>() {
        on { subscribe(any(), any()) } doReturn disposable
    }
    private val connection = mock<Observable<PeerConnectionStatus>> {
        on { subscribeOn(Schedulers.io()) } doReturn asyncConnection
    }
    private val stackNode = mock<StackPeer<*>> {
        on { connection() } doReturn connection
    }
    private val bluetoothAddressNodeKey = mock<BluetoothAddressNodeKey>()
    private val nodeMapper = mock<NodeMapper>()
    private val defaultConfiguration = mock<StackPeerBuilder<*>>()
    private val configurationMap = mock<MutableMap<String?, StackPeerBuilder<*>>>()
    private val bluetoothAddressNodeKeyProvider = mock<(GattConnection) -> BluetoothAddressNodeKey> {
        on { invoke(gattConnection) } doReturn bluetoothAddressNodeKey
    }
    private val lastConnectionHolder = mock<RemoteApiConfigurationState.LastConnectionHolder>()

    @Test
    fun setPeerConfiguration() {
        RemoteApiConfigurationState(
                bluetoothProvider,
                disposableContainer,
                nodeMapper,
                defaultConfiguration,
                configurationMap,
                bluetoothAddressNodeKeyProvider,
                lastConnectionHolder
        ).setPeerConfiguration(peerName, stackNodeBuilder)

        verify(configurationMap)[peerName] = stackNodeBuilder
    }

    @Test
    fun getNode() {
        val nodeMapper = mock<NodeMapper> {
            on { get(bluetoothAddressNodeKey, stackNodeBuilder) } doReturn stackNode
        }
        val configurationMap = mock<MutableMap<String?, StackPeerBuilder<*>>> {
            on { this[peerName] } doReturn stackNodeBuilder
        }
        assertEquals(RemoteApiConfigurationState(
                bluetoothProvider,
                disposableContainer,
                nodeMapper,
                defaultConfiguration,
                configurationMap,
                bluetoothAddressNodeKeyProvider,
                lastConnectionHolder
        ).getNode(context, peerName), stackNode)

        verify(bluetoothProvider).bluetoothDevice(context, peerName)
        verify(configurationMap)[peerName]
        verify(nodeMapper).get(bluetoothAddressNodeKey, stackNodeBuilder)
    }

    @Test
    fun getNodeWithoutPeerName() {
        val lastConnectionHolder = mock<RemoteApiConfigurationState.LastConnectionHolder> {
            on { lastConnectedNode } doReturn stackNode
        }
        assertEquals(RemoteApiConfigurationState(
                bluetoothProvider,
                disposableContainer,
                nodeMapper,
                defaultConfiguration,
                configurationMap,
                bluetoothAddressNodeKeyProvider,
                lastConnectionHolder
        ).getNode(context), stackNode)

        verify(bluetoothProvider, never()).bluetoothDevice(context, peerName)
        verify(configurationMap, never())[any()]
        verify(lastConnectionHolder).lastConnectedNode
    }

    @Test(expected = IllegalStateException::class)
    fun getNodeWithoutSettingAndNoNodeConnection() {
        RemoteApiConfigurationState(
                bluetoothProvider,
                disposableContainer,
                nodeMapper,
                defaultConfiguration,
                configurationMap,
                bluetoothAddressNodeKeyProvider,
                lastConnectionHolder
        ).getNode(context)

        verify(bluetoothProvider, never()).bluetoothDevice(context, peerName)
        verify(configurationMap)[null]
        verify(nodeMapper).get(bluetoothAddressNodeKey, defaultConfiguration)
    }

    @Test
    fun connect() {
        val nodeMapper = mock<NodeMapper> {
            on { get(bluetoothAddressNodeKey, stackNodeBuilder) } doReturn stackNode
        }
        val configurationMap = mock<MutableMap<String?, StackPeerBuilder<*>>> {
            on { this[peerName] } doReturn stackNodeBuilder
        }
        RemoteApiConfigurationState(
                bluetoothProvider,
                disposableContainer,
                nodeMapper,
                defaultConfiguration,
                configurationMap,
                bluetoothAddressNodeKeyProvider,
                lastConnectionHolder
        ).connect(context, peerName)

        verify(lastConnectionHolder).lastConnectedNode = stackNode
        verify(stackNode).connection()
        verify(connection).subscribeOn(Schedulers.io())
        verify(asyncConnection).subscribe(any(), any())
        verify(disposableContainer).add(disposable)
    }
}
