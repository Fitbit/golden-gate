// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost.scan

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.support.v7.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import androidx.core.app.ActivityCompat
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengate.bindings.io.BLASTER_DEFAULT_PACKET_SIZE
import com.fitbit.goldengate.bindings.io.EXTRA_BLAST_PACKET_SIZE
import com.fitbit.goldengatehost.EXTRA_STACK_CONFIG
import com.fitbit.goldengatehost.EXTRA_IS_BLE_CENTRAL_ROLE
import com.fitbit.goldengatehost.EXTRA_SET_START_STACK_MTU
import com.fitbit.goldengatehost.R

const val EXTRA_LAUNCH_CLASS = "launchClass"
const val EXTRA_DEVICE = "extra_device"

class ScanActivity : AppCompatActivity(), ScanFragment.ScanListener {

    private lateinit var toolbar: Toolbar

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_scan)
        bindViews()
        setSupportActionBar(toolbar)
    }

    private fun bindViews() {
        toolbar = ActivityCompat.requireViewById(this, R.id.toolbar)
    }

    override fun onScanItemClick(connection: GattConnection) {
        startActivity(
            Intent(this, intent.getSerializableExtra(EXTRA_LAUNCH_CLASS) as Class<*>)
                .putExtra(EXTRA_DEVICE, connection.device.btDevice)
                .putExtra(EXTRA_STACK_CONFIG, intent.getSerializableExtra(EXTRA_STACK_CONFIG))
                .putExtra(EXTRA_BLAST_PACKET_SIZE, intent.getIntExtra(EXTRA_BLAST_PACKET_SIZE, BLASTER_DEFAULT_PACKET_SIZE))
                .putExtra(EXTRA_IS_BLE_CENTRAL_ROLE, intent.getBooleanExtra(EXTRA_IS_BLE_CENTRAL_ROLE, true))
                .putExtra(EXTRA_SET_START_STACK_MTU, intent.getBooleanExtra(EXTRA_SET_START_STACK_MTU, true))
        )
    }

    companion object {
        fun getIntent(context: Context): Intent {
            return Intent(context, ScanActivity::class.java)
        }
    }

}
