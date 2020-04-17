// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bindings.stack.StackEvent
import com.fitbit.linkcontroller.LinkController
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCode
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionMode
import com.nhaarman.mockitokotlin2.any
import com.nhaarman.mockitokotlin2.doReturn
import com.nhaarman.mockitokotlin2.inOrder
import com.nhaarman.mockitokotlin2.mock
import com.nhaarman.mockitokotlin2.times
import com.nhaarman.mockitokotlin2.verify
import io.reactivex.Completable
import io.reactivex.subjects.PublishSubject
import org.junit.Test
import java.util.concurrent.TimeUnit

class StackEventHandlerTest {

    private val gattConnection = mock<GattConnection>()
    private val eventStream = PublishSubject.create<StackEvent>()

    companion object {
        private val STALLED_EVENT_DURATION_IN_MILLISECONDS: Int = TimeUnit.SECONDS.toMillis(12).toInt()
    }

    @Test
    fun `Test gattlink buffer congested and decongested received and fast mode slow mode triggered`() {
        // Given
        val linkController = mock<LinkController> {
            on { setPreferredConnectionMode(any()) } doReturn Completable.complete()
        }
        val stackEventDispatcher = StackEventHandler(eventStream, gattConnection, true, { linkController })

        // When
        stackEventDispatcher.dispatchEvents().test()
        eventStream.onNext(StackEvent.GattlinkBufferOverThreshold)
        eventStream.onNext(StackEvent.GattlinkBufferUnderThreshold)

        // Then
        val inOrder = inOrder(linkController)
        inOrder.verify(linkController).setPreferredConnectionMode(PreferredConnectionMode.FAST)
        inOrder.verify(linkController).setPreferredConnectionMode(PreferredConnectionMode.SLOW)
    }

    @Test
    fun `Test an error with the linkcontroller does not break the chain`() {
        // Given
        val linkController = mock<LinkController> {
            on { setPreferredConnectionMode(any()) } doReturn Completable.error(Exception())
        }
        val stackEventDispatcher = StackEventHandler(eventStream, gattConnection, true,
            { linkController })

        // When
        stackEventDispatcher.dispatchEvents().test()
        eventStream.onNext(StackEvent.GattlinkBufferOverThreshold)
        eventStream.onNext(StackEvent.GattlinkBufferUnderThreshold)

        // Then
        val inOrder = inOrder(linkController)
        inOrder.verify(linkController).setPreferredConnectionMode(PreferredConnectionMode.FAST)
        inOrder.verify(linkController).setPreferredConnectionMode(PreferredConnectionMode.SLOW)
    }

    @Test
    fun `Test we do no trigger fast or slow mode when voting is disabled`() {
        // Given
        val linkController = mock<LinkController> {
            on { setPreferredConnectionMode(any()) } doReturn Completable.complete()
        }
        val stackEventDispatcher = StackEventHandler(eventStream, gattConnection, false, { linkController })

        // When
        stackEventDispatcher.dispatchEvents().test()
        eventStream.onNext(StackEvent.GattlinkBufferOverThreshold)

        // Then
        val inOrder = inOrder(linkController)
        inOrder.verifyNoMoreInteractions()
    }

    @Test
    fun `Test we only trigger disconnection when gattlink stalled time is longer than 60 secs`() {
        // Given
        val linkController = mock<LinkController>() {
            on { setGeneralPurposeCommand(any()) } doReturn Completable.complete()
        }
        val stackEventDispatcher = StackEventHandler(eventStream, gattConnection, true, { linkController })

        // When
        stackEventDispatcher.dispatchEvents().test()
        eventStream.onNext(StackEvent.GattlinkSessionStall(STALLED_EVENT_DURATION_IN_MILLISECONDS))
        eventStream.onNext(StackEvent.GattlinkSessionStall(STALLED_EVENT_DURATION_IN_MILLISECONDS * 2))
        eventStream.onNext(StackEvent.GattlinkSessionStall(STALLED_EVENT_DURATION_IN_MILLISECONDS * 3))
        eventStream.onNext(StackEvent.GattlinkSessionStall(STALLED_EVENT_DURATION_IN_MILLISECONDS * 4))
        eventStream.onNext(StackEvent.GattlinkSessionStall(STALLED_EVENT_DURATION_IN_MILLISECONDS * 5))
        eventStream.onNext(StackEvent.GattlinkSessionStall(STALLED_EVENT_DURATION_IN_MILLISECONDS * 6))

        // Then
        verify(linkController, times(1)).setGeneralPurposeCommand(GeneralPurposeCommandCode.DISCONNECT)
    }
}
