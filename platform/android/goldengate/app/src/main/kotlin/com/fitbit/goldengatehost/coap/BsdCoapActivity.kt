// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost.coap

import android.os.Bundle
import android.view.View
import android.widget.Button
import android.widget.TextView
import android.support.v7.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.app.ActivityCompat
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
    private lateinit var coapMessageTextView: TextView
    private lateinit var coapModeTextView: TextView
    private lateinit var getHelloWorldBtn: Button
    private lateinit var toolbar: Toolbar

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_bsd_coap)
        bindViews()
        setSupportActionBar(toolbar)

        getIntentData()
        setupUI()

        createEndpoint()
        addAllAvailableCoapHandlers(disposeBag, endpoint)
    }

    private fun bindViews() {
        coapMessageTextView = ActivityCompat.requireViewById(this, R.id.coap_message_text_view)
        coapModeTextView = ActivityCompat.requireViewById(this, R.id.coap_mode_text_view)
        getHelloWorldBtn = ActivityCompat.requireViewById(this, R.id.get_hello_world_btn)
        toolbar = ActivityCompat.requireViewById(this, R.id.toolbar)
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
        coapModeTextView.text = "Coap Mode: $mode"
    }

    private fun setupAvailableClientCalls() {
        coapMessageTextView.visibility = View.VISIBLE
        getHelloWorldBtn.visibility = View.VISIBLE
        getHelloWorldBtn.setOnClickListener { setGetHelloWorldClickHandler() }
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
        coapMessageTextView.text = message
    }

    override fun onStop() {
        disposeBag.dispose()
        super.onStop()
    }
}
