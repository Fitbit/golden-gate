package com.fitbit.goldengatehost

import android.content.Intent
import android.content.pm.PackageManager
import android.os.Bundle
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.appcompat.app.AppCompatActivity
import android.view.View
import android.view.View.GONE
import android.view.View.VISIBLE
import android.widget.AdapterView
import android.widget.ArrayAdapter
import android.widget.Toast
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
import kotlinx.android.synthetic.main.a_setup.container_edit_ip
import kotlinx.android.synthetic.main.a_setup.edit_local_ip
import kotlinx.android.synthetic.main.a_setup.edit_local_port
import kotlinx.android.synthetic.main.a_setup.edit_remote_ip
import kotlinx.android.synthetic.main.a_setup.edit_remote_port
import kotlinx.android.synthetic.main.a_setup.packet_size_layout
import kotlinx.android.synthetic.main.a_setup.packet_size_spinner
import kotlinx.android.synthetic.main.a_setup.radioGroupConfiguration
import kotlinx.android.synthetic.main.a_setup.radioGroupService
import kotlinx.android.synthetic.main.a_setup.rb_blaster
import kotlinx.android.synthetic.main.a_setup.rb_bsd_socket_coap
import kotlinx.android.synthetic.main.a_setup.rb_coap
import kotlinx.android.synthetic.main.a_setup.rb_gattlink
import kotlinx.android.synthetic.main.a_setup.rb_gl_udp
import kotlinx.android.synthetic.main.a_setup.rb_gl_udp_dtls
import kotlinx.android.synthetic.main.a_setup.start_button
import timber.log.Timber
import java.net.Inet4Address

/**
 * A screen for configuring the Host App
 */
class SetupActivity : AppCompatActivity(), AdapterView.OnItemSelectedListener {

    companion object {
        private val packetSizeOptions = arrayOf("32", "64", "128", "256", "512", "1024")
        private var packetSizeIndex = packetSizeOptions.lastIndex
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.a_setup)
        respondToSettingsChange()
        setPacketSizeSpinner()

        radioGroupConfiguration.setOnCheckedChangeListener { _, _ -> respondToSettingsChange() }
        radioGroupService.setOnCheckedChangeListener { _, _ -> respondToSettingsChange() }

        start_button.setOnClickListener { startMainActivity() }
    }

    private fun setPacketSizeSpinner() {
        val spinner = packet_size_spinner
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
        edit_local_port.setText("")
        edit_remote_port.setText("")

        //Checked state for this switch means client mode
        radioGroupService.visibility = when {
            rb_bsd_socket_coap.isChecked -> GONE
            else -> {
                edit_local_ip.isEnabled = true
                VISIBLE
            }
        }

        if (rb_gattlink.isChecked) {
            when { rb_coap.isChecked -> rb_blaster.isChecked = true }
            container_edit_ip.visibility = GONE
            rb_coap.isEnabled = false
        } else {
            container_edit_ip.visibility = VISIBLE
            if (rb_bsd_socket_coap.isChecked) {
                edit_remote_port.setText(COAP_DEFAULT_PORT.toString())
                edit_remote_ip.setText("")
                edit_local_port.setText(COAP_DEFAULT_PORT.toString())
                // local ip is not editable in Bsd socket mode
                edit_local_ip.setText(getNetworkInterfaceIpAddress())
                edit_local_ip.isEnabled = false
            } else {
                edit_local_ip.setText(DEFAULT_SERVER_ADDRESS)
                edit_remote_ip.setText(DEFAULT_CLIENT_ADDRESS)
                rb_coap.isEnabled = true
            }
        }

        packet_size_layout.visibility = when {
            rb_blaster.isChecked -> VISIBLE
            else -> GONE
        }
    }

    private fun startMainActivity() {
        val i: Intent = when {
            rb_bsd_socket_coap.isChecked -> {
                startBsdCoapActivity()
                return
            }
            rb_coap.isChecked -> ScanActivity.getIntent(this).putExtra(EXTRA_LAUNCH_CLASS, CoapActivity::class.java)
            rb_blaster.isChecked -> {
                ScanActivity.getIntent(this)
                        .putExtra(EXTRA_LAUNCH_CLASS, BlastActivity::class.java)
                        .putExtra(EXTRA_BLAST_PACKET_SIZE, packetSizeOptions[packetSizeIndex].toInt())
            }
            else -> throw RuntimeException("Something isn't right here")
        }

        val config: StackConfig
        if (rb_gattlink.isChecked) {
            config = GattlinkStackConfig
        } else {
            if (!verifyIp4Address(edit_local_ip.text.toString())) {
                Toast.makeText(this, "Please enter valid Local Ip4 address", Toast.LENGTH_LONG).show()
                return
            }
            if (!verifyIp4Address(edit_remote_ip.text.toString())) {
                Toast.makeText(this, "Please enter valid Remote Ip4 address", Toast.LENGTH_LONG).show()
                return
            }
            val localIp = Inet4Address.getByName(edit_local_ip.text.toString()) as Inet4Address
            val remoteIp = Inet4Address.getByName(edit_remote_ip.text.toString()) as Inet4Address
            val localPort: Int
            val remotePort: Int
            try {
                localPort = edit_local_port.text.toString().let { when (it) { "" -> 0 else -> it.toInt()} }
                remotePort = edit_remote_port.text.toString().let { when (it) { "" -> 0 else -> it.toInt()} }
            } catch (e: NumberFormatException) {
                Toast.makeText(this, "Invalid port number", Toast.LENGTH_LONG).show()
                return //Don't start the activity if the port is invalid
            }
            config = when {
                rb_gl_udp.isChecked -> SocketNetifGattlink(localIp, localPort, remoteIp, remotePort)
                rb_gl_udp_dtls.isChecked -> DtlsSocketNetifGattlink(localIp, localPort, remoteIp, remotePort)
                else -> {
                    Toast.makeText(this, "Add the new stack config!", Toast.LENGTH_LONG).show()
                    return
                }
            }
        }
        i.putExtra(EXTRA_STACK_CONFIG, config)
        startActivity(i)
    }

    private fun addIpAndPort(intent: Intent): Boolean {
        intent.putExtra(EXTRA_REMOTE_IP, edit_remote_ip.text.toString())
        try {
            val localPort = edit_local_port.text.toString()
            intent.putExtra(EXTRA_LOCAL_PORT, if (localPort == "") 0 else localPort.toInt())
            val remotePort = edit_remote_port.text.toString()
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
        if (edit_local_port.text.isBlank()) {
            return Pair(false, "Please enter Local port number")
        }
        if (edit_remote_port.text.isBlank()) {
            return Pair(false, "Please enter Remote port number")
        }
        // if empty remote ip we assuming intention is to be in coap server only mode
        if (edit_remote_ip.text.isNotBlank() &&
                !verifyIp4Address(edit_remote_ip.text.toString())
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
