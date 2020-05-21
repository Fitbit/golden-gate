// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.io.BLASTER_DEFAULT_PACKET_SIZE
import com.fitbit.goldengate.bindings.io.Blaster
import com.fitbit.goldengate.bindings.io.EXTRA_BLAST_PACKET_SIZE
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.GattlinkStackConfig
import com.fitbit.goldengate.bindings.stack.SocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.isLwipBased
import com.fitbit.goldengate.node.NodeBuilder
import com.fitbit.goldengate.node.stack.StackNode
import com.fitbit.goldengate.node.stack.StackNodeBuilder
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import kotlinx.android.synthetic.main.a_blast.bytes_received
import kotlinx.android.synthetic.main.a_blast.packets_received
import kotlinx.android.synthetic.main.a_blast.throughput
import timber.log.Timber
import java.util.concurrent.TimeUnit

class BlastActivity : AbstractHostActivity<Blaster>() {

    private var started = false
    private var packetSize = BLASTER_DEFAULT_PACKET_SIZE;

    override fun onCreate(savedInstanceState: Bundle?) {
        // update blast packet size
        packetSize = intent.getIntExtra(EXTRA_BLAST_PACKET_SIZE, BLASTER_DEFAULT_PACKET_SIZE)

        super.onCreate(savedInstanceState)
    }

    private fun toggleBlast(blaster: Blaster, stackConfig: StackConfig) {
        when {
            started -> blaster.stop()
            else -> blaster.start()
        }
        started = !started
        renderButtonText(stackConfig)
    }

    private fun renderButtonText(stackConfig: StackConfig) {
        val messageType = when (stackConfig) {
            is GattlinkStackConfig -> "Gattlink"
            is SocketNetifGattlink -> "UDP"
            is DtlsSocketNetifGattlink -> "DTLS"
        }
        return when {
            started -> send.text = "Stop Blast over $messageType"
            else -> send.text = "Start Blast over $messageType"
        }
    }

    override fun getContentViewRes(): Int = R.layout.a_blast

    override fun getNodeBuilder(
            stackConfig: StackConfig,
            connectionStatus: (GattConnection) -> Observable<PeripheralConnectionStatus>,
            dtlsStatus: (Stack) -> Observable<DtlsProtocolStatus>
    ): NodeBuilder<Blaster, BluetoothAddressNodeKey> = StackNodeBuilder(
            Blaster::class.java,
            stackConfig
    ) { nodeKey -> StackNode(nodeKey, stackConfig, Blaster(stackConfig.isLwipBased(), packetSize = packetSize), connectionStatus, dtlsStatus) }

    override fun onConnected(stackService: Blaster, stackConfig: StackConfig) {
        throughput.visibility = View.VISIBLE
        bytes_received.visibility = View.VISIBLE
        packets_received.visibility = View.VISIBLE
        renderButtonText(stackConfig)
        send.setOnClickListener { toggleBlast(stackService, stackConfig) }
        stackService.getStats()
        disposeBag.add(Observable.interval(1000L, TimeUnit.MILLISECONDS, Schedulers.computation())
                .timeInterval()
                .flatMapMaybe { stackService.getStats() }
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe({
                    throughput.text = String.format("Throughput: %d B/s", it.throughput)
                    packets_received.text = String.format("Packets Rec: %d", it.packetsReceived)
                    bytes_received.text = String.format("Bytes Rec: %d", it.bytesReceived)
                }, {
                    Timber.e(it, "Error getting stats")
                }))
    }

    companion object {
        fun getIntent(context: Context): Intent {
            return Intent(context, BlastActivity::class.java)
        }
    }
}
