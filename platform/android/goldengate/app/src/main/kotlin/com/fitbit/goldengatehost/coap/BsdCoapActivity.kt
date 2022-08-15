// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost.coap

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import android.view.View
import com.fitbit.goldengate.bindings.coap.CoapEndpoint
import com.fitbit.goldengate.bindings.coap.CoapEndpointProvider
import com.fitbit.goldengate.bindings.coap.data.IncomingResponse
import com.fitbit.goldengate.bindings.coap.data.Method
import com.fitbit.goldengate.bindings.coap.data.OutgoingRequestBuilder
import com.fitbit.goldengate.bindings.node.BsdSocketNodeKey
import com.fitbit.goldengate.bindings.sockets.bsd.BsdDatagramSocket
import com.fitbit.goldengate.bindings.sockets.bsd.BsdDatagramSocketAddress
import com.fitbit.goldengatehost.EXTRA_LOCAL_PORT
import com.fitbit.goldengatehost.EXTRA_REMOTE_IP
import com.fitbit.goldengatehost.EXTRA_REMOTE_PORT
import com.fitbit.goldengatehost.R
import com.fitbit.goldengatehost.coap.handler.addAllAvailableCoapHandlers
import com.fitbit.goldengatehost.coap.handler.removeAllAvailableCoapHandlers
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.schedulers.Schedulers
import kotlinx.android.synthetic.main.activity_bsd_coap.toolbar
import kotlinx.android.synthetic.main.content_bsd_coap.coap_message_text_view
import kotlinx.android.synthetic.main.content_bsd_coap.coap_mode_text_view
import kotlinx.android.synthetic.main.content_bsd_coap.get_hello_world_btn
import java.net.Inet4Address
import java.net.InetSocketAddress

class BsdCoapActivity : AppCompatActivity() {

    private lateinit var endpoint: CoapEndpoint
    private lateinit var bsdDatagramSocket: BsdDatagramSocket

    private val disposeBag = CompositeDisposable()

    private var localPort: Int = 0
    private var remotePort: Int = 0
    private lateinit var remoteIpAddress: String
    private lateinit var bsdDatagramSocketAddress: BsdDatagramSocketAddress

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_bsd_coap)
        setSupportActionBar(toolbar)

        getIntentData()
        setupUI()

        createEndpoint()
        addAllAvailableCoapHandlers(disposeBag, endpoint)
    }

    override fun onDestroy() {
        super.onDestroy()
        removeAllAvailableCoapHandlers(endpoint)
        disposeBag.dispose()
        bsdDatagramSocket.close()
        endpoint.close()
    }

    private fun getIntentData() {
        localPort = intent.getIntExtra(EXTRA_LOCAL_PORT, 0)
        remoteIpAddress = intent.getStringExtra(EXTRA_REMOTE_IP).toString()
        remotePort = intent.getIntExtra(EXTRA_REMOTE_PORT, 0)
    }

    private fun setupUI() {
        val isServerOnly = remoteIpAddress.isBlank()

        setCoapModeText(isServerOnly)

        if (!isServerOnly) {
            setupAvailableClientCalls()
        }
    }

    private fun setCoapModeText(isServerOnly: Boolean) {
        val mode = if (isServerOnly) {
            "Server Only"
        } else {
            "Client and Server Only"
        }
        coap_mode_text_view.text = "Coap Mode: $mode"
    }

    private fun setupAvailableClientCalls() {
        coap_message_text_view.visibility = View.VISIBLE
        get_hello_world_btn.visibility = View.VISIBLE
        get_hello_world_btn.setOnClickListener { setGetHelloWorldClickHandler() }
    }

    private fun createEndpoint() {
        createBsdDatagramSocket()
        endpoint = CoapEndpointProvider.getCoapEndpointInternal(BsdSocketNodeKey(bsdDatagramSocketAddress))
        endpoint.attach(bsdDatagramSocket)
    }

    private fun createBsdDatagramSocket() {

        var remoteSocketAddress: InetSocketAddress? = null
        if (remoteIpAddress.isNotBlank()) {
            val remoteInetAddress = Inet4Address.getByName(remoteIpAddress)
            remoteSocketAddress = InetSocketAddress(remoteInetAddress, remotePort)
        }

        bsdDatagramSocketAddress = BsdDatagramSocketAddress(localPort, remoteSocketAddress)
        bsdDatagramSocket = BsdDatagramSocket(bsdDatagramSocketAddress)
    }

    private fun setGetHelloWorldClickHandler() {
        disposeBag.add(
                endpoint.responseFor(OutgoingRequestBuilder("helloworld", Method.GET).build())
                        .subscribeOn(Schedulers.io())
                        .observeOn(AndroidSchedulers.mainThread())
                        .subscribe(
                                { response -> updateResponseMessage(response) },
                                { throwable ->
                                    updateResponseMessage(throwable.message
                                            ?: "Error requesting helloworld")
                                }
                        )
        )
    }

    private fun updateResponseMessage(response: IncomingResponse) {
        disposeBag.add(
                response.body.asData()
                        .subscribeOn(Schedulers.io())
                        .observeOn(AndroidSchedulers.mainThread())
                        .subscribe(
                                { data -> updateResponseMessage(String(data)) },
                                { throwable ->
                                    updateResponseMessage(throwable.message
                                            ?: "Error reading helloworld response")
                                }
                        )
        )
    }

    private fun updateResponseMessage(message: String) {
        coap_message_text_view.text = message
    }

    override fun onStop() {
        disposeBag.dispose()
        super.onStop()
    }
}
