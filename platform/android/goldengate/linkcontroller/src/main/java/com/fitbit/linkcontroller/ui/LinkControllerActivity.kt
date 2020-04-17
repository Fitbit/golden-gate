// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.linkcontroller.ui

import android.bluetooth.BluetoothDevice
import android.os.Bundle
import androidx.appcompat.app.AppCompatActivity
import android.view.KeyEvent
import android.view.View
import android.widget.Toast
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
import kotlinx.android.synthetic.main.a_linkcontroller_setup.apply_settings
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_bonded
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_dle
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_dle_reboot
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_encrypted
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_interval
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_latency
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_max_interval
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_min_interval
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_mtu
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_slave_latency
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_speed_mode
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_supervision_timeout
import kotlinx.android.synthetic.main.a_linkcontroller_setup.connection_timeout
import kotlinx.android.synthetic.main.a_linkcontroller_setup.dle_pdu_size
import kotlinx.android.synthetic.main.a_linkcontroller_setup.dle_time
import kotlinx.android.synthetic.main.a_linkcontroller_setup.max_rx_payload
import kotlinx.android.synthetic.main.a_linkcontroller_setup.max_rx_time
import kotlinx.android.synthetic.main.a_linkcontroller_setup.max_tx_payload
import kotlinx.android.synthetic.main.a_linkcontroller_setup.max_tx_time
import kotlinx.android.synthetic.main.a_linkcontroller_setup.mtu
import kotlinx.android.synthetic.main.a_linkcontroller_setup.rb_fast_mode
import kotlinx.android.synthetic.main.a_linkcontroller_setup.request_disconnect
import kotlinx.android.synthetic.main.a_linkcontroller_setup.set_fast_mode
import kotlinx.android.synthetic.main.a_linkcontroller_setup.set_slow_mode
import kotlinx.android.synthetic.main.a_linkcontroller_setup.speedModeConfiguration
import timber.log.Timber


class LinkControllerActivity : AppCompatActivity() {


    private val disposeBag = CompositeDisposable()
    private lateinit var linkController: LinkController
    private val preferredConnectionConfigurationBuilder: PreferredConnectionConfiguration.Builder =
        PreferredConnectionConfiguration.Builder()


    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        setContentView(R.layout.a_linkcontroller_setup)
        val bluetoothDevice =
            intent.getParcelableExtra(EXTRA_DEVICE) as BluetoothDevice
        val controller = LinkControllerProvider.INSTANCE.getLinkController(bluetoothDevice)
        if (controller == null) {
            Toast.makeText(
                this,
                "Device ${bluetoothDevice.address} not known to bitgatt",
                Toast.LENGTH_LONG
            ).show()
            finish()
            return
        }
        linkController = controller
        updateConnectionConfig(CurrentConnectionConfiguration.getDefaultConfiguration())
        updateConnectionStatus(CurrentConnectionStatus())

        setupListeners()
        speedModeConfiguration.setOnCheckedChangeListener { _, _ ->
            val preferredConnectionMode =
                if (rb_fast_mode.isChecked)
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

    private fun setupListeners() {
        set_fast_mode.setOnClickListener {
            Timber.v("Set fast Config ")
            try {
                preferredConnectionConfigurationBuilder.setFastModeConfig(
                    connection_min_interval.text.toString().toFloat(),
                    connection_max_interval.text.toString().toFloat(),
                    connection_slave_latency.text.toString().toInt(),
                    connection_supervision_timeout.text.toString().toInt()
                )
            } catch (e: IllegalArgumentException) {
                Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
            }


        }
        set_slow_mode.setOnClickListener {
            Timber.v("Set slow Config ")
            try {
                preferredConnectionConfigurationBuilder.setSlowModeConfig(
                    connection_min_interval.text.toString().toFloat(),
                    connection_max_interval.text.toString().toFloat(),
                    connection_slave_latency.text.toString().toInt(),
                    connection_supervision_timeout.text.toString().toInt()
                )
            } catch (e: IllegalArgumentException) {
                Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
            }

        }
        dle_pdu_size.setOnKeyListener(View.OnKeyListener { _, keyCode, event ->

            if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
                //Perform Code
                Timber.v("Set dlesize ${dle_pdu_size.text}")
                setDleConfig()
                return@OnKeyListener true
            }
            false
        })
        dle_time.setOnKeyListener(View.OnKeyListener { _, keyCode, event ->
            if (keyCode == KeyEvent.KEYCODE_ENTER && event.action == KeyEvent.ACTION_UP) {
                //Perform Code
                Timber.v("Set dletime ${dle_time.text}")
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

        request_disconnect.setOnCheckedChangeListener { _, isChecked ->
            Timber.v("requestDisconnect $isChecked")
            preferredConnectionConfigurationBuilder.requestDisconnect(isChecked)

        }
        apply_settings.setOnClickListener {
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
        if (dle_pdu_size.text.isNotBlank() && dle_time.text.isNotBlank()) {
            try {
                preferredConnectionConfigurationBuilder.setDLEConfig(
                    dle_pdu_size.text.toString().toInt(),
                    dle_time.text.toString().toInt()
                )
            } catch (e: IllegalArgumentException) {
                Toast.makeText(this, e.message, Toast.LENGTH_LONG).show()
            }
        }
    }

    private fun updateConnectionStatus(currentConnectionStatus: CurrentConnectionStatus) {

        Timber.v("Update connection Status $currentConnectionStatus")
        connection_bonded.text = getString((R.string.bonded)).format(currentConnectionStatus.bonded)
        connection_encrypted.text =
                getString((R.string.encrypted)).format(currentConnectionStatus.encrypted)
        connection_dle.text = getString((R.string.dle)).format(currentConnectionStatus.dleEnabled)
        connection_dle_reboot.text =
                getString((R.string.dle_reboot)).format(currentConnectionStatus.dleReboot)

        max_tx_payload.text =
                getString((R.string.max_tx_payload)).format(currentConnectionStatus.maxTxPayload)
        max_tx_time.text =
                getString((R.string.max_tx_time)).format(currentConnectionStatus.maxTxTime)

        max_rx_payload.text =
                getString((R.string.max_rx_payload)).format(currentConnectionStatus.maxRxPayload)
        max_rx_time.text =
                getString((R.string.max_rx_time)).format(currentConnectionStatus.maxRxTime)

    }

    private fun updateConnectionConfig(currentConnectionConfiguration: CurrentConnectionConfiguration) {
        Timber.v("Update connection Config $currentConnectionConfiguration")
        connection_interval.text =
                getString((R.string.connection_interval)).format(currentConnectionConfiguration.interval)
        connection_latency.text =
                getString((R.string.connection_latency)).format(currentConnectionConfiguration.slaveLatency)
        connection_timeout.text =
                getString((R.string.connection_timeout)).format(currentConnectionConfiguration.supervisionTimeout)
        connection_mtu.text =
                getString((R.string.connection_mtu)).format(currentConnectionConfiguration.mtu)
        connection_speed_mode.text =
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
