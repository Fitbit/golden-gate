//
//  Node+NetworkLink.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 2/25/19.
//  Copyright © 2019 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import GoldenGateXP
import RxBluetoothKit
import RxCocoa
import RxSwift

private struct NodeNetworkLink: NetworkLink {
    let dataSink: DataSink
    let dataSource: DataSource
    let mtu: BehaviorRelay<UInt>
}

extension NodeNetworkLink: CustomStringConvertible {}

extension Node {
    func networkLinkStatus(
        for centralIdentifier: UUID,
        gattlinkService: GattlinkService,
        peripheralManager: PeripheralManager
    ) -> Observable<NetworkLinkStatus> {
        return gattlinkService.centralSubscriptionStatus(for: centralIdentifier)
            .map { $0.central }
            .filterNil()
            .map {
                try self.networkLinkStatus(
                    for: $0,
                    gattlinkService: gattlinkService,
                    peripheralManager: peripheralManager
                )
            }
            .startWith(.negotiating("Waiting for Gattlink subscription..."))
    }

    // Returns the connection status representing a central subscription to Gattlink Service
    func networkLinkStatus(
        for central: CBCentral,
        gattlinkService: GattlinkService,
        peripheralManager: PeripheralManager
    ) throws -> NetworkLinkStatus {
        let dataSink = PeripheralManagerSink(
            runLoop: runLoop,
            peripheralManager: peripheralManager.peripheralManager,
            central: central,
            characteristic: gattlinkService.tx,
            readyToUpdateSubscribers: peripheralManager.readyToUpdateSubscribers
        )

        // Gattlink doesn't handle back-pressure on the link
        // and assumes that it can always write a full window
        // consecutively.
        //
        // So we just cache up to the maximum window size
        // since we can afford it on the mobile platform.
        let dataBuffer = try DataBuffer(
            size: Int(GG_GENERIC_GATTLINK_CLIENT_DEFAULT_MAX_TX_WINDOW_SIZE),
            dataSink: dataSink
        )

        let dataSource = PeripheralManagerSource(
            runLoop: runLoop,
            writeRequests: gattlinkService.didReceiveWrite
                .filter { [gattlinkService] in
                    $0.characteristic == gattlinkService.rx && $0.central == central
                }
                .do(onNext: {
                    LogBluetoothDebug("Incoming gattlink data from \($0.central)")
                })
        )

        let networkLink = NodeNetworkLink(
            dataSink: dataBuffer,
            dataSource: dataSource,
            mtu: .init(value: UInt(central.maximumUpdateValueLength))
        )

        return .connected(networkLink)
    }
}
