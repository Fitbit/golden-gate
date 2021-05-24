//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeerViewModel.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 7/6/18.
//

#if os(iOS)

import Foundation
import RxCocoa
import RxSwift

public class PeerViewModel<ConnectionType: LinkConnection>: PeerViewControllerViewModel {
    public let peer: Observable<Peer>

    public var connectionStatus: Observable<ConnectionStatus<ConnectionType>> {
        connectionController.connectionStatus
    }

    public func connect(trigger: ConnectionTrigger) {
        connectionController.establishConnection(trigger: trigger)
    }

    public func disconnect(trigger: ConnectionTrigger) {
        connectionController.disconnect(trigger: trigger)
    }

    public var descriptor: Observable<PeerDescriptor?> {
        connectionController.descriptor
    }

    public var stackDescriptor: Observable<StackDescriptor> {
        peer.flatMapLatest(\.stackDescriptor)
    }

    public var serviceDescriptor: Observable<ServiceDescriptor> {
        peer.flatMapLatest(\.serviceDescriptor)
    }

    public var supportedStackDescriptors: Observable<[StackDescriptor]> {
        return peer.map { type(of: $0).supportedStackDescriptors }
    }

    public var supportedServiceDescriptors: Observable<[ServiceDescriptor]> {
        return peer.map { type(of: $0).supportedServiceDescriptors }
    }

    public var customPortUrl: Observable<URL?> {
        peer.flatMapLatest(\.customPortUrl)
    }

    public var disconnectionRequired: Observable<Void> {
        peer.flatMapLatest(\.disconnectionRequired)
    }

    public var bufferFullness: Observable<BufferFullness> {
        peer.flatMapLatest(\.bufferFullness)
    }

    public func setCustomStackDescriptor(_ descriptor: StackDescriptor?) {
        customStackDescriptor.onNext(descriptor)
    }

    public func setCustomServiceDescriptor(_ descriptor: ServiceDescriptor?) {
        customServiceDescriptor.onNext(descriptor)
    }

    public var configureStackViewControllerViewModel: ConfigureStackViewControllerViewModel {
        return ConfigureStackViewModel(peer: peer)
    }

    public var stackDescription: Driver<String> {
        return Observable
            .combineLatest(
                customPortUrl.startWith(nil),
                stackDescriptor
            ) { customPortUrl, stackDescriptor in
                customPortUrl?.description ?? stackDescriptor.rawValue
            }
            .asDriver(onErrorJustReturn: "")
    }

    private var peerConnectionStatus: Driver<ConnectionStatus<ConnectionType>> {
        return connectionStatus
            .asDriver(onErrorJustReturn: .disconnected)
    }

    public var connectionDetails: [(label: String, value: Driver<String?>, isChecked: Driver<Bool>)] {
        return [
            (
                label: "Peripheral",
                value: peerConnectionStatus.map { $0.cellDescription },
                isChecked: peerConnectionStatus.map { $0.connected }
            )
        ]
    }

    public var networkLinkStatusDetails: [(label: String, value: Driver<String?>)] {
        let networkLink = peerConnectionStatus
            .map { $0.connection }
            .flatMapLatest { $0?.networkLink.asDriver() ?? Driver.just(nil) }

        return [
            (label: "Status", value: networkLink.map { $0 != nil ? "Up" : "Down" })
        ]
    }

    public var connectionStatusDetails: [(label: String, value: Driver<String?>)] {
        let connectionStatus = remoteConnectionStatus.asDriver(onErrorJustReturn: nil)

        var details = [
            (
                label: "Bonded",
                value: connectionStatus.map {
                    ($0?.bonded).map { $0 ? "Yes" : "No" }
                }
            ),
            (
                label: "Encrypted",
                value: connectionStatus.map {
                    ($0?.encrypted).map { $0 ? "Yes" : "No" }
                }
            )
        ]

        let halfBonded: Driver<Bool?> = {
            if #available(iOS 13.4, *) {
                return connectionController.isHalfBonded.optionalize().asDriver(onErrorJustReturn: nil)
            } else {
                return connectionStatus.map { $0?.halfBonded }
            }
        }()

        details.append(
            (
                label: "Half-bonded",
                value: halfBonded.map {
                    $0.map { $0 ? "Yes" : "No" }
                }
            )
        )

        return details
    }

    public var descriptorDetails: [(label: String, value: Driver<String?>)] {
        let descriptor = self.descriptor.asDriver(onErrorJustReturn: nil)

        return [
            (label: "PeerIdentifier", value: descriptor.map { $0?.identifier.uuidString ?? "" })
        ]
    }

    public var connectionConfigurationDetails: [(label: String, value: Driver<String?>)] {
        let configuration = remoteConnectionConfiguration.asDriver(onErrorJustReturn: nil)

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

    private let connectionController: ConnectionController<ConnectionType>
    private let remoteConnectionStatus: Observable<LinkStatusService.ConnectionStatus?>
    private let remoteConnectionConfiguration: Observable<LinkStatusService.ConnectionConfiguration?>
    public let linkConfigurationViewControllerViewModel: LinkConfigurationViewControllerViewModel

    private let customStackDescriptor = PublishSubject<StackDescriptor?>()
    private let customServiceDescriptor = PublishSubject<ServiceDescriptor?>()
    private let disposeBag = DisposeBag()

    public init(
        peer: Observable<Peer>,
        connectionController: ConnectionController<ConnectionType>,
        remoteConnectionStatus: Observable<LinkStatusService.ConnectionStatus?>,
        remoteConnectionConfiguration: Observable<LinkStatusService.ConnectionConfiguration?>,
        linkConfigurationViewControllerViewModel: LinkConfigurationViewControllerViewModel
    ) {
        self.peer = peer
        self.connectionController = connectionController
        self.remoteConnectionStatus = remoteConnectionStatus
        self.remoteConnectionConfiguration = remoteConnectionConfiguration
        self.linkConfigurationViewControllerViewModel = linkConfigurationViewControllerViewModel

        Observable.combineLatest(peer, customStackDescriptor)
            .subscribe(onNext: {
                $0.0.setCustomStackDescriptor($0.1)
            })
            .disposed(by: disposeBag)

        Observable.combineLatest(peer, customServiceDescriptor)
            .subscribe(onNext: {
                $0.0.setCustomServiceDescriptor($0.1)
            })
            .disposed(by: disposeBag)
    }
}

private final class ConfigureStackViewModel: ConfigureStackViewControllerViewModel {
    private let customPortUrlSubject = PublishSubject<URL?>()
    private let disposeBag = DisposeBag()

    let supportsCustomPortUrl: Observable<Bool> = .just(true)
    let customPortUrl: Observable<URL?>

    init(peer: Observable<Peer>) {
        self.customPortUrl = peer.flatMapLatest(\.customPortUrl)

        Observable.combineLatest(peer, customPortUrlSubject)
            .subscribe(onNext: {
                $0.0.setCustomPortUrl($0.1)
            })
            .disposed(by: disposeBag)
    }

    func setCustomPortUrl(_ url: URL?) {
        customPortUrlSubject.onNext(url)
    }
}

#endif

public extension ConnectionStatus {
    var cellDescription: String {
        switch self {
        case .disconnected: return "Disconnected"
        case .connecting: return "Connecting…"
        case .connected: return "Connected"
        }
    }
}
