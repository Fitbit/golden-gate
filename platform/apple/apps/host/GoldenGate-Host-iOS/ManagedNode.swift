//
//  ManagedNode.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 11/20/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import GoldenGate
import RxCocoa
import RxSwift

class ManagedNode: ManagedPeer {
    private let disposeBag = DisposeBag()

    public let preferredConnectionConfiguration = BehaviorRelay<LinkConfigurationService.PreferredConnectionConfiguration>(value: .default)
    public let preferredConnectionMode = BehaviorRelay<LinkConfigurationService.PreferredConnectionMode>(value: .default)
    
    init(record: PeerRecord, commonPeerParameters: CommonPeer.Parameters, remoteTestServer: RemoteTestServerType?, linkConfigurationService: LinkConfigurationService) {
        super.init(
            record: record,
            commonPeerParameters: commonPeerParameters,
            remoteTestServer: remoteTestServer
        )

        // Handle gattlink session stall event
        setupGattlinkSessionStallHandling(linkConfigurationService: linkConfigurationService)

        // Handle transport activity monitor logging
        setupTransportActivityMonitor()

        // Restore preferred connection configuration from DB
        preferredConnectionConfiguration.accept(record.preferredConnectionConfiguration)
        preferredConnectionMode.accept(record.preferredConnectionMode)

        // Notify subscribed centrals when preferred connection mode changes
        // and save new value to database
        preferredConnectionMode.asObservable()
            .skip(1)
            .subscribe(onNext: { [linkConfigurationService] in
                if let bluetoothPeerDescriptor = record.bluetoothPeerDescriptor {
                    linkConfigurationService.update(
                        preferredConnectionModeValue: $0,
                        onSubscribedCentral: bluetoothPeerDescriptor.identifier
                    )
                }
                record.preferredConnectionMode = $0
            })
            .disposed(by: disposeBag)

        // Notify subscribed centrals when preferred connection configuration changes
        // and save new value to database
        preferredConnectionConfiguration.asObservable()
            .skip(1)
            .subscribe(onNext: { [linkConfigurationService] in
                if let bluetoothPeerDescriptor = record.bluetoothPeerDescriptor {
                    linkConfigurationService.update(
                        preferredConnectionConfigurationValue: $0,
                        onSubscribedCentral: bluetoothPeerDescriptor.identifier
                    )
                }
                record.preferredConnectionConfiguration = $0
            })
            .disposed(by: disposeBag)

        // Handle Preferred Connection Configuration read requests
        linkConfigurationService.preferredConnectionConfigurationReadRequests
            .filter {
                $0.central.identifier == record.bluetoothPeerDescriptor?.identifier
            }
            .subscribe(onNext: { [preferredConnectionConfiguration] in
                $0.respond(withResult: .success(preferredConnectionConfiguration.value.rawValue))
            })
            .disposed(by: disposeBag)

        // Handle Preferred Connection Mode read requests
        linkConfigurationService.preferredConnectionModeReadRequests
            .filter {
                $0.central.identifier == record.bluetoothPeerDescriptor?.identifier
            }
            .subscribe(onNext: { [preferredConnectionMode] in
                $0.respond(withResult: .success(preferredConnectionMode.value.rawValue))
            })
            .disposed(by: disposeBag)
    }

    private func setupTransportActivityMonitor() {
        transportActivityChange
            .subscribe(onNext: { (direction, active) in
                Log("Detected \(direction) \(active ? "activity" : "inactivity")...", .info)
            })
            .disposed(by: disposeBag)
    }

    private func setupGattlinkSessionStallHandling(linkConfigurationService: LinkConfigurationService) {
        // Force disconnect on gattlink session stall event
        gattlinkSessionStalled.withLatestFrom(connectionController.descriptor)
            .filterNil()
            .subscribe(onNext: { bluetoothDescriptor in
                Log("Gattlink session stalled. Sending a force disconnect command...", .warning)

                linkConfigurationService.send(
                    command: .init(code: .disconnect),
                    onSubscribedCentral: bluetoothDescriptor.identifier
                )
            })
            .disposed(by: disposeBag)
    }
}
