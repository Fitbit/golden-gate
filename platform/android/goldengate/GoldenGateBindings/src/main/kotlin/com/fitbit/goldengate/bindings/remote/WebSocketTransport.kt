// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.bindings.remote

import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.WebSocket
import okhttp3.WebSocketListener
import okio.ByteString
import timber.log.Timber
import java.nio.ByteBuffer
import java.util.concurrent.ArrayBlockingQueue

/**
 * The native side of this class keeps a global reference to the instance on the C-side
 * Remember to call cleanup() when it's no longer needed.
 */
class WebSocketTransport(url: String, private val ignoreFailure: Boolean = false) : WebSocketListener() {

    private val webSocket: WebSocket
    val ptr: Long
    private val holder = ArrayBlockingQueue<ByteString>(1)

    @Volatile
    private var shouldClose = false

    init {
        val client = OkHttpClient()
        val request = Request.Builder().url(url).build()
        webSocket = client.newWebSocket(request, this)
        Timber.d("WebSocket created: opening in progress")
        ptr = createJNI()
        Timber.d("WebSocketTransport native side created")
    }

    fun send(data: ByteArray) : Boolean {
        Timber.v("Sending message of size ${data.size}")
        val buffer = ByteBuffer.wrap(data)
        return webSocket.send(ByteString.of(buffer))
    }

    /**
     * returns an empty array if the websocket is closed and the remote shell should terminate.
     */
    fun receive() : ByteArray {
        if (shouldClose) {
            return ByteArray(0)
        }
        val byteString = holder.take()
        return byteString.toByteArray()
    }

    /**
     * This should be called after the RemoteShell exits.
     */
    fun cleanup() {
        Timber.d("Cleaning up WebSocketTransport")
        destroyJNI()
        Timber.d("WebSocketTransport native side cleaned up")
    }

    override fun onFailure(webSocket: WebSocket?, t: Throwable?, response: Response?) {
        if (ignoreFailure) {
            //This is for tests when we don't want to have the websocket actually connect to anything.
            return;
        }
        Timber.w(t, "Websocket returned failure. Response: %s", response)
        shouldClose = true
        holder.offer(ByteString.EMPTY)
    }

    override fun onMessage(webSocket: WebSocket?, text: String?) {
        throw IllegalArgumentException("Unexpected string message from websocket")
    }

    override fun onMessage(webSocket: WebSocket?, bytes: ByteString?) {
        if (!holder.offer(bytes) && !shouldClose) {
            throw IllegalStateException("Two messages sent over websocket without a response")
        }
    }

    override fun onClosed(webSocket: WebSocket?, code: Int, reason: String?) {
        shouldClose = true
        holder.offer(ByteString.EMPTY)
        Timber.d("WebSocket closed")
    }

    private external fun createJNI(): Long
    private external fun destroyJNI(webSocketTransportPtr: Long = ptr)
}
