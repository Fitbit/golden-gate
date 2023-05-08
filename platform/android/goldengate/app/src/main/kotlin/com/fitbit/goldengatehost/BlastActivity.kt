// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.View
import android.widget.TextView
import androidx.core.app.ActivityCompat
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
import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.node.PeerBuilder
import com.fitbit.goldengate.node.stack.StackPeer
import com.fitbit.goldengate.node.stack.StackPeerBuilder
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.concurrent.TimeUnit

class BlastActivity : AbstractHostActivity<Blaster>() {

    private var started = false
    private var packetSize = BLASTER_DEFAULT_PACKET_SIZE

    private lateinit var bytesReceived: TextView
    private lateinit var packetsReceived: TextView
    private lateinit var throughput: TextView

    override fun onCreate(savedInstanceState: Bundle?) {
        // update blast packet size
        packetSize = intent.getIntExtra(EXTRA_BLAST_PACKET_SIZE, BLASTER_DEFAULT_PACKET_SIZE)

        super.onCreate(savedInstanceState)
        bindViews()
    }

    private fun bindViews() {
        bytesReceived = ActivityCompat.requireViewById(this, R.id.bytes_received)
        packetsReceived = ActivityCompat.requireViewById(this, R.id.packets_received)
        throughput = ActivityCompat.requireViewById(this, R.id.throughput)
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

    override fun getPeerBuilder(
        peerRole: PeerRole,
        stackConfig: StackConfig,
        connectionStatus: (GattConnection) -> Observable<PeripheralConnectionStatus>,
        dtlsStatus: (Stack) -> Observable<DtlsProtocolStatus>,
        setStackStartMtu: Boolean,
    ): PeerBuilder<Blaster, BluetoothAddressNodeKey> = StackPeerBuilder(
        Blaster::class.java,
        peerRole,
        stackConfig
    ) { nodeKey ->
        StackPeer(
            nodeKey,
            peerRole,
            stackConfig,
            Blaster(stackConfig.isLwipBased(), packetSize = packetSize),
            connectionStatus,
            dtlsStatus,
            { setStackStartMtu })
    }

    override fun onConnected(stackService: Blaster, stackConfig: StackConfig) {
        throughput.visibility = View.VISIBLE
        bytesReceived.visibility = View.VISIBLE
        packetsReceived.visibility = View.VISIBLE
        renderButtonText(stackConfig)
        send.setOnClickListener { toggleBlast(stackService, stackConfig) }
        stackService.getStats()
        disposeBag.add(Observable.interval(1000L, TimeUnit.MILLISECONDS, Schedulers.computation())
            .timeInterval()
            .flatMapMaybe { stackService.getStats() }
            .observeOn(AndroidSchedulers.mainThread())
            .subscribe({
                throughput.text = String.format("Throughput: %d B/s", it.throughput)
                packetsReceived.text = String.format("Packets Rec: %d", it.packetsReceived)
                bytesReceived.text = String.format("Bytes Rec: %d", it.bytesReceived)
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
