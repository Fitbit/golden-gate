package com.fitbit.remoteapi

import android.content.Context

val HOST_APP = "com.fitbit.goldengatehost"

object RemoteApiContext {
    fun isHostApp(context: Context) = context.applicationContext.packageName == HOST_APP
}