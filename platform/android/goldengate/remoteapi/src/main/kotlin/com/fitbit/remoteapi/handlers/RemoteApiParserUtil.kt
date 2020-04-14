package com.fitbit.remoteapi.handlers

import co.nstant.`in`.cbor.model.Map
import co.nstant.`in`.cbor.model.UnicodeString
import com.fitbit.remoteapi.PeerId
import com.fitbit.remoteapi.PeerIdType

object RemoteApiParserUtil {
    fun getPeerId(paramMap: Map?): PeerId = PeerId(getIdField(paramMap), getTypeField(paramMap))

    private fun getIdField(paramMap: Map?): String {
        return getFieldFromStruct(paramMap, "peer", "id")
    }

    private fun getTypeField(paramMap: Map?): PeerIdType {
        return PeerIdType.fromString(getFieldFromStruct(paramMap, "peer", "type"))
    }

    private fun getFieldFromStruct(
        paramMap: Map?,
        structFieldName: String,
        fieldName: String
    ): String =
        paramMap?.get(UnicodeString(structFieldName))
            ?.let { it as? Map }
            ?.get(UnicodeString(fieldName))
            ?.let { it as? UnicodeString }
            ?.string
            ?: throw IllegalArgumentException("Must pass $fieldName with key'$structFieldName' ")
}