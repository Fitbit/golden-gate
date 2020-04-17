//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CommonPeerViewModel.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 7/6/18.
//

#if os(iOS)

import Rxbit
import RxCocoa
import RxSwift

public class CommonPeerViewModel: CommonPeerViewControllerViewModel {
    public var connectionController: DefaultConnectionController {
        return commonPeer.connectionController
    }

    public var stackDescriptor: Observable<StackDescriptor> {
        return commonPeer.stackDescriptor
    }

    public func setCustomStackDescriptor(_ descriptor: StackDescriptor?) {
        commonPeer.setCustomStackDescriptor(descriptor)
    }

    public var serviceDescriptor: Observable<ServiceDescriptor> {
        return commonPeer.serviceDescriptor
    }

    public func setCustomServiceDescriptor(_ descriptor: ServiceDescriptor?) {
        commonPeer.setCustomServiceDescriptor(descriptor)
    }

    public func setUserWantsToConnect(_ connect: Bool) {
        commonPeer.setUserWantsToConnect(connect)
    }

    public var userWantsToConnect: SharedSequence<DriverSharingStrategy, Bool> {
        return commonPeer.userWantsToConnect
    }

    public var stackDescription: Driver<String> {
        return Observable
            .combineLatest(commonPeer.customPortUrl, commonPeer.stackDescriptor) { customPortUrl, stackDescriptor in
                customPortUrl?.description ?? stackDescriptor.rawValue
            }
            .asDriver(onErrorJustReturn: "")
    }

    public var connectionIsUserControllable: Driver<Bool> {
        return .just(true)
    }

    public var configureStackViewControllerViewModel: ConfigureStackViewControllerViewModel {
        return commonPeer
    }

    public var serviceViewControllerViewModel: ServiceViewControllerViewModel {
        return commonPeer
    }

    private var connection: Driver<Hub.Connection?> {
        return commonPeer.connectionController.connection
            .asDriver(onErrorJustReturn: nil)
            .map { $0 as? Hub.Connection }
    }

    public var connectionDetails: [(label: String, value: Driver<String?>, isChecked: Driver<Bool>)] {
        let peripheralConnection = connection
            .flatMapLatest { $0?.peripheralConnection.asDriver().optionalize() ?? Driver.just(nil) }

        return [(
                label: "Peripheral",
                value: peripheralConnection.map { $0?.cellDescription },
                isChecked: peripheralConnection.map { $0?.connected ?? false }
            )
        ]
    }

    public var networkLinkStatusDetails: [(label: String, value: Driver<String?>)] {
        let networkLinkStatus = connection
            .flatMapLatest { $0?.networkLinkStatus.asDriver() ?? Driver.just(.disconnected) }

        return [
            (label: "Status", value: networkLinkStatus.map { "\($0)" })
        ]
    }

    public var connectionStatusDetails: [(label: String, value: Driver<String?>)] {
        let connectionStatus = connection
            .flatMapLatest { $0?.remoteConnectionStatus.asDriver() ?? Driver.just(nil) }

        return [
            (label: "Bonded", value: connectionStatus.map { ($0?.bonded).map { $0 ? "Yes" : "No" } }),
            (label: "Encrypted", value: connectionStatus.map { ($0?.encrypted).map { $0 ? "Yes" : "No" } }),
            (label: "Half-bonded", value: connectionStatus.map { ($0?.halfBonded).map { $0 ? "Yes" : "No" } })
        ]
    }

    public var batteryLevelDetails: [(label: String, value: Driver<String?>)] {
        let batteryLevel = connection
            .flatMapLatest { $0?.remoteBatteryLevel.asDriver() ?? Driver.just(nil) }

        return [
            (label: "Level", value: batteryLevel.map { ($0?.level).map { "\($0) %" } ?? "Unknown" })
        ]
    }

    public var descriptorDetails: [(label: String, value: Driver<String?>)] {
        let descriptor = commonPeer.connectionController.descriptor
            .asDriver(onErrorJustReturn: nil)

        return [
            (label: "PeerIdentifier", value: descriptor.map { $0?.identifier.uuidString ?? "" })
        ]
    }

    public var connectionConfigurationDetails: [(label: String, value: Driver<String?>)] {
        let configuration = connection
            .flatMapLatest { $0?.remoteConnectionConfiguration.asDriver() ?? Driver.just(nil) }

        return [(
                label: "Connection Interval",
                value: configuration
                    .map { $0?.connectionInterval }
                    .map { $0.map { "\(Double($0) * 1.25) ms" } }
            ), (
                label: "Slave Latency",
                value: configuration
                    .map { $0?.slaveLatency }
                    .map { $0.map { "\($0) intervals" } }
            ), (
                label: "Supervision Timeout",
                value: configuration
                    .map { $0?.supervisionTimeout }
                    .map { $0.map { "\(Double($0) * 10) ms" } }
            ), (
                label: "MTU",
                value: configuration.map { $0?.MTU.description }
            ), (
                label: "Mode",
                value: configuration.map { "\($0?.mode ??? "unknown")" }
            )
        ]
    }

    public let commonPeer: CommonPeer
    public let linkConfigurationViewControllerViewModel: LinkConfigurationViewControllerViewModel

    public init(commonPeer: CommonPeer, linkConfigurationViewControllerViewModel: LinkConfigurationViewControllerViewModel) {
        self.commonPeer = commonPeer
        self.linkConfigurationViewControllerViewModel = linkConfigurationViewControllerViewModel
    }
}

extension CommonPeer: ConfigureStackViewControllerViewModel {
    public var supportsCustomPortUrl: Observable<Bool> {
        return .just(true)
    }
}

extension CommonPeer: ServiceViewControllerViewModel {}

private extension Role {
    var cellDescription: String {
        switch self {
        case .node: return "Node"
        case .hub: return "Hub"
        }
    }
}

private extension CentralSubscriptionStatus {
    var subscribed: Bool {
        switch self {
        case .unsubscribed: return false
        case .subscribed: return true
        }
    }

    var cellDescription: String {
        switch self {
        case .unsubscribed: return "Not Subscribed"
        case .subscribed: return "Subscribed"
        }
    }
}

private extension PeripheralConnectionStatus {
    var connected: Bool {
        switch self {
        case .disconnected, .connecting: return false
        case .connected: return true
        }
    }

    var cellDescription: String {
        switch self {
        case .disconnected: return "Disconnected"
        case .connecting: return "Connecting…"
        case .connected: return "Connected"
        }
    }
}

#endif
