package com.fitbit.goldengate.bindings.coap.handler

import com.fitbit.goldengate.bindings.coap.data.IncomingRequest
import com.fitbit.goldengate.bindings.coap.data.IntOptionValue
import com.fitbit.goldengate.bindings.coap.data.OptionNumber
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponse
import com.fitbit.goldengate.bindings.coap.data.OutgoingResponseBuilder
import com.fitbit.goldengate.bindings.coap.data.ResponseCode.Companion.badRequest
import com.fitbit.goldengate.bindings.util.toBlockInfo
import io.reactivex.Single
import io.reactivex.android.schedulers.AndroidSchedulers
import timber.log.Timber

abstract class BlockwisePostResourceHandler: BaseResourceHandler(), BlockwiseBlock1Server {

    var isStarted = false
    var expetedNextOffset = 0

    override fun onPost(request: IncomingRequest, responseBuilder: OutgoingResponseBuilder): Single<OutgoingResponse> {
        return request.body.asData()
            .observeOn(AndroidSchedulers.mainThread())
            .map { data ->

                val options = request.options

                val block1Option = request.options.firstOrNull { it.number == OptionNumber.BLOCK1 }

                if (block1Option != null) {
                    val blockInfo = (block1Option.value as IntOptionValue).value.toBlockInfo()
                    Timber.w("block1 option: $blockInfo: $expetedNextOffset")

                    if (blockInfo.startOffset == 0 && !isStarted) {
                        onStart(options)
                        isStarted = true
                    }

                    var response: OutgoingResponse? = null

                    if (blockInfo.startOffset == expetedNextOffset && isStarted) {
                        response = onData(options, data)
                        expetedNextOffset += data.size
                    }

                    if (!blockInfo.moreBlock && isStarted) {
                        response = onEnd(request.options, true)
                        expetedNextOffset = 0
                        isStarted = false
                    }

                    response ?: responseBuilder.autogenerateBlockwiseConfig(true).build()

                } else {
                    responseBuilder
                        .responseCode(badRequest)
                        .build()
                }

            }
    }
}
