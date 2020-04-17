// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost

import android.content.Context
import android.net.ConnectivityManager
import android.net.wifi.WifiManager
import android.text.TextUtils
import timber.log.Timber
import java.net.Inet4Address
import java.net.NetworkInterface


fun getNetworkInterfaceIpAddress(): String {
    try {
        val en = NetworkInterface.getNetworkInterfaces()
        while (en.hasMoreElements()) {
            val networkInterface = en.nextElement()
            val enumIpAddr = networkInterface.inetAddresses
            while (enumIpAddr.hasMoreElements()) {
                val inetAddress = enumIpAddr.nextElement()
                if (!inetAddress.isLoopbackAddress && inetAddress is Inet4Address) {
                    val host = inetAddress.getHostAddress()
                    if (!TextUtils.isEmpty(host)) {
                        return host
                    }
                }
            }
        }
    } catch (ex: Exception) {
        Timber.e(ex, "getLocalIpAddress")
    }

    return ""
}

private fun getDeviceIpAddress(context: Context): String {
    var actualConnectedToNetwork: String = ""
    val connManager = context.getSystemService(Context.CONNECTIVITY_SERVICE)
    if (connManager is ConnectivityManager) {
        val wifi = connManager.getNetworkInfo(ConnectivityManager.TYPE_WIFI)
        if (wifi.isConnected) {
            actualConnectedToNetwork = getWifiIpAddress(context)
        }
    }
    if (actualConnectedToNetwork.isBlank()) {
        actualConnectedToNetwork = getNetworkInterfaceIpAddress()
    }
    if (TextUtils.isEmpty(actualConnectedToNetwork)) {
        actualConnectedToNetwork = "127.0.0.1"
    }
    return actualConnectedToNetwork
}

private fun getWifiIpAddress(context: Context): String {
    val wifiManager = context.applicationContext.getSystemService(Context.WIFI_SERVICE)
    if (wifiManager is WifiManager) {
        if (wifiManager.isWifiEnabled) {
            val ip = wifiManager.connectionInfo.ipAddress
            return ((ip and 0xFF).toString()
                + "."
                + (ip shr 8 and 0xFF)
                + "."
                + (ip shr 16 and 0xFF)
                + "."
                + (ip shr 24 and 0xFF))
        }
    }
    return ""
}
