// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import android.support.v7.app.AppCompatActivity
import android.support.v7.widget.SwitchCompat
import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Button
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.RelativeLayout
import android.widget.Spinner
import android.widget.Switch
import android.widget.Toast
import com.fitbit.goldengate.GoldenGateConnectionManagerModule
import com.fitbit.goldengate.bindings.coap.COAP_DEFAULT_PORT
import com.fitbit.goldengate.bindings.io.EXTRA_BLAST_PACKET_SIZE
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.GattlinkStackConfig
import com.fitbit.goldengate.bindings.stack.SocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bt.DEFAULT_CLIENT_ADDRESS
import com.fitbit.goldengate.bt.DEFAULT_SERVER_ADDRESS
import com.fitbit.goldengatehost.coap.BsdCoapActivity
import com.fitbit.goldengatehost.scan.EXTRA_LAUNCH_CLASS
import com.fitbit.goldengatehost.scan.ScanActivity
import timber.log.Timber
import java.net.Inet4Address
import java.util.concurrent.atomic.AtomicBoolean

/**
 * A screen for configuring the Host App
 */
class SetupActivity : AppCompatActivity(), AdapterView.OnItemSelectedListener {

    companion object {
        private val packetSizeOptions = arrayOf("32", "64", "128", "256", "512", "1024")
        private var packetSizeIndex = packetSizeOptions.lastIndex
    }

    private val initialized = AtomicBoolean()
    private lateinit var containerEditIp: RelativeLayout
    private lateinit var editLocalIp: EditText
    private lateinit var editLocalPort: EditText
    private lateinit var editRemoteIp: EditText
    private lateinit var editRemotePort: EditText
    private lateinit var packetSizeLayout: LinearLayout
    private lateinit var packetSizeSpinner: Spinner
    private lateinit var radioGroupBleRole: RadioGroup
    private lateinit var radioGroupConfiguration: RadioGroup
    private lateinit var radioGroupService: RadioGroup
    private lateinit var rbBlaster: RadioButton
    private lateinit var rbBsdSocketCoap: RadioButton
    private lateinit var rbCentral: RadioButton
    private lateinit var rbCoap: RadioButton
    private lateinit var rbGattlink: RadioButton
    private lateinit var rbGlUdp: RadioButton
    private lateinit var rbGlUdpDtls: RadioButton
    private lateinit var startButton: Button
    private lateinit var setStackStartMtuSwitch: Switch

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.a_setup)
        bindViews()

        respondToSettingsChange()
        setPacketSizeSpinner()

        radioGroupConfiguration.setOnCheckedChangeListener { _, _ -> respondToSettingsChange() }
        radioGroupService.setOnCheckedChangeListener { _, _ -> respondToSettingsChange() }
        radioGroupBleRole.setOnCheckedChangeListener { _, _ -> respondToSettingsChange()  }

        startButton.setOnClickListener { startMainActivity() }
    }

    private fun bindViews() {
        containerEditIp = ActivityCompat.requireViewById(this, R.id.container_edit_ip)
        editLocalIp = ActivityCompat.requireViewById(this, R.id.edit_local_ip)
        editLocalPort = ActivityCompat.requireViewById(this, R.id.edit_local_port)
        editRemoteIp = ActivityCompat.requireViewById(this, R.id.edit_remote_ip)
        editRemotePort = ActivityCompat.requireViewById(this, R.id.edit_remote_port)
        packetSizeLayout = ActivityCompat.requireViewById(this, R.id.packet_size_layout)
        packetSizeSpinner = ActivityCompat.requireViewById(this, R.id.packet_size_spinner)
        radioGroupBleRole = ActivityCompat.requireViewById(this, R.id.radioGroupBleRole)
        radioGroupConfiguration = ActivityCompat.requireViewById(this, R.id.radioGroupConfiguration)
        radioGroupService = ActivityCompat.requireViewById(this, R.id.radioGroupService)
        rbBlaster = ActivityCompat.requireViewById(this, R.id.rb_blaster)
        rbBsdSocketCoap = ActivityCompat.requireViewById(this, R.id.rb_bsd_socket_coap)
        rbCentral = ActivityCompat.requireViewById(this, R.id.rb_central)
        rbCoap = ActivityCompat.requireViewById(this, R.id.rb_coap)
        rbGattlink = ActivityCompat.requireViewById(this, R.id.rb_gattlink)
        rbGlUdp = ActivityCompat.requireViewById(this, R.id.rb_gl_udp)
        rbGlUdpDtls = ActivityCompat.requireViewById(this, R.id.rb_gl_udp_dtls)
        startButton = ActivityCompat.requireViewById(this, R.id.start_button)
        setStackStartMtuSwitch = ActivityCompat.requireViewById(this, R.id.set_stack_start_mtu)
    }

    private fun setPacketSizeSpinner() {
        val spinner = packetSizeSpinner
        val arrayAdaptor = ArrayAdapter(this, android.R.layout.simple_spinner_item, packetSizeOptions)
        arrayAdaptor.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinner.adapter = arrayAdaptor
        spinner.setSelection(packetSizeIndex)
        spinner.onItemSelectedListener = this
    }

    override fun onResume() {
        super.onResume()

        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(android.Manifest.permission.ACCESS_COARSE_LOCATION), REQ_ID)
        }
    }

    override fun onNothingSelected(parent: AdapterView<*>?) {
    }

    override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
        packetSizeIndex = position
    }

    private fun respondToSettingsChange() {
        editLocalPort.setText("")
        editRemotePort.setText("")

        //Checked state for this switch means client mode
        radioGroupService.visibility = when {
            rbBsdSocketCoap.isChecked -> GONE
            else -> {
                editLocalIp.isEnabled = true
                VISIBLE
            }
        }

        radioGroupBleRole.visibility = when {
            rbBsdSocketCoap.isChecked -> GONE
            else -> VISIBLE
        }

        if (rbGattlink.isChecked) {
            when { rbCoap.isChecked -> rbBlaster.isChecked = true }
            containerEditIp.visibility = GONE
            rbCoap.isEnabled = false
        } else {
            containerEditIp.visibility = VISIBLE
            if (rbBsdSocketCoap.isChecked) {
                editRemotePort.setText(COAP_DEFAULT_PORT.toString())
                editRemoteIp.setText("")
                editLocalPort.setText(COAP_DEFAULT_PORT.toString())
                // local ip is not editable in Bsd socket mode
                editLocalIp.setText(getNetworkInterfaceIpAddress())
                editLocalIp.isEnabled = false
            } else {
                if (rbCentral.isChecked) {
                    editLocalIp.setText(DEFAULT_SERVER_ADDRESS)
                    editRemoteIp.setText(DEFAULT_CLIENT_ADDRESS)
                } else {
                    editLocalIp.setText(DEFAULT_CLIENT_ADDRESS)
                    editRemoteIp.setText(DEFAULT_SERVER_ADDRESS)
                }
                rbCoap.isEnabled = true
            }
        }

        packetSizeLayout.visibility = when {
            rbBlaster.isChecked -> VISIBLE
            else -> GONE
        }
    }

    private fun startMainActivity() {

        if (!initialized.getAndSet(true)) {
            GoldenGateConnectionManagerModule.init(applicationContext, loggingEnabled=true, isBleCentralRole=rbCentral.isChecked)
        }

        if (rbBsdSocketCoap.isChecked) {
            startBsdCoapActivity()
            return
        }

        val intent: Intent = when {
            // central role
            rbCentral.isChecked -> when {
                rbCoap.isChecked -> ScanActivity.getIntent(this).putExtra(EXTRA_LAUNCH_CLASS, CoapActivity::class.java)
                rbBlaster.isChecked -> {
                    ScanActivity.getIntent(this)
                        .putExtra(EXTRA_LAUNCH_CLASS, BlastActivity::class.java)
                        .putExtra(EXTRA_BLAST_PACKET_SIZE, packetSizeOptions[packetSizeIndex].toInt())
                }
                else -> throw RuntimeException("Something isn't right here")
            }
            // peripheral role
            else -> when {
                rbCoap.isChecked -> CoapActivity.getIntent(this)
                rbBlaster.isChecked -> {
                    BlastActivity.getIntent(this)
                        .putExtra(EXTRA_BLAST_PACKET_SIZE, packetSizeOptions[packetSizeIndex].toInt())
                }
                else -> throw RuntimeException("Something isn't right here")
            }
        }

        val stackConfig = getStackConfig()

        intent.putExtra(EXTRA_STACK_CONFIG, stackConfig)
            .putExtra(EXTRA_IS_BLE_CENTRAL_ROLE, rbCentral.isChecked)
            .putExtra(EXTRA_SET_START_STACK_MTU, setStackStartMtuSwitch.isChecked)
        startActivity(intent)
    }

    private fun getStackConfig(): StackConfig? {
        var config: StackConfig? = null
        if (rbGattlink.isChecked) {
            config = GattlinkStackConfig
        } else {
            if (!verifyIp4Address(editLocalIp.text.toString())) {
                Toast.makeText(this, "Please enter valid Local Ip4 address", Toast.LENGTH_LONG).show()
                return config
            }
            if (!verifyIp4Address(editRemoteIp.text.toString())) {
                Toast.makeText(this, "Please enter valid Remote Ip4 address", Toast.LENGTH_LONG).show()
                return config
            }
            val localIp = Inet4Address.getByName(editLocalIp.text.toString()) as Inet4Address
            val remoteIp = Inet4Address.getByName(editRemoteIp.text.toString()) as Inet4Address
            val localPort: Int
            val remotePort: Int
            try {
                localPort = editLocalPort.text.toString().let { when (it) { "" -> 0 else -> it.toInt()} }
                remotePort = editRemotePort.text.toString().let { when (it) { "" -> 0 else -> it.toInt()} }
            } catch (e: NumberFormatException) {
                Toast.makeText(this, "Invalid port number", Toast.LENGTH_LONG).show()
                return config
            }
            config = when {
                rbGlUdp.isChecked -> SocketNetifGattlink(localIp, localPort, remoteIp, remotePort)
                rbGlUdpDtls.isChecked -> DtlsSocketNetifGattlink(localIp, localPort, remoteIp, remotePort)
                else -> {
                    Toast.makeText(this, "Add the new stack config!", Toast.LENGTH_LONG).show()
                    null
                }
            }
        }

        return config
    }

    private fun addIpAndPort(intent: Intent): Boolean {
        intent.putExtra(EXTRA_REMOTE_IP, editRemoteIp.text.toString())
        try {
            val localPort = editLocalPort.text.toString()
            intent.putExtra(EXTRA_LOCAL_PORT, if (localPort == "") 0 else localPort.toInt())
            val remotePort = editRemotePort.text.toString()
            intent.putExtra(EXTRA_REMOTE_PORT, if (remotePort == "") 0 else remotePort.toInt())
        } catch (e: NumberFormatException) {
            Toast.makeText(this, "Invalid port number", Toast.LENGTH_LONG).show()
            return false //Don't start the activity if the port is invalid
        }
        return true
    }

    private fun startBsdCoapActivity() {
        val result = verifyBsdSocketInput()
        if (!result.first) {
            Toast.makeText(this, result.second, Toast.LENGTH_LONG).show()
            return
        }

        val intent = Intent(this, BsdCoapActivity::class.java)
        if(!addIpAndPort(intent)) {
            return
        }
        startActivity(intent)
    }

    private fun verifyBsdSocketInput(): Pair<Boolean, String> {
        if (editLocalPort.text.isBlank()) {
            return Pair(false, "Please enter Local port number")
        }
        if (editRemotePort.text.isBlank()) {
            return Pair(false, "Please enter Remote port number")
        }
        // if empty remote ip we assuming intention is to be in coap server only mode
        if (editRemoteIp.text.isNotBlank() &&
                !verifyIp4Address(editRemoteIp.text.toString())
        ) {
            return Pair(false, "Please enter valid Remote Ip4 address")
        }
        return Pair(true, "Great job")
    }

    private fun verifyIp4Address(ipString: String): Boolean {
        try {
            val inetAddress = Inet4Address.getByName(ipString)
            return inetAddress.address != null && inetAddress.address.size == 4
        } catch (ex: Exception) {
            Timber.e(ex, "Wrong IP format $ipString")
        }
        return false
    }
}
