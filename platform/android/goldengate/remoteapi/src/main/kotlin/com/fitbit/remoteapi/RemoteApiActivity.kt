// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.remoteapi

import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import com.fitbit.goldengate.GoldenGateConnectionManagerModule
import com.fitbit.goldengate.bindings.remote.RemoteShellThread
import com.fitbit.goldengate.bindings.remote.WebSocketTransport
import com.fitbit.goldengate.bindings.services.BlastService
import com.fitbit.goldengate.bindings.services.CoapServices
import com.fitbit.linkcontroller.LinkControllerProvider
import com.fitbit.remoteapi.handlers.ConfigurationRpc
import com.fitbit.remoteapi.handlers.ConnectRpc
import com.fitbit.remoteapi.handlers.ConnectionModeRpc
import com.fitbit.remoteapi.handlers.SetTlsKeyRpc
import com.fitbit.remoteapi.handlers.StartPairingRpc
import com.fitbit.remoteapi.handlers.VersionRpc
import io.reactivex.disposables.CompositeDisposable
import timber.log.Timber

class RemoteApiActivity : AppCompatActivity() {

    private val EXTRA_URL = "EXTRA_URL"
    private val IS_CENTRAL = "IS_CENTRAL"

    private val compositeDisposable: CompositeDisposable = CompositeDisposable()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val isBleCentralRole: Boolean = intent.getBooleanExtra(IS_CENTRAL, true)
        GoldenGateConnectionManagerModule.init(applicationContext, true, isBleCentralRole)

        setContentView(R.layout.a_remote_api)
        val url = intent.getStringExtra(EXTRA_URL)
        if (url == null) {
            Timber.e("Please pass a URL with the extra name \"EXTRA_URL\"")
            finish()
            return
        }

        val bluetoothProvider = BluetoothProvider()

        val remoteShellThread = RemoteShellThread(WebSocketTransport(url))

        val packageInfo = packageManager.getPackageInfo(packageName, 0)

        val remoteApiConfigurationState =
                RemoteApiConfigurationState(bluetoothProvider, compositeDisposable)

        remoteShellThread.apply {
            registerHandler(VersionRpc(packageInfo.versionName, packageInfo.versionCode))
            registerHandler(
                ConfigurationRpc(
                    remoteApiConfigurationState,
                    CoapServices.Provider(this),
                    BlastService.Provider(this)
                )
            )
            if (RemoteApiContext.isHostApp(applicationContext)) {
                registerHandler(ConnectRpc(applicationContext, remoteApiConfigurationState))
                registerHandler(StartPairingRpc(applicationContext, remoteApiConfigurationState))
            }
            registerHandler(SetTlsKeyRpc())
            registerHandler(
                ConnectionModeRpc(
                    applicationContext,
                    LinkControllerProvider.INSTANCE,
                    bluetoothProvider,
                    remoteApiConfigurationState
                )
            )
            RemoteApiExternalHandlers.handlers.forEach(this::registerHandler)
            start()
        }
    }

    override fun onStop() {
        compositeDisposable.clear()
        super.onStop()
    }
}
