package com.fitbit.goldengate.peripheral

class NodeDisconnectedException(
    message: String = "The node disconnected unexpectedly",
    cause: Throwable? = null
): Exception(message, cause)
