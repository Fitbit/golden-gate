// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost

import android.content.Context
import android.content.Intent
import android.view.View
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.BaseFitbitGattCallback
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointBuilder
import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequestBuilder
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.SocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.node.PeerBuilder
import com.fitbit.goldengate.node.stack.StackPeer
import com.fitbit.goldengate.node.stack.StackPeerBuilder
import com.fitbit.goldengatehost.coap.handler.addAllAvailableCoapHandlers
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.schedulers.Schedulers
import kotlinx.android.synthetic.main.a_single_message.coap_response_time
import kotlinx.android.synthetic.main.a_single_message.edittext
import kotlinx.android.synthetic.main.a_single_message.rxtext
import timber.log.Timber

class CoapActivity : AbstractHostActivity<CoapEndpoint>() {

    private var sendRequestTime: Long = 0
    private val fitbitGatt: FitbitGatt = FitbitGatt.getInstance()

    override fun getContentViewRes(): Int = R.layout.a_single_message

    override fun getPeerBuilder(
        peerRole: PeerRole,
        stackConfig: StackConfig,
        connectionStatus: (GattConnection) -> Observable<PeripheralConnectionStatus>,
        dtlsStatus: (Stack) -> Observable<DtlsProtocolStatus>
    ): PeerBuilder<CoapEndpoint, BluetoothAddressNodeKey> = StackPeerBuilder(
        CoapEndpoint::class.java,
        peerRole,
        stackConfig
    ) { nodeKey ->
        StackPeer(
            nodeKey,
            peerRole,
            stackConfig,
            CoapEndpointBuilder(),
            connectionStatus,
            dtlsStatus
        )
    }

    override fun onConnected(stackService: CoapEndpoint, stackConfig: StackConfig) {
        renderButtonText(stackConfig)
        edittext.visibility = View.VISIBLE

        send.setOnClickListener {
            sendRequestTime = System.currentTimeMillis()
            sendMessage(stackService)
        }
        addAllAvailableCoapHandlers(
            disposeBag,
            stackService
        )
    }

    private fun renderButtonText(stackConfig: StackConfig) {
        val messageType = when (stackConfig) {
            is SocketNetifGattlink -> "UDP"
            is DtlsSocketNetifGattlink -> "DTLS"
            else -> throw IllegalStateException("StackConfig not supported for single message")
        }
        send.text = "Send CoAP request over $messageType"
    }

    private fun sendMessage(stackService: CoapEndpoint) {
        Timber.i("Sending coap message")
        val request = OutgoingRequestBuilder("helloworld", Method.GET).build()

        disposeBag.add(
            stackService.responseFor(request)
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe(
                    { response -> updateResponseMessage(response) },
                    { throwable ->
                        updateResponseMessage(
                            throwable.message ?: "Error sending message"
                        )
                    }
                )
        )
    }

    private fun updateResponseMessage(response: IncomingResponse) {
        var callback: FitbitGatt.FitbitGattCallback? = null
        disposeBag.add(
            response.body.asData()
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .doOnSubscribe { disposable ->
                    callback = object : BaseFitbitGattCallback() {
                        // cancel CoAP request when BT is off
                        override fun onBluetoothOff() {
                            Timber.d("BT is off")
                            disposable.dispose()
                            super.onBluetoothOff()
                        }
                    }.also {
                        fitbitGatt.registerGattEventListener(it)
                    }
                }
                .doFinally { callback?.let { fitbitGatt.unregisterGattEventListener(it) } }
                .subscribe({ data -> updateResponseMessage(String(data)) }, { Timber.e(it) })
        )
    }

    private fun updateResponseMessage(message: String) {
        coap_response_time.visibility = View.VISIBLE
        val sendResponseTime = System.currentTimeMillis() - sendRequestTime
        coap_response_time.text = getString(R.string.label_response_time).format(sendResponseTime)
        rxtext.visibility = View.VISIBLE
        rxtext.text = getString(R.string.label_log_coap).format(message)
    }

    companion object {
        fun getIntent(context: Context): Intent {
            return Intent(context, CoapActivity::class.java)
        }
    }
}
