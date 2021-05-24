//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Node+NetworkLink.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 2/25/19.
//

import BluetoothConnection
import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxCocoa
import RxSwift

extension Node {
    func networkLink(
        for centralIdentifier: UUID,
        linkService: LinkService,
        peripheralManager: BluetoothConnection.PeripheralManager
    ) -> Observable<NetworkLink> {
        return linkService.centralSubscriptionStatus(for: centralIdentifier)
            .compactMap { $0.central }
            .map {
                try self.networkLink(
                    for: $0,
                    linkService: linkService,
                    peripheralManager: peripheralManager
                )
            }
    }

    // Returns the network link representing a central subscription to Link Service
    func networkLink(
        for central: CBCentral,
        linkService: LinkService,
        peripheralManager: BluetoothConnection.PeripheralManager
    ) throws -> NetworkLink {
        let dataSink = PeripheralManagerSink(
            listenerScheduler: protocolScheduler,
            peripheralManager: peripheralManager.peripheralManager,
            central: central,
            characteristic: linkService.tx,
            readyToUpdateSubscribers: peripheralManager.readyToUpdateSubscribers
        )

        // The link service doesn't handle back-pressure on the link
        // and assumes that it can always write a full window
        // consecutively.
        //
        // So we just cache up to the maximum window size
        // since we can afford it on the mobile platform.
        let dataBuffer = try DataBuffer(
            size: Int(dataSinkBufferSize),
            dataSink: dataSink
        )

        let dataSource = PeripheralManagerSource(
            sinkScheduler: protocolScheduler,
            writeRequests: linkService.didReceiveWrite
                .filter { [linkService] in
                    $0.characteristic == linkService.rx && $0.central == central
                }
                .do(onNext: {
                    LogBluetoothDebug("Incoming link data from \($0.central)")
                })
        )

        return NetworkLink(
            dataSink: dataBuffer,
            dataSource: dataSource,
            mtu: .init(value: UInt(central.maximumUpdateValueLength))
        )
    }
}
