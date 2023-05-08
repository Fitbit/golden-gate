// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost

import android.annotation.SuppressLint
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
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import androidx.appcompat.app.AlertDialog
import android.support.v7.app.AppCompatActivity
import android.view.View
import android.view.ViewGroup
import android.widget.Button
import android.widget.LinearLayout
import android.widget.Switch
import android.widget.TextView
import android.widget.Toast
import androidx.annotation.RequiresApi
import com.fitbit.bluetooth.fbgatt.FitbitGatt
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.CentralConnectionChangeListener
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionChangeListener
import com.fitbit.bluetooth.fbgatt.rx.PeripheralConnectionStatus
import com.fitbit.bluetooth.fbgatt.rx.advertiser.Advertiser
import com.fitbit.bluetooth.fbgatt.rx.client.BitGattPeer
import com.fitbit.bluetooth.fbgatt.rx.client.GattCharacteristicWriter
import com.fitbit.bluetooth.fbgatt.rx.server.listeners.ConnectionEvent
import com.fitbit.goldengate.bindings.GoldenGate
import com.fitbit.goldengate.bindings.dtls.DtlsProtocolStatus
import com.fitbit.goldengate.bindings.node.BluetoothAddressNodeKey
import com.fitbit.goldengate.bindings.stack.DtlsSocketNetifGattlink
import com.fitbit.goldengate.bindings.stack.Stack
import com.fitbit.goldengate.bindings.stack.StackConfig
import com.fitbit.goldengate.bindings.stack.StackService
import com.fitbit.goldengate.bt.PeerRole
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.FitbitGattlinkService
import com.fitbit.goldengate.node.Peer
import com.fitbit.goldengate.node.PeerBuilder
import com.fitbit.goldengate.node.NodeMapper
import com.fitbit.goldengatehost.scan.EXTRA_DEVICE
import com.fitbit.linkcontroller.ui.LinkControllerActivity
import com.google.android.material.snackbar.Snackbar
import io.reactivex.Observable
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.schedulers.Schedulers
import timber.log.Timber
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicInteger

const val EXTRA_SET_START_STACK_MTU = "extra_set_start_stack_mtu"
const val EXTRA_STACK_CONFIG = "extra_stack_config"
const val EXTRA_LOCAL_PORT = "extra_local_port"
const val EXTRA_REMOTE_IP = "extra_remote_ip"
const val EXTRA_REMOTE_PORT = "extra_remote_port"
const val EXTRA_IS_BLE_CENTRAL_ROLE = "extra_is_ble_central_role"
const val REQ_ID = 1234
const val SELECT_DEVICE_REQUEST_CODE = 9123
const val GGHOST_PARTIAL_WAKELOCK = "goldengatehost:partial_wakelock"
const val MAX_RETRY_ATTEMPTS = 5

abstract class AbstractHostActivity<T: StackService> : AppCompatActivity() {

    internal val disposeBag = CompositeDisposable()

    protected lateinit var send: Button
    private lateinit var buildmodel: TextView
    private lateinit var disconnect: Button
    private lateinit var layout: ViewGroup
    private lateinit var container: ViewGroup
    private lateinit var gattlinkStatus: TextView
    private lateinit var handshakeStatus: TextView
    private lateinit var ggVersion: TextView
    private lateinit var linkControllerSetup: Button
    private lateinit var slowLogsToggle: Switch
    private lateinit var deviceLinking: Switch
    private lateinit var wakeLock: Switch
    private lateinit var advertiser: Advertiser
    private lateinit var centralControls: LinearLayout

    private var companionDeviceManager: CompanionDeviceManager? = null
    private var bluetoothWakeLock: PowerManager.WakeLock? = null
    private var backPressedListener: (() -> Unit)? = null
    private lateinit var nodeKey: BluetoothAddressNodeKey
    private var isBleCentralRole: Boolean = true
    private var setStackStartMtu: Boolean = true

    /**
     * Get the android content view
     */
    abstract fun getContentViewRes(): Int

    /**
     * Get the peer builder that can build a peer (node or hub) with stack service [T]
     */
    abstract fun getPeerBuilder(
        peerRole: PeerRole,
        stackConfig: StackConfig,
        connectionStatus: (GattConnection) -> Observable<PeripheralConnectionStatus>,
        dtlsStatus: (Stack) -> Observable<DtlsProtocolStatus>,
        setStackStartMtu: Boolean,
        ): PeerBuilder<T, BluetoothAddressNodeKey>

    /**
     * Method called when a node reports a successful connection
     */
    abstract fun onConnected(stackService: T, stackConfig: StackConfig)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(getContentViewRes())
        bindViews()
        val buildNumber = packageManager.getPackageInfo(packageName, 0).versionCode
        if (buildNumber != 10000) { //If we're not being built off a jenkins job, the version code will be 10000.
            title = "$title v$buildNumber"
        }

        ggVersion.setOnClickListener { showVersion() }
        buildmodel.text = Build.MODEL

        val stackConfig = intent.getSerializableExtra(EXTRA_STACK_CONFIG) as StackConfig

        isBleCentralRole = intent.getBooleanExtra(EXTRA_IS_BLE_CENTRAL_ROLE, true)
        setStackStartMtu = intent.getBooleanExtra(EXTRA_SET_START_STACK_MTU, true)

        if (isBleCentralRole) {
            val bluetoothDevice  = intent.getParcelableExtra<BluetoothDevice>(EXTRA_DEVICE)
            val gattConnection = FitbitGatt.getInstance().getConnection(bluetoothDevice)
            if (gattConnection == null || bluetoothDevice == null) {
                Toast.makeText(this, "Device ${bluetoothDevice?.address} not known to bitgatt", Toast.LENGTH_LONG).show()
                finish()
                return
            }
            val currentDevice = BitGattPeer(gattConnection)
            nodeKey = BluetoothAddressNodeKey(bluetoothDevice.address)
            val stackNode = NodeMapper.instance.get(
                nodeKey,
                getPeerBuilder(
                    PeerRole.Peripheral,
                    stackConfig,
                    listenToGattlinkStatus,
                    listenToDtlsEvents(stackConfig),
                    setStackStartMtu),
                true
            ) as Peer<T>

            if (stackConfig is DtlsSocketNetifGattlink) {
                handshakeStatus.visibility = View.VISIBLE
            }
            gattlinkStatus.text = getString(R.string.gattlink_status, PeripheralConnectionStatus.DISCONNECTED)
            handshakeStatus.text = getString(R.string.handshake_status, DtlsProtocolStatus.TlsProtocolState.TLS_STATE_INIT.name)

            connectToPeripheral(stackNode, currentDevice, stackConfig)
            linkControllerSetup.setOnClickListener { startLinkControllerActivity(bluetoothDevice) }

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
        } else {
            layout.visibility = View.VISIBLE
            if (stackConfig is DtlsSocketNetifGattlink) {
                handshakeStatus.visibility = View.VISIBLE
            }
            centralControls.visibility = View.GONE
            gattlinkStatus.visibility = View.GONE
            handshakeStatus.text = getString(R.string.handshake_status, DtlsProtocolStatus.TlsProtocolState.TLS_STATE_INIT.name)

            connectToCentral(stackConfig)

            startAdvertisement()
        }

        wakeLock.setOnCheckedChangeListener { buttonView, isChecked ->
            if (isChecked) {
                acquirePartialWakeLock()
            } else {
                releasePartialWakeLock()
            }
        }
    }

    private fun bindViews() {
        send = ActivityCompat.requireViewById(this, R.id.send)
        buildmodel = ActivityCompat.requireViewById(this, R.id.buildmodel)
        disconnect = ActivityCompat.requireViewById(this, R.id.disconnect)
        layout = ActivityCompat.requireViewById(this, R.id.layout)
        container = ActivityCompat.requireViewById(this, R.id.container)
        gattlinkStatus = ActivityCompat.requireViewById(this, R.id.gattlink_status)
        handshakeStatus = ActivityCompat.requireViewById(this, R.id.handshake_status)
        ggVersion = ActivityCompat.requireViewById(this, R.id.gg_version)
        linkControllerSetup = ActivityCompat.requireViewById(this, R.id.link_controller_setup)
        slowLogsToggle = ActivityCompat.requireViewById(this, R.id.slow_logs_toggle)
        deviceLinking = ActivityCompat.requireViewById(this, R.id.deviceLinking)
        wakeLock = ActivityCompat.requireViewById(this, R.id.wakelock)
        centralControls = ActivityCompat.requireViewById(this, R.id.central_controls)
    }

    private fun startAdvertisement() {
        advertiser = Advertiser(this)
        disposeBag.add(
            advertiser.startAdvertising(
                Advertiser.getAdvertiseSettings(),
                Advertiser.getAdvertiseData(FitbitGattlinkService.uuid.toString()),
                Advertiser.getScanResponseData())
                .subscribe({
                    Snackbar.make(container, "Start Advertising", Snackbar.LENGTH_SHORT).show()
                    Timber.d("Started advertising")
                }, {
                    Timber.e(it, "Error! Didn't start advertising.")
                    Snackbar.make(container, "Failed to start Advertising", Snackbar.LENGTH_SHORT).show()
                })
        )
    }

    @SuppressLint("CheckResult") // this is a fire-and-forget call
    private fun stopAdvertisement() {
        advertiser.stopAdvertising()
            .subscribe({
                Snackbar.make(container, "Stop Advertising", Snackbar.LENGTH_SHORT).show()
                Timber.d("Stop advertising")
            }, {
                Timber.e(it, "Womp womp. Didn't stop advertising.")
                Snackbar.make(container, "Failed to stop Advertising", Snackbar.LENGTH_SHORT).show()
            })
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
                override fun onDeviceFound(chooserLauncher: IntentSender) {
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
                                handshakeStatus.text = getString(R.string.handshake_status, it.state.name)
                            } else {
                                handshakeStatus.text = getString(R.string.handshake_status,
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
                gattlinkStatus.text = getString(R.string.gattlink_status, it)
            }
    }

    private fun connectToPeripheral(stackPeer: Peer<T>, currentDevice: BitGattPeer, stackConfig: StackConfig) {
        disposeBag.add(stackPeer.connection()
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

                disconnect.setOnClickListener { disconnect(stackPeer) }
                disconnect.visibility = View.VISIBLE

                backPressedListener = {
                    NodeMapper.instance.removeNode(nodeKey)
                }
                onConnected(stackPeer.stackService, stackConfig)
            }, {
                Timber.e(it, "Error connecting")
            }))
    }

    private fun connectToCentral(stackConfig: StackConfig) {
        lateinit var currentDevice: BitGattPeer
        var stackHub: Peer<T>? = null

        disposeBag.add(
            CentralConnectionChangeListener()
                .register()
                .filter { it is ConnectionEvent.Connected }
                .map {
                    stopAdvertisement()
                    it as ConnectionEvent.Connected
                }
                .flatMap {
                    waitForConnectionReady(it.centralDevice)
                }
                .flatMap {
                    currentDevice = BitGattPeer(it)
                    nodeKey = BluetoothAddressNodeKey(it.device.address)
                    stackHub = getPeerBuilder(
                        PeerRole.Central,
                        stackConfig,
                        listenToGattlinkStatus,
                        listenToDtlsEvents(stackConfig),
                        setStackStartMtu).build(nodeKey)
                    stackHub?.connection()
                }
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .doFinally {
                    stackHub?.let {
                        disconnect(it)
                    }
                    onConnectionLost()
                }
                .subscribe({
                    centralControls.visibility = View.VISIBLE
                    send.visibility = View.VISIBLE
                    buildmodel.text = String.format("Connected to %s", currentDevice.fitbitDevice.btDevice.address)

                    disconnect.setOnClickListener { disconnect(stackHub) }
                    disconnect.visibility = View.VISIBLE

                    backPressedListener = {
                        disconnect(stackHub)
                    }

                    stackHub?.let{ onConnected(it.stackService, stackConfig) }
                },
                    { Timber.d(it, "Fail to set up peripheral mode.") }
                )
        )

        disposeBag.add(
            CentralConnectionChangeListener()
                .register()
                .filter { it is ConnectionEvent.Disconnected }
                .map {
                    it as ConnectionEvent.Disconnected
                }
                .subscribe(
                    { Timber.d("Disconnect!")
                        onConnectionLost() },
                    { Timber.d(it, "Error to handle disconnection!") }
                )
        )
    }

    private fun waitForConnectionReady(device: BluetoothDevice): Observable<GattConnection> {
        val fitbitGatt: FitbitGatt = FitbitGatt.getInstance()

        return Observable.create<GattConnection> { emitter ->
            var connection = fitbitGatt.getConnection(device)
            if (connection!= null) {
                emitter.onNext(connection)
            } else {
                emitter.onError(RuntimeException("GATT connection has not been added to connection map"))
            }}
            .retryWhen { errors ->
                val counter = AtomicInteger()
                errors.takeWhile { counter.getAndIncrement() <= MAX_RETRY_ATTEMPTS }
                    .flatMap { e ->
                        val retryCount = counter.get()
                        if (retryCount > MAX_RETRY_ATTEMPTS) {
                            throw e
                        }
                        Observable.timer(retryCount * 1000L, TimeUnit.MILLISECONDS, Schedulers.computation())
                    }
            }
    }

    private fun disconnect(peer: Peer<T>?) {
        peer?.apply {
            disconnect()
        }
        backPressedListener = null
    }

    private fun onConnectionLost() {
        Snackbar.make(container, R.string.connection_disconnected, Snackbar.LENGTH_LONG).show()
        finish()
    }

    override fun onDestroy() {
        if (isBleCentralRole) {
            if (Build.VERSION.SDK_INT >= O) {
                unlinkAllDevices()
            }
        } else {
            stopAdvertisement()
        }
        releasePartialWakeLock()
        disposeBag.dispose()
        super.onDestroy()
    }

    override fun onBackPressed() {
        backPressedListener?.invoke()
        super.onBackPressed()
    }
}
