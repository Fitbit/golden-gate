//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ManagedNode.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 11/20/17.
//

import BluetoothConnection
import Foundation
import GoldenGate
import RxCocoa
import RxSwift

class ManagedNode: ManagedPeer<HubConnection> {
    private let disposeBag = DisposeBag()

    public let preferredConnectionConfiguration = BehaviorRelay<LinkConfigurationService.PreferredConnectionConfiguration>(value: .default)
    public let preferredConnectionMode = BehaviorRelay<LinkConfigurationService.PreferredConnectionMode>(value: .default)

    init(
        connectionController: ConnectionController<HubConnection>,
        record: PeerRecord,
        peerParameters: PeerParameters,
        runLoop: GoldenGate.RunLoop,
        globalBlasterConfiguration: Observable<BlasterService.Configuration>,
        linkConfigurationService: LinkConfigurationServiceType
    ) {
        super.init(
            connectionController: connectionController,
            record: record,
            peerParameters: peerParameters,
            runLoop: runLoop,
            globalBlasterConfiguration: globalBlasterConfiguration
        )

        // Handle disconnection requirements (i.e. when gattlink session stalls)
        setupDisconnectionRequiredHandling(linkConfigurationService: linkConfigurationService)

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
                if let peerDescriptor = record.peerDescriptor {
                    linkConfigurationService.update(
                        preferredConnectionModeValue: $0,
                        onSubscribedCentralUuid: peerDescriptor.identifier
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
                if let peerDescriptor = record.peerDescriptor {
                    linkConfigurationService.update(
                        preferredConnectionConfigurationValue: $0,
                        onSubscribedCentralUuid: peerDescriptor.identifier
                    )
                }
                record.preferredConnectionConfiguration = $0
            })
            .disposed(by: disposeBag)

        // Handle Preferred Connection Configuration read requests
        linkConfigurationService.preferredConnectionConfigurationReadRequests
            .filter {
                $0.central.identifier == record.peerDescriptor?.identifier
            }
            .subscribe(onNext: { [preferredConnectionConfiguration] in
                $0.respond(withResult: .success(preferredConnectionConfiguration.value.rawValue))
            })
            .disposed(by: disposeBag)

        // Handle Preferred Connection Mode read requests
        linkConfigurationService.preferredConnectionModeReadRequests
            .filter {
                $0.central.identifier == record.peerDescriptor?.identifier
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

    private func setupDisconnectionRequiredHandling(linkConfigurationService: LinkConfigurationServiceType) {
        // Force disconnect when required
        disconnectionRequired.withLatestFrom(connectionController.descriptor)
            .filterNil()
            .subscribe(onNext: { bluetoothDescriptor in
                Log("Disconnection required. Sending a force disconnect command...", .warning)

                linkConfigurationService.send(
                    command: .init(code: .disconnect),
                    onSubscribedCentralUuid: bluetoothDescriptor.identifier
                )
            })
            .disposed(by: disposeBag)
    }
}
