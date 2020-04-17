// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost.scan

import android.content.Context
import android.os.Bundle
import android.os.ParcelUuid
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.DividerItemDecoration
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.rx.GOLDEN_GATE_SERVICE_UUID_MASK
import com.fitbit.bluetooth.fbgatt.rx.KnownGattConnectionFinder
import com.fitbit.bluetooth.fbgatt.rx.scanner.PeripheralScanner
import com.fitbit.bluetooth.fbgatt.rx.scanner.PeripheralScanner.ScanEvent.Discovered
import com.fitbit.bluetooth.fbgatt.rx.scanner.ServiceUuidPeripheralScannerFilter
import com.fitbit.goldengate.bt.gatt.server.services.gattlink.GattlinkService
import com.fitbit.goldengatehost.R
import io.reactivex.android.schedulers.AndroidSchedulers
import io.reactivex.disposables.CompositeDisposable
import io.reactivex.schedulers.Schedulers
import timber.log.Timber

class ScanFragment : Fragment() {

    interface ScanListener {
        fun onScanItemClick(connection: GattConnection)
    }

    private var listener: ScanListener? = null
    private lateinit var adapter: ScanRecyclerViewAdapter
    private val scanner: PeripheralScanner = PeripheralScanner()
    private val knownGattConnectionFinder: KnownGattConnectionFinder = KnownGattConnectionFinder()
    private val disposeBag = CompositeDisposable()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {

        adapter = ScanRecyclerViewAdapter(listener)

        val view = inflater.inflate(R.layout.fragment_scan_list, container, false)
        if (view is RecyclerView) {
            with(view) {
                adapter = this@ScanFragment.adapter
                layoutManager = LinearLayoutManager(context)
                addItemDecoration(DividerItemDecoration(view.context, DividerItemDecoration.VERTICAL))
            }
        }
        return view
    }

    override fun onAttach(context: Context) {
        super.onAttach(context)
        if (context is ScanListener) {
            listener = context
        } else {
            throw RuntimeException("$context must implement OnListFragmentInteractionListener")
        }
    }

    override fun onResume() {
        super.onResume()
        scan()
    }

    override fun onPause() {
        super.onPause()
        disposeBag.clear()
    }

    override fun onDetach() {
        super.onDetach()
        listener = null
    }

    private fun scan() {
        disposeBag.add(
            scanner.scanForDevices(
                context = requireContext(),
                resetExistingFilters = true,
                scanFilters = scanFilter()
            )
                // TODO: Filter known connections by for example GG service ID
                .mergeWith(knownGattConnectionFinder.find().flattenAsObservable { connections -> connections.map { connection -> Discovered(connection) } })
                .subscribeOn(Schedulers.io())
                .observeOn(AndroidSchedulers.mainThread())
                .subscribe(
                    { scanEvent ->
                        Timber.d("Scan result: $scanEvent")
                        when (scanEvent) {
                            is Discovered -> adapter.add(scanEvent.connection)
                            else -> Timber.w("Ignoring: Unexpected scan result")
                        }
                    },
                    { t -> Timber.e(t, "Failure scanning") },
                    {
                        Toast.makeText(context, "Fail to start scanning!\n" +
                            "Please check Bluetooth settings or\n" +
                            "wait a few seconds before retrying.", Toast.LENGTH_LONG).show()
                    }
                )
        )
    }

    private fun scanFilter() = listOf(gattlinkServiceFilter())

    private fun gattlinkServiceFilter() =
        ServiceUuidPeripheralScannerFilter(ParcelUuid(GattlinkService.uuid), ParcelUuid.fromString(GOLDEN_GATE_SERVICE_UUID_MASK))

}
