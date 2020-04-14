package com.fitbit.goldengate.node

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bindings.stack.StackEvent
import com.fitbit.linkcontroller.LinkController
import com.fitbit.linkcontroller.LinkControllerProvider
import com.fitbit.linkcontroller.services.configuration.GeneralPurposeCommandCode
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionMode
import io.reactivex.Completable
import io.reactivex.Observable
import timber.log.Timber
import java.util.concurrent.TimeUnit

/**
 * Receives stack events and dispatches them to the right handler. For now this only supports
 * gattlink buffer events.
 * @param eventStream The stream emitting stack events
 * @param gattConnection The connection on which to operate
 * @param linkController The link controller instance (default)
 * @param votingEnabled Determines whether we enable voting for fast/slow mode
 */
class StackEventHandler(
    val eventStream: Observable<StackEvent>,
    val gattConnection: GattConnection,
    private val votingEnabled: Boolean = true,
    val linkControllerProvider: (GattConnection) -> LinkController = { LinkControllerProvider.INSTANCE.getLinkController(it) }
) {

    companion object {
        private val ONE_MINUTE_IN_MILLISECONDS: Int = TimeUnit.MINUTES.toMillis(1).toInt()
    }

    fun dispatchEvents(): Completable = eventStream
        .doOnNext { Timber.d("StackEvent received: $it") }
        .flatMapCompletable { stackEvent ->
            when (stackEvent) {
                is StackEvent.GattlinkBufferOverThreshold -> setPreferredConnectionModeToFast()
                is StackEvent.GattlinkBufferUnderThreshold -> setPreferredConnectionModeToSlow()
                is StackEvent.GattlinkSessionStall -> handleStackStalledEvent(stackEvent.stalledTime)
                else -> Completable.complete()
            }
        }

    private fun setPreferredConnectionModeToFast(): Completable {
        return when {
            votingEnabled -> linkControllerProvider(gattConnection).setPreferredConnectionMode(PreferredConnectionMode.FAST)
                .onErrorComplete()
            else -> Completable.complete()
        }
    }

    private fun setPreferredConnectionModeToSlow(): Completable {
        return when {
            votingEnabled -> linkControllerProvider(gattConnection).setPreferredConnectionMode(PreferredConnectionMode.SLOW)
                .onErrorComplete()
            else -> Completable.complete()
        }
    }

    private fun setGeneralPurposeCommandDisconnect(): Completable {
        Timber.d("set Disconnection via GeneralPurposeCommand characteristic")
        return linkControllerProvider(gattConnection).setGeneralPurposeCommand(GeneralPurposeCommandCode.DISCONNECT)
            .onErrorComplete()
    }

    private fun handleStackStalledEvent(stalledTime: Int): Completable {
        Timber.d("Received a stack stalled event with stalled Time: $stalledTime")
        return if (stalledTime > ONE_MINUTE_IN_MILLISECONDS) {
            setGeneralPurposeCommandDisconnect()
        } else {
            Completable.complete()
        }
    }
}