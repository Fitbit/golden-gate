package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.BlockInfo
import com.fitbit.goldengate.bindings.coap.data.Data
import com.fitbit.goldengate.bindings.coap.data.IntOptionValue
import com.fitbit.goldengate.bindings.coap.data.OptionNumber
import com.fitbit.goldengate.bindings.coap.data.Options
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import com.fitbit.goldengate.bindings.coap.data.ResponseCode
import com.fitbit.goldengate.bindings.coap.data.error
import com.fitbit.goldengate.bindings.util.toBlockInfo
import io.reactivex.Completable
import io.reactivex.disposables.Disposable
import timber.log.Timber
import java.util.concurrent.TimeUnit

/**
 * Helper class to process blockwise CoAP request with Block1 option
 * this class will invoke [BlockwiseBlock1Server] method to provide data to client
 */
class Block1RequestProcessHelper {
    private var isStarted = false
    private var currentOffset = 0
    private var expectedNextOffset = 0
    private var timerDisposable: Disposable? = null

    companion object {
        const val TIMEOUT_MINUTES: Long = 3
    }

    /**
     * handles the blockwise request with block1 option
     *
     * when receiving a new block, we might run into the following cases:
     *  1. receive the first block from a clean start
     *      --> onStart() + onData()
     *  2. receive the first block and the total block number is one
     *      --> onStart() + onData() + onEnd()
     *  3. terminate the previous transaction and handle the first block
     *      --> onEnd() + onStart() + onData()
     *  4. receive the second to N-1 block
     *      --> onData()
     *  5. the last (Nth) block
     *      --> onData() + onEnd()
     */
    fun processBlock1Request(
        block1Server: BlockwiseBlock1Server,
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
                handleFirstBlock(block1Server, requestOptions)
            }

            // process data from each block
            var response = processData(block1Server, requestOptions, blockInfo, data)

            // check the response to see if the blockwise transfer should be early terminated
            if (response != null && response.responseCode.error()) {
                clearTimer()
                resetState()
            }

            // handle the last block (the first block could be the last block)
            if (!blockInfo.moreBlock && isStarted) {
                response = handleLastBlock(block1Server, requestOptions)
            }

            return response ?: responseBuilder.autogenerateBlockwiseConfig(true).build()

        } else {
            // return error response if the request doesn't contain block1 option
            return responseBuilder
                .responseCode(ResponseCode.badRequest)
                .build()
        }
    }

    private fun handleFirstBlock(block1Server: BlockwiseBlock1Server, requestOptions: Options) {
        // if the previous request has not finished, terminate it first
        if (isStarted) {
            clearTimer()
            resetState()
            block1Server.onEnd(requestOptions, false)
        }

        block1Server.onStart(requestOptions)
        isStarted = true
    }

    private fun processData(
        block1Server: BlockwiseBlock1Server,
        requestOptions: Options,
        blockInfo: BlockInfo,
        data: Data
    ): OutgoingResponse? {
        var response: OutgoingResponse? = null
        when (blockInfo.startOffset) {
            expectedNextOffset -> {
                response = block1Server.onData(requestOptions, data)
                currentOffset = blockInfo.startOffset
                expectedNextOffset += data.size
                startNewTimer(block1Server, requestOptions)
            }
            currentOffset -> {
                // skip the duplicated block
            }
            else -> {
                // terminate the quest, if the incorrect offset has been received
                Timber.d("Incorrect start offset, expected: $expectedNextOffset, actual: ${blockInfo.startOffset}")
                response = block1Server.onEnd(requestOptions, false) ?: OutgoingResponseBuilder()
                    .responseCode(ResponseCode.badOption)
                    .build()
            }
        }
        return response
    }

    private fun handleLastBlock(
        block1Server: BlockwiseBlock1Server,
        requestOptions: Options
    ): OutgoingResponse? {
        clearTimer()
        resetState()
        return block1Server.onEnd(requestOptions, true)
    }

    /**
     * starts the CoAP request timer
     */
    private fun startNewTimer(block1Server: BlockwiseBlock1Server, requestOptions: Options) {
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
                        block1Server.onEnd(requestOptions, false)
                    }
                },
                { Timber.e("$it") }
            )
    }

    private fun clearTimer() {
        // clean up the old timer
        timerDisposable?.dispose()
    }

    private fun resetState() {
        expectedNextOffset = 0
        currentOffset = 0
        isStarted = false
    }
}
