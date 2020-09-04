package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.Data
import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.IntOptionValue
import com.fitbit.goldengate.bindings.coap.data.OptionNumber
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import com.fitbit.goldengate.bindings.coap.data.ResponseCode.Companion.badOption
import com.fitbit.goldengate.bindings.coap.data.ResponseCode.Companion.badRequest
import com.fitbit.goldengate.bindings.coap.data.error
import com.fitbit.goldengate.bindings.util.toBlockInfo
import io.reactivex.Completable
import io.reactivex.Single
import io.reactivex.disposables.Disposable
import timber.log.Timber
import java.util.concurrent.TimeUnit

abstract class BlockwisePostResourceHandler: BaseResourceHandler(), BlockwiseBlock1Server {

    private var isStarted = false
    private var currentOffset = 0
    private var expectedNextOffset = 0
    lateinit var requestOptions: Options
    private var timerDisposable: Disposable? = null

    companion object {
        const val TIMEOUT_MINUTES: Long = 3
    }

    override fun onPost(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
        return request.body.asData()
            .map { data ->
                requestOptions = request.options
                processBlock1Data(requestOptions, data, responseBuilder)
            }
    }

    private fun processBlock1Data(
        requestOptions: Options,
        data: Data,
        responseBuilder: OutgoingResponseBuilder
    ): OutgoingResponse {
        val block1Option = requestOptions.firstOrNull { it.number == OptionNumber.BLOCK1 }

        if (block1Option != null) {
            val blockInfo = (block1Option.value as IntOptionValue).value.toBlockInfo()
            Timber.d("block1 option: $blockInfo: $expectedNextOffset")

            // handle the first block
            if (blockInfo.startOffset == 0) {

                // if the previous request has not finished, terminate it first
                if (isStarted) {
                    resetState()
                    onEnd(requestOptions, false)
                }

                onStart(requestOptions)
                isStarted = true
            }

            var response: OutgoingResponse? = null

            // process data from each block
            when (blockInfo.startOffset) {
                expectedNextOffset -> {
                    response = onData(requestOptions, data)
                    currentOffset = blockInfo.startOffset
                    expectedNextOffset += data.size
                    startNewTimer()
                }
                currentOffset -> {
                    // skip the duplicated block
                }
                else -> {
                    // terminate the quest, if the incorrect offset has been received
                    Timber.d("Incorrect start offset, expected: $expectedNextOffset, actual: ${blockInfo.startOffset}")
                    response = onEnd(requestOptions, false) ?: OutgoingResponseBuilder()
                        .responseCode(badOption)
                        .build()
                }
            }

            if (response != null && response.responseCode.error()) {
                resetState()
            }

            // handle the last block
            if ((!blockInfo.moreBlock && isStarted)) {
                response = onEnd(requestOptions, true)
                resetState()
            }

            return response ?: responseBuilder.autogenerateBlockwiseConfig(true).build()

        } else {
            return responseBuilder
                .responseCode(badRequest)
                .build()
        }
    }

    /**
     * reset the CoAP request timer
     */
    private fun cleanupTimer() {
        // clean up the old timer
        timerDisposable?.dispose()
    }

    /**
     * starts the CoAP request timer
     */
    private fun startNewTimer() {
        // clean up the old timer
        cleanupTimer()

        // start a new timer
        timerDisposable = Completable
            .timer(TIMEOUT_MINUTES, TimeUnit.MINUTES)
            .subscribe(
                {
                    Timber.d("Coap request timer expired")
                    onEnd(requestOptions, false)
                },
                { Timber.e("$it") }
            )
    }

    private fun resetState() {
        expectedNextOffset = 0
        currentOffset = 0
        isStarted = false
        cleanupTimer()
    }
}
