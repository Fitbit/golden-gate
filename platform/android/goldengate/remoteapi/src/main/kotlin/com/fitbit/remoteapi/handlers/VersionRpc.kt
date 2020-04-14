package com.fitbit.remoteapi.handlers

import co.nstant.`in`.cbor.CborBuilder
import co.nstant.`in`.cbor.CborEncoder
import com.fitbit.goldengate.bindings.remote.CborHandler
import com.fitbit.remoteapi.StagedCborHandler
import java.io.ByteArrayOutputStream

class VersionRpc private constructor() {

    data class VersionInfo(val version: String, val build: Int)

    companion object Handler {

        operator fun invoke(version: String, build: Int): CborHandler {
            return StagedCborHandler(
                    "version",
                    StagedCborHandler.Stages(
                            StagedCborHandler.UnitParser(),
                            Processor(VersionInfo(version, build)),
                            Responder()
                    )
            )
        }
    }

    class Processor(private val versionInfo: VersionInfo) : StagedCborHandler.Processor<Unit, VersionInfo> {

        override fun process(parameterObject: Unit): VersionInfo {
            return versionInfo
        }
    }

    class Responder : StagedCborHandler.Responder<VersionInfo> {

        override fun respondWith(responseObject: VersionInfo): ByteArray {
            val cborReturnValueStream = ByteArrayOutputStream()
            val encoder = CborEncoder(cborReturnValueStream)
            val builder = CborBuilder()
            val builtStructure = builder
                    .addMap()
                    .put("version", responseObject.version)
                    .put("build", responseObject.build.toLong())
                    .end()
                    .build()
            encoder.encode(builtStructure)
            return cborReturnValueStream.toByteArray()
        }
    }
}
