package com.fitbit.goldengatehost.scan


import android.bluetooth.BluetoothDevice
import androidx.recyclerview.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.TextView
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.goldengatehost.R
import com.fitbit.goldengatehost.scan.ScanFragment.ScanListener
import kotlinx.android.synthetic.main.fragment_scan.view.bluetooth_address
import kotlinx.android.synthetic.main.fragment_scan.view.device_name
import kotlinx.android.synthetic.main.fragment_scan.view.device_origin
import kotlinx.android.synthetic.main.fragment_scan.view.rssi

class ScanRecyclerViewAdapter(
    private val listener: ScanListener?
) : RecyclerView.Adapter<ScanRecyclerViewAdapter.ViewHolder>() {

    private val onClickListener: View.OnClickListener
    private val connectionsMap: LinkedHashMap<BluetoothDevice, Pair<GattConnection, Int>> = LinkedHashMap()

    init {
        onClickListener = View.OnClickListener { v ->
            val connection = v.tag as GattConnection
            listener?.onScanItemClick(connection)
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.fragment_scan, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        val connectionsMapValues = ArrayList<Pair<GattConnection, Int>>(connectionsMap.values)
        val connection = connectionsMapValues[position].first

        holder.deviceNameView.text = String.format(holder.view.context.getString(R.string.scan_item_name), connection.device.name)
        holder.bluetoothAddressView.text = String.format(holder.view.context.getString(R.string.scan_item_address), connection.device.address)
        holder.deviceOriginView.text = String.format(holder.view.context.getString(R.string.scan_item_origin), connection.device.origin.toString())
        holder.rssiView.text = String.format(holder.view.context.getString(R.string.scan_item_rssi), connection.device.rssi)

        with(holder.view) {
            tag = connection
            setOnClickListener(onClickListener)
        }
    }

    override fun getItemCount(): Int = connectionsMap.size

    fun add(connection: GattConnection) {
        var changed = false
        val position = connectionsMap[connection.device.btDevice]?.let {
            changed = true
            it.second
        }?: connectionsMap.size

        connectionsMap[connection.device.btDevice] = Pair(connection, position)
        if(changed) {
            notifyItemChanged(position)
        } else {
            notifyItemInserted(position)
        }

    }

    inner class ViewHolder(val view: View) : RecyclerView.ViewHolder(view) {
        val deviceNameView: TextView = view.device_name
        val bluetoothAddressView: TextView = view.bluetooth_address
        val deviceOriginView: TextView = view.device_origin
        val rssiView: TextView = view.rssi
    }
}
