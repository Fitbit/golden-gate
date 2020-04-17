// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import com.fitbit.goldengate.bindings.remote.CborHandler

object RemoteApiExternalHandlers {
    val handlers : MutableList<CborHandler> = mutableListOf()
}
