// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

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

class ScanRecyclerViewAdapter(
    private val listener: ScanListener?
) : RecyclerView.Adapter<ScanRecyclerViewAdapter.ViewHolder>() {

    private val onClickListener: View.OnClickListener = View.OnClickListener { v ->
        val connection = v.tag as GattConnection
        listener?.onScanItemClick(connection)
    }
    private val connectionsMap: LinkedHashMap<BluetoothDevice, Pair<GattConnection, Int>> = LinkedHashMap()

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
        val deviceNameView: TextView = view.findViewById(R.id.device_name)
        val bluetoothAddressView: TextView = view.findViewById(R.id.bluetooth_address)
        val deviceOriginView: TextView = view.findViewById(R.id.device_origin)
        val rssiView: TextView = view.findViewById(R.id.rssi)
    }
}
