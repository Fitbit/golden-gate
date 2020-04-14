package com.fitbit.goldengatehost

import android.app.Activity
import android.bluetooth.BluetoothDevice
import android.companion.AssociationRequest
import android.companion.BluetoothDeviceFilter
import android.companion.CompanionDeviceManager
import android.content.Context
import android.content.Intent
import android.content.IntentSender
import android.content.pm.PackageManager
import android.os.Build
import android.os.Build.VERSION_CODES.O
import android.os.Bundle
import android.os.PowerManager
import com.google.android.material.snackbar.Snackbar
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.annotation.RequiresApi
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionChangeListener
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeripheral
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicWriter
import com.fitbit.goldengate.bindings.GoldenGate
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.node.Node
import com.fitbit.goldengate.node.NodeBuilder
import com.fitbit.goldengate.node.NodeMapper
import com.fitbit.goldengatehost.scan.EXTRA_DEVICE
import com.fitbit.linkcontroller.ui.LinkControllerActivity
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.schedulers.Schedulers
import timber.log.Timber

const val EXTRA_STACK_CONFIG = "extra_stack_config"
const val EXTRA_LOCAL_PORT = "extra_local_port"
const val EXTRA_REMOTE_IP = "extra_remote_ip"
const val EXTRA_REMOTE_PORT = "extra_remote_port"
const val REQ_ID = 1234
const val SELECT_DEVICE_REQUEST_CODE = 9123
const val GGHOST_PARTIAL_WAKELOCK = "goldengatehost:partial_wakelock"

abstract class AbstractHostActivity<T: StackService> : AppCompatActivity() {

    internal val disposeBag = CompositeDisposable()

    protected lateinit var send: Button
    private lateinit var buildmodel: TextView
    private lateinit var disconnect: Button
    private lateinit var layout: ViewGroup
    private lateinit var container: ViewGroup
    private lateinit var gattlink_status: TextView
    private lateinit var handshake_status: TextView
    private lateinit var ggVersion: TextView
    private lateinit var linkControllerSetup: Button
    private lateinit var slowLogsToggle: Switch
    private lateinit var deviceLinking: Switch
    private lateinit var wakeLock: Switch

    private var companionDeviceManager: CompanionDeviceManager? = null
    private var bluetoothWakeLock: PowerManager.WakeLock? = null
    private var backPressedListener: (() -> Unit)? = null
    private lateinit var nodeKey:BluetoothAddressNodeKey
    /**
     * Get the android content view
     */
    abstract fun getContentViewRes(): Int

    /**
     * Get the node builder that can build a node with stack service [T]
     */
    abstract fun getNodeBuilder(
            stackConfig: StackConfig,
            connectionStatus: (GattConnection) -> Observable<PeripheralConnectionStatus>,
            dtlsStatus: (Stack) -> Observable<DtlsProtocolStatus>
    ): NodeBuilder<T, BluetoothAddressNodeKey>

    /**
     * Method called when a node reports a successful connection
     */
    abstract fun onConnected(stackService: T, stackConfig: StackConfig)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(getContentViewRes())
        val buildNumber = packageManager.getPackageInfo(packageName, 0).versionCode
        if (buildNumber != 10000) { //If we're not being built off a jenkins job, the version code will be 10000.
            title = "$title v$buildNumber"
        }

        send = findViewById(R.id.send)
        buildmodel = findViewById(R.id.buildmodel)
        disconnect = findViewById(R.id.disconnect)
        layout = findViewById(R.id.layout)
        container = findViewById(R.id.container)
        gattlink_status = findViewById(R.id.gattlink_status)
        handshake_status = findViewById(R.id.handshake_status)
        ggVersion = findViewById(R.id.gg_version)
        linkControllerSetup = findViewById(R.id.link_controller_setup)
        slowLogsToggle = findViewById(R.id.slow_logs_toggle)
        deviceLinking = findViewById(R.id.deviceLinking)
        wakeLock = findViewById(R.id.wakelock)

        ggVersion.setOnClickListener { showVersion() }
        buildmodel.text = Build.MODEL

        val stackConfig = intent.getSerializableExtra(EXTRA_STACK_CONFIG) as StackConfig
        val bluetoothDevice = intent.getParcelableExtra(EXTRA_DEVICE) as BluetoothDevice
        val gattConnection = FitbitGatt.getInstance().getConnection(bluetoothDevice)
        if (gattConnection == null) {
            Toast.makeText(this, "Device ${bluetoothDevice.address} not known to bitgatt", Toast.LENGTH_LONG).show()
            finish()
            return
        }
        val currentDevice = BitGattPeripheral(gattConnection)
        nodeKey = BluetoothAddressNodeKey(bluetoothDevice.address)
        val stackNode = NodeMapper.instance.get(
                nodeKey,
                getNodeBuilder(stackConfig, listenToGattlinkStatus, listenToDtlsEvents(stackConfig)),
                true
        ) as Node<T>

        if (stackConfig is DtlsSocketNetifGattlink) {
            findViewById<View>(R.id.handshake_status).visibility = View.VISIBLE
        }
        gattlink_status.text = getString(R.string.gattlink_status, PeripheralConnectionStatus.DISCONNECTED)
        handshake_status.text = getString(R.string.handshake_status, DtlsProtocolStatus.TlsProtocolState.TLS_STATE_INIT.name)

        connectCentralMode(stackNode, currentDevice, stackConfig)
        linkControllerSetup.setOnClickListener { startLinkControllerActivity(bluetoothDevice)}

        slowLogsToggle.setOnCheckedChangeListener { _, isChecked ->
            FitbitGatt.getInstance().setSlowLoggingEnabled(isChecked)
            GattCharacteristicWriter.slowLoggingEnabled = isChecked
        }

        if (Build.VERSION.SDK_INT >= O) {
            companionDeviceManager = getSystemService(CompanionDeviceManager::class.java)
            unlinkAllDevices()
            deviceLinking.visibility = View.VISIBLE

            deviceLinking.setOnCheckedChangeListener { buttonView, isChecked ->
                if (isChecked) {
                    linkDevice(bluetoothDevice)
                } else {
                    unlinkDevice(bluetoothDevice)
                }
            }
        }

        wakeLock.setOnCheckedChangeListener { buttonView, isChecked ->
            if (isChecked) {
                acquirePartialWakeLock()
            } else {
                releasePartialWakeLock()
            }
        }
    }

    @RequiresApi(O)
    private fun linkDevice(device: BluetoothDevice?) {
        if (device == null) {
            Timber.d("No device to link")
            return
        }

        val associateMacs = companionDeviceManager?.associations
        if (associateMacs != null) {
            for (mac in associateMacs) {
                if (mac == device.address) {
                    Timber.v("Device already associated")
                    return
                }
            }
        }

        val deviceFilter = BluetoothDeviceFilter.Builder()
            .setAddress(device.address)
            .build()
        val linkingRequest = AssociationRequest.Builder()
            .addDeviceFilter(deviceFilter)
            .setSingleDevice(true)
            .build()

        companionDeviceManager?.associate(linkingRequest,
            object : CompanionDeviceManager.Callback() {
                override fun onDeviceFound(chooserLauncher: IntentSender?) {
                    Timber.v("We've found the device for the user")
                    try {
                        if (!isFinishing) {
                            startIntentSenderForResult(chooserLauncher, SELECT_DEVICE_REQUEST_CODE,
                                null, 0, 0, 0)
                        } else {
                            Timber.w("There was a problem linking with device. Could not start pairing dialog")
                        }
                    } catch (e: IntentSender.SendIntentException) {
                        Timber.e(e, "Show user linking dialog failed")
                    }
                }

                override fun onFailure(error: CharSequence?) {
                    Timber.e("There was a problem linking with device %s", error)
                }

            }, null)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        when (requestCode) {
            SELECT_DEVICE_REQUEST_CODE ->
                when (resultCode) {
                    Activity.RESULT_OK -> {
                        // User has chosen to pair with the Bluetooth device.
                        val deviceToPair: BluetoothDevice? =
                            data?.getParcelableExtra(CompanionDeviceManager.EXTRA_DEVICE)
                        deviceToPair?.createBond()

                    }
                }
        }
    }

    @RequiresApi(O)
    private fun unlinkAllDevices() {
        val associateMacs = companionDeviceManager?.associations
        if (associateMacs != null) {
            for (mac in associateMacs) {
                companionDeviceManager?.disassociate(mac)
            }
        }
    }

    @RequiresApi(O)
    private fun unlinkDevice(device: BluetoothDevice?) {
        if (device == null) {
            val associateMacs = companionDeviceManager?.associations
            if (associateMacs != null) {
                for (mac in associateMacs) {
                    companionDeviceManager?.disassociate(mac)
                }
            }
            return
        }
        companionDeviceManager?.disassociate(device.address)
    }

    private fun acquirePartialWakeLock() {
        bluetoothWakeLock =
            (getSystemService(Context.POWER_SERVICE) as PowerManager).run {
                newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, GGHOST_PARTIAL_WAKELOCK).apply {
                    acquire()
                }
            }
        Timber.v("Acquired the partial wake lock")
    }

    private fun releasePartialWakeLock() {
        bluetoothWakeLock?.release()
        bluetoothWakeLock = null
        Timber.v("Release the partial wake lock")
    }

    private fun startLinkControllerActivity(bluetoothDevice: BluetoothDevice){
        val intent = Intent(this, LinkControllerActivity::class.java)
        intent.putExtra(LinkControllerActivity.EXTRA_DEVICE, bluetoothDevice)
        startActivity(intent)

    }
    private fun showVersion() {
        val version = GoldenGate.getVersion()
        val builder = AlertDialog.Builder(this)
        builder.setMessage(
                getString(
                        R.string.version_number,
                    version.maj,
                    version.min,
                    version.patch,
                    version.branchName,
                    version.commitHash,
                    version.buildDate,
                    version.buildTime
                )
        ).setPositiveButton("Ok") { dialog, _ -> dialog.cancel() }
        builder.show()
    }

    override fun onResume() {
        super.onResume()

        if (ContextCompat.checkSelfPermission(this, android.Manifest.permission.ACCESS_COARSE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            ActivityCompat.requestPermissions(this, arrayOf(android.Manifest.permission.ACCESS_COARSE_LOCATION), REQ_ID)
        }
    }

    private fun listenToDtlsEvents(stackConfig: StackConfig): (Stack) -> Observable<DtlsProtocolStatus> =
            when (stackConfig) {
                is DtlsSocketNetifGattlink -> {
                    { stack ->
                        stack.dtlsEventObservable
                                .observeOn(AndroidSchedulers.mainThread())
                                .doOnNext {
                                    if (it.state != DtlsProtocolStatus.TlsProtocolState.TLS_STATE_ERROR) {
                                        handshake_status.text = getString(R.string.handshake_status, it.state.name)
                                    } else {
                                        handshake_status.text = getString(R.string.handshake_status,
                                                "%s (error code = %d)".format(it.state.name, it.lastError))
                                    }
                                }
                    }
                }
                else ->  { stack -> stack.dtlsEventObservable }
            }

    private val listenToGattlinkStatus: (GattConnection) -> Observable<PeripheralConnectionStatus> = { connection ->
        PeripheralConnectionChangeListener().register(connection).observeOn(AndroidSchedulers.mainThread())
                .doOnNext {
                    gattlink_status.text = getString(R.string.gattlink_status, it)
                }
    }

    private fun connectCentralMode(stackNode: Node<T>, currentDevice: BitGattPeripheral, stackConfig: StackConfig) {
        disposeBag.add(stackNode.connection()
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .doOnSubscribe {
                    layout.visibility = View.VISIBLE
                    buildmodel.text = String.format("Connecting to %s", currentDevice.fitbitDevice.btDevice.address)
                }
                .doFinally { onConnectionLost() }
                .subscribe({
                    send.visibility = View.VISIBLE
                    buildmodel.text = String.format("Connected to %s", currentDevice.fitbitDevice.btDevice.address)

                    disconnect.setOnClickListener { disconnect(stackNode) }
                    disconnect.visibility = View.VISIBLE

                    backPressedListener = {
                        NodeMapper.instance.removeNode(nodeKey)
                    }
                    onConnected(stackNode.stackService, stackConfig)
                }, {
                    Timber.e(it, "Error connecting")
                }))
    }

    private fun disconnect(node: Node<T>) {
        node.disconnect()
        backPressedListener = null
    }

    private fun onConnectionLost() {
        Snackbar.make(container, R.string.connection_disconnected, Snackbar.LENGTH_LONG).show()
        finish()
    }

    override fun onStop() {
        if (Build.VERSION.SDK_INT >= O) {
            unlinkAllDevices()
        }
        releasePartialWakeLock()
        disposeBag.dispose()
        super.onStop()
    }

    override fun onBackPressed() {
        backPressedListener?.invoke()
    }
}
