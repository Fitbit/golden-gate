// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller.ui

import android.bluetooth.BluetoothDevice
import android.os.Bundle
import android.view.KeyEvent
import android.view.View
import android.widget.Button
import android.widget.CheckBox
import android.widget.EditText
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import com.fitbit.linkcontroller.LinkController
import com.fitbit.linkcontroller.LinkControllerProvider
import com.fitbit.linkcontroller.R
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionConfiguration
import com.fitbit.linkcontroller.services.configuration.PreferredConnectionMode
import com.fitbit.linkcontroller.services.status.CurrentConnectionConfiguration
import com.fitbit.linkcontroller.services.status.CurrentConnectionStatus
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.schedulers.Schedulers
import timber.log.Timber


class LinkControllerActivity : AppCompatActivity() {


    private val disposeBag = CompositeDisposable()
    private lateinit var linkController: LinkController
    private val preferredConnectionConfigurationBuilder: PreferredConnectionConfiguration.Builder =
        PreferredConnectionConfiguration.Builder()

    private lateinit var applySettings: Button
    private lateinit var connectionBonded: TextView
    private lateinit var connectionDle: TextView
    private lateinit var connectionDleReboot: TextView
    private lateinit var connectionEncrypted: TextView
    private lateinit var connectionInterval: TextView
    private lateinit var connectionLatency: TextView
    private lateinit var connectionMaxInterval: EditText
    private lateinit var connectionMinInterval: EditText
    private lateinit var connectionMtu: TextView
    private lateinit var connectionSlaveLatency: EditText
    private lateinit var connectionSpeedMode: TextView
    private lateinit var connectionSupervisionTimeout: EditText
    private lateinit var connectionTimeout: TextView
    private lateinit var dlePduSize: EditText
    private lateinit var dleTime: EditText
    private lateinit var maxRxPayload: TextView
    private lateinit var maxRxTime: TextView
    private lateinit var maxTxPayload: TextView
    private lateinit var maxTxTime: TextView
    private lateinit var mtu: EditText
    private lateinit var rbFastMode: RadioButton
    private lateinit var requestDisconnect: CheckBox
    private lateinit var setFastMode: Button
    private lateinit var setSlowMode: Button
    private lateinit var speedModeConfiguration: RadioGroup

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.a_linkcontroller_setup)
        bindViews()

        val bluetoothDevice =
            intent.getParcelableExtra<BluetoothDevice>(EXTRA_DEVICE) as BluetoothDevice
        linkController = LinkControllerProvider.INSTANCE.getLinkController(bluetoothDevice)
        updateConnectionConfig(CurrentConnectionConfiguration.getDefaultConfiguration())
        updateConnectionStatus(CurrentConnectionStatus())

        setupListeners()
        speedModeConfiguration.setOnCheckedChangeListener { _, _ ->
            val preferredConnectionMode =
                if (rbFastMode.isChecked)
                    PreferredConnectionMode.FAST
                else
                    PreferredConnectionMode.SLOW
            disposeBag.add(linkController.setPreferredConnectionMode(preferredConnectionMode)
                .subscribeOn(Schedulers.io())
                .subscribe(
                    { Timber.i("Set Preferred Connection mode to $preferredConnectionMode for Device ${bluetoothDevice.address}") },
                    { t -> Timber.e(t, "Error setting Preferred Connection mode for Device ${bluetoothDevice.address}") }
                )
            )
        }

        disposeBag.add(
            linkController.subscribeToCurrentConnectionConfigurationNotifications()
                .andThen(linkController.subscribeToCurrentConnectionStatusNotifications())
                .subscribeOn(Schedulers.io())
                .subscribe(
                    { Timber.i("Successfully setup up LinkController subscriptions for Device ${bluetoothDevice.address}") },
                    { t -> Timber.e(t, "Error setting up LinkController subscriptions for Device ${bluetoothDevice.address}") }
                )
        )

        disposeBag.add(linkController.observeCurrentConnectionConfiguration()
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .subscribe(
                { updateConnectionConfig(it) },
                { t ->
                    Timber.e(t)
                    Toast.makeText(this, "error updating connection config", Toast.LENGTH_LONG)
                        .show()
                }
            ))
        disposeBag.add(linkController.observeCurrentConnectionStatus()
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .subscribe(
                { updateConnectionStatus(it) },
                { t->
                    Timber.e(t)
                    Toast.makeText(this, "error updating connection status", Toast.LENGTH_LONG)
                        .show()
                }
            )
        )

    }

    private fun bindViews() {
        applySettings = ActivityCompat.requireViewById(this, R.id.apply_settings)
        connectionBonded = ActivityCompat.requireViewById(this, R.id.connection_bonded)
        connectionDle = ActivityCompat.requireViewById(this, R.id.connection_dle)
        connectionDleReboot = ActivityCompat.requireViewById(this, R.id.connection_dle_reboot)
        connectionEncrypted = ActivityCompat.requireViewById(this, R.id.connection_encrypted)
        connectionInterval = ActivityCompat.requireViewById(this, R.id.connection_interval)
        connectionLatency = ActivityCompat.requireViewById(this, R.id.connection_latency)
        connectionMaxInterval = ActivityCompat.requireViewById(this, R.id.connection_max_interval)
        connectionMinInterval = ActivityCompat.requireViewById(this, R.id.connection_min_interval)
        connectionMtu = ActivityCompat.requireViewById(this, R.id.connection_mtu)
        connectionSlaveLatency = ActivityCompat.requireViewById(this, R.id.connection_slave_latency)
        connectionSpeedMode = ActivityCompat.requireViewById(this, R.id.connection_speed_mode)
        connectionSupervisionTimeout = ActivityCompat.requireViewById(this, R.id.connection_supervision_timeout)
        connectionTimeout = ActivityCompat.requireViewById(this, R.id.connection_timeout)
        dlePduSize = ActivityCompat.requireViewById(this, R.id.dle_pdu_size)
        dleTime = ActivityCompat.requireViewById(this, R.id.dle_time)
        maxRxPayload = ActivityCompat.requireViewById(this, R.id.max_rx_payload)
        maxRxTime = ActivityCompat.requireViewById(this, R.id.max_rx_time)
        maxTxPayload = ActivityCompat.requireViewById(this, R.id.max_tx_payload)
        maxTxTime = ActivityCompat.requireViewById(this, R.id.max_tx_time)
        mtu = ActivityCompat.requireViewById(this, R.id.mtu)
        rbFastMode = ActivityCompat.requireViewById(this, R.id.rb_fast_mode)
        requestDisconnect = ActivityCompat.requireViewById(this, R.id.request_disconnect)
        setFastMode = ActivityCompat.requireViewById(this, R.id.set_fast_mode)
        setSlowMode = ActivityCompat.requireViewById(this, R.id.set_slow_mode)
        speedModeConfiguration = ActivityCompat.requireViewById(this, R.id.speedModeConfiguration)
    }

    private fun setupListeners() {
        setFastMode.setOnClickListener {
            Timber.v("Set fast Config ")
            try {
                preferredConnectionConfigurationBuilder.setFastModeConfig(
                    connectionMinInterval.text.toString().toFloat(),
                    connectionMaxInterval.text.toString().toFloat(),
                    connectionSlaveLatency.text.toString().toInt(),
                    connectionSupervisionTimeout.text.toString().toInt()
                )
            } catch (e: IllegalArgumentException) {
                Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
            }


        }
        setSlowMode.setOnClickListener {
            Timber.v("Set slow Config ")
            try {
                preferredConnectionConfigurationBuilder.setSlowModeConfig(
                    connectionMinInterval.text.toString().toFloat(),
                    connectionMaxInterval.text.toString().toFloat(),
                    connectionSlaveLatency.text.toString().toInt(),
                    connectionSupervisionTimeout.text.toString().toInt()
                )
            } catch (e: IllegalArgumentException) {
                Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
            }

        }
        dlePduSize.setOnKeyListener(View.OnKeyListener { _, keyCode, event ->

            if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
                //Perform Code
                Timber.v("Set dlesize ${dlePduSize.text}")
                setDleConfig()
                return@OnKeyListener true
            }
            false
        })
        dleTime.setOnKeyListener(View.OnKeyListener { _, keyCode, event ->
            if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
                //Perform Code
                Timber.v("Set dletime ${dleTime.text}")
                setDleConfig()
                return@OnKeyListener true
            }
            false
        })
        mtu.setOnKeyListener(View.OnKeyListener { _, keyCode, event ->
            if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
                //Perform Code
                Timber.v("Set mtu ${mtu.text}")
                try {
                    preferredConnectionConfigurationBuilder.setMtu(mtu.text.toString().toShort())
                } catch (e: IllegalArgumentException) {
                    Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
                }
                return@OnKeyListener true
            }
            false
        })

        requestDisconnect.setOnCheckedChangeListener { _, isChecked ->
            Timber.v("requestDisconnect $isChecked")
            preferredConnectionConfigurationBuilder.requestDisconnect(isChecked)

        }
        applySettings.setOnClickListener {
            disposeBag.add(
                linkController.setPreferredConnectionConfiguration(preferredConnectionConfigurationBuilder.build())
                .subscribeOn(Schedulers.io())
                .subscribe(
                    { Timber.i("Set Preferred Connection configuration") },
                    { t -> Timber.e(t, "Error setting Preferred Connection configuration") }
                )
            )
        }
    }

    private fun setDleConfig() {
        if (dlePduSize.text.isNotBlank() && dleTime.text.isNotBlank()) {
            try {
                preferredConnectionConfigurationBuilder.setDLEConfig(
                    dlePduSize.text.toString().toInt(),
                    dleTime.text.toString().toInt()
                )
            } catch (e: IllegalArgumentException) {
                Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun updateConnectionStatus(currentConnectionStatus: CurrentConnectionStatus) {

        Timber.v("Update connection Status $currentConnectionStatus")
        connectionBonded.text = getString((R.string.bonded)).format(currentConnectionStatus.bonded)
        connectionEncrypted.text =
                getString((R.string.encrypted)).format(currentConnectionStatus.encrypted)
        connectionDle.text = getString((R.string.dle)).format(currentConnectionStatus.dleEnabled)
        connectionDleReboot.text =
                getString((R.string.dle_reboot)).format(currentConnectionStatus.dleReboot)

        maxTxPayload.text =
                getString((R.string.max_tx_payload)).format(currentConnectionStatus.maxTxPayload)
        maxTxTime.text =
                getString((R.string.max_tx_time)).format(currentConnectionStatus.maxTxTime)

        maxRxPayload.text =
                getString((R.string.max_rx_payload)).format(currentConnectionStatus.maxRxPayload)
        maxRxTime.text =
                getString((R.string.max_rx_time)).format(currentConnectionStatus.maxRxTime)

    }

    private fun updateConnectionConfig(currentConnectionConfiguration: CurrentConnectionConfiguration) {
        Timber.v("Update connection Config $currentConnectionConfiguration")
        connectionInterval.text =
                getString((R.string.connection_interval)).format(currentConnectionConfiguration.interval)
        connectionLatency.text =
                getString((R.string.connection_latency)).format(currentConnectionConfiguration.slaveLatency)
        connectionTimeout.text =
                getString((R.string.connection_timeout)).format(currentConnectionConfiguration.supervisionTimeout)
        connectionMtu.text =
                getString((R.string.connection_mtu)).format(currentConnectionConfiguration.mtu)
        connectionSpeedMode.text =
                getString((R.string.connection_speed_mode)).format(currentConnectionConfiguration.connectionMode.toString())

    }

    override fun onDestroy() {
        super.onDestroy()
        disposeBag.dispose()
    }
    companion object {
        val EXTRA_DEVICE = "extra_device"
    }
}
