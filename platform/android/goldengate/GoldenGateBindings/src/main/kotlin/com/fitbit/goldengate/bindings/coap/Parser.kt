package com.fitbit.goldengate.bindings.coap

import io.reactivex.Single

/**
 * This can be used by the library to parse responses into a format the caller desires
 */
interface Parser<Input, Result> {

    /**
     * Parse an object of type [Input] into an object of type [Result]
     *
     * @param input is the incoming data to parse.
     */
    fun parse(input: Input): Single<Result>
}

/**
 * This interface is a specific subinterface of Parser that parses ByteArray objects
 */
interface ByteArrayParser<Result> : Parser<ByteArray, Result>
