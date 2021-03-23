package com.fitbit.goldengate.bindings

open class GoldenGateNativeException(
    val title: String,
    val nakCode: Int,
    val nakNameSpace: String = "gg.result",
    val nakMessage: String? = null,
    cause: Throwable? = null
): Throwable(title, cause)
