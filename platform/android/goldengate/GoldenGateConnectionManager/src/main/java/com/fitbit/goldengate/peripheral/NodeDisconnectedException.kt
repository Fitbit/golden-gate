// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengate.peripheral

class NodeDisconnectedException(
    message: String = "The node disconnected unexpectedly",
    cause: Throwable? = null
): Exception(message, cause)
