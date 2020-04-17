// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import android.content.Context

val HOST_APP = "com.fitbit.goldengatehost"

object RemoteApiContext {
    fun isHostApp(context: Context) = context.applicationContext.packageName == HOST_APP
}
