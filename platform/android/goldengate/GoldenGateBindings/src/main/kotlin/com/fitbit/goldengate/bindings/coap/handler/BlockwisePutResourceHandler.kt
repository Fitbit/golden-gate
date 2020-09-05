package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.BlockInfo
import com.fitbit.goldengate.bindings.coap.data.Data
import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.IntOptionValue
import com.fitbit.goldengate.bindings.coap.data.OptionNumber
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import com.fitbit.goldengate.bindings.coap.data.ResponseCode
import com.fitbit.goldengate.bindings.coap.data.error
import com.fitbit.goldengate.bindings.util.toBlockInfo
import io.reactivex.Completable
import io.reactivex.Single
import io.reactivex.disposables.Disposable
import timber.log.Timber
import java.util.concurrent.TimeUnit

/**
 * Implementation of resource handler to handle the request with block1 option with Put method
 * The subclass needs to implement [BlockwiseBlock1Server] to receive blockwise request payload
 */
abstract class BlockwisePutResourceHandler: BaseResourceHandler(), BlockwiseBlock1Server {
    private var isStarted = false
    private var currentOffset = 0
    private var expectedNextOffset = 0
    lateinit var requestOptions: Options
    private var timerDisposable: Disposable? = null

    companion object {
        const val TIMEOUT_MINUTES: Long = 3
    }

    override fun onPut(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
        return request.body.asData()
            .map { data ->
                requestOptions = request.options
                processBlock1Request(requestOptions, data, responseBuilder)
            }
    }

    private fun processBlock1Request(
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
                handleFirstBlock()
            }

            // process data from each block
            var response = processData(blockInfo, data)

            // check the response to see if the blockwise transfer should be early terminated
            if (response != null && response.responseCode.error()) {
                resetTimer()
                resetState()
            }

            // handle the last block (the first block could be the last block)
            if (!blockInfo.moreBlock && isStarted) {
                response = handleLastBlock()
            }

            return response ?: responseBuilder.autogenerateBlockwiseConfig(true).build()

        } else {
            // return error response if the request doesn't contain block1 option
            return responseBuilder
                .responseCode(ResponseCode.badRequest)
                .build()
        }
    }

    private fun handleFirstBlock() {
        // if the previous request has not finished, terminate it first
        if (isStarted) {
            resetTimer()
            resetState()
            onEnd(requestOptions, false)
        }

        onStart(requestOptions)
        isStarted = true
    }

    private fun processData(blockInfo: BlockInfo, data: Data): OutgoingResponse? {
        var response: OutgoingResponse? = null
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
                    .responseCode(ResponseCode.badOption)
                    .build()
            }
        }
        return response
    }

    private fun handleLastBlock(): OutgoingResponse? {
        resetTimer()
        resetState()
        return onEnd(requestOptions, true)
    }

    /**
     * starts the CoAP request timer
     */
    private fun startNewTimer() {
        // clean up the old timer
        timerDisposable?.dispose()

        // start a new timer
        timerDisposable = Completable
            .timer(TIMEOUT_MINUTES, TimeUnit.MINUTES)
            .subscribe(
                {
                    Timber.d("Coap request timer expired")
                    if (isStarted) {
                        resetState()
                        onEnd(requestOptions, false)
                    }
                },
                { Timber.e("$it") }
            )
    }

    private fun resetTimer() {
        // clean up the old timer
        timerDisposable?.dispose()
    }

    private fun resetState() {
        expectedNextOffset = 0
        currentOffset = 0
        isStarted = false
    }
}
