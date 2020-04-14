package com.fitbit.remoteapi.handlers

import co.nstant.`in`.cbor.CborEncoder
import co.nstant.`in`.cbor.model.SimpleValue
import com.fitbit.remoteapi.StagedCborHandler
import java.io.ByteArrayOutputStream

class NullResponder<T> : StagedCborHandler.Responder<T> {

    override fun respondWith(responseObject: T): ByteArray {
        val cborReturnValueStream = ByteArrayOutputStream()
        CborEncoder(cborReturnValueStream).encode(SimpleValue.NULL)
        return cborReturnValueStream.toByteArray()
    }
}
