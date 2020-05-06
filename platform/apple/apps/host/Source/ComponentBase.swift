//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ComponentBase.swift
//  GoldenGateHost-iOS
//
//  Created by Marcel Jackwerth on 11/1/17.
//

import CoreBluetooth
import GoldenGate
import GoldenGateXP
import RxBluetoothKit
import RxCocoa
import RxSwift

// swiftlint:disable trailing_closure

/// Values that need to be provided by the specific role.
protocol ComponentBaseDependencies {
    var role: Role { get }
    var peerRecordDatabaseStore: PeerRecordDatabaseStore { get }
}

/// DI container shared by the "Hub" and "Node" role.
class ComponentBase {
    let dependencies: ComponentBaseDependencies
    let runLoop = GoldenGate.RunLoop.shared
    let appDidBecomeActiveSubject = PublishSubject<Void>()
    let bluetoothConfiguration = BluetoothConfiguration.default
    let disposeBag = DisposeBag()

    init(dependencies: ComponentBaseDependencies) {
        self.dependencies = dependencies

        do {
            try GoldenGateInitializer.initialize()
        } catch let error {
            assertionFailure("Failed to initialize GoldenGate: \(error)")
        }

        runLoop.start()
    }

    lazy var bluetoothQueue = DispatchQueue(label: "bluetooth")

    lazy var rxCentralManager: RxBluetoothKit.CentralManager = {
        let restoreIdentifier = UUID(uuidString: "1e5c8785-2f5c-4e87-9101-c7df1ebe75a6")!

        var options: [String: AnyObject] = [
            CBCentralManagerOptionShowPowerAlertKey: false as NSNumber
        ]

         #if os(iOS)
             options[CBCentralManagerOptionRestoreIdentifierKey] = restoreIdentifier.uuidString as NSString
         #endif

        return RxBluetoothKit.CentralManager(
            queue: bluetoothQueue,
            options: options
        )
    }()

    lazy var centralManager: GoldenGate.CentralManager = {
        GoldenGate.CentralManager(
            manager: rxCentralManager,
            queue: bluetoothQueue
        )
    }()

    lazy var peripheralManager: GoldenGate.PeripheralManager = {
        let restoreIdentifier = UUID(uuidString: "2dae1d80-b261-4dd5-863c-6b813f3306ba")!

        var options: [String: AnyObject] = [
            CBCentralManagerOptionShowPowerAlertKey: false as NSNumber
        ]

        #if os(iOS)
//            options[CBCentralManagerOptionRestoreIdentifierKey] = restoreIdentifier.uuidString as NSString
        #endif

        return GoldenGate.PeripheralManager(
            queue: bluetoothQueue,
            options: options,
            configuration: bluetoothConfiguration
        )
    }()

    private lazy var bluetoothAvailable: Observable<Bool> = {
        rxCentralManager.observeState()
            .map { ![.unsupported, .unauthorized, .poweredOff].contains($0) }
    }()

    lazy var gattlinkService: GattlinkService = {
        let service = GattlinkService(
            configuration: bluetoothConfiguration,
            peripheralManager: peripheralManager
        )

        // Eagerly publish service
        peripheralManager
            .publish(service: service)
            .subscribe()
            .disposed(by: disposeBag)

        return service
    }()

    lazy var linkStatusService: LinkStatusService = {
        let service = LinkStatusService(
            configuration: bluetoothConfiguration,
            peripheralManager: peripheralManager
        )

        // Eagerly publish service
        peripheralManager
            .publish(service: service)
            .subscribe()
            .disposed(by: disposeBag)

        return service
    }()

    lazy var node: Node = {
        Node(
            runLoop: runLoop,
            centralManager: centralManager,
            peripheralManager: peripheralManager,
            gattlinkService: gattlinkService,
            configuration: bluetoothConfiguration
        )
    }()

    lazy var linkConfigurationService: LinkConfigurationService = {
        return hub.linkConfigurationService
    }()

    lazy var hub: Hub = {
        Hub(
            runLoop: runLoop,
            centralManager: centralManager,
            peripheralManager: peripheralManager,
            configuration: bluetoothConfiguration
        )
    }()

    lazy var scanner: Hub.Scanner = {
        Hub.Scanner(
            centralManager: rxCentralManager
        )
    }()

    lazy var advertiser: Node.Advertiser = {
        Node.Advertiser(
            peripheralManager: peripheralManager,
            gattlinkService: gattlinkService,
            configuration: bluetoothConfiguration
        )
    }()

    lazy var peerRecordDatabase: PeerRecordDatabase = {
        let database = dependencies.peerRecordDatabaseStore.load()

        database.didChange
            .subscribe(onNext: { [unowned self] in
                self.dependencies.peerRecordDatabaseStore.store(database)
            })
            .disposed(by: disposeBag)

        return database
    }()

    func makeDefaultConnectionController(descriptor: Observable<StackDescriptor>) -> DefaultConnectionController {
        let connector: AnyBluetoothPeerConnector

        switch dependencies.role {
        case .hub: connector = AnyBluetoothPeerConnector(hub)
        case .node: connector = AnyBluetoothPeerConnector(node)
        }

        let stackBuilder = StackBuilder(
            runLoop: runLoop,
            descriptor: descriptor,
            role: dependencies.role,
            stackProvider: makeStack
        )

        let resetFailureHistory = PublishSubject<Void>()
        let reconnectStrategy = BluetoothReconnectStrategy(
            resumeTrigger: self.appDidBecomeActiveSubject,
            bluetoothState: self.rxCentralManager.observeState(),
            resetFailureHistory: resetFailureHistory
        )

        // swiftlint:disable:next force_try
        let reconnectingController = try! ReconnectingConnectionController(
            ConnectionController(
                runLoop: self.runLoop,
                connector: connector,
                stackBuilder: stackBuilder,
                descriptor: nil
            ),
            reconnectStrategy: reconnectStrategy
        )

        _ = reconnectingController.state
            .asObservable()
            .filter { $0 == .connected }
            .subscribe(onNext: { _ in
                resetFailureHistory.onNext(())
            })

        return reconnectingController
    }

    func makeStack(descriptor: StackDescriptor, role: Role, link: NetworkLink) throws -> Stack {
        Log("Creating new stack for \(link) with \(descriptor)", .info)
        return try Stack(
            runLoop: runLoop,
            descriptor: descriptor,
            role: role,
            link: link,
            elementConfigurationProvider: self
        )
    }

    lazy var helloPsk: (identity: Data, key: Data) = {
        let identity = "hello".data(using: .utf8)!

        let key = Data([
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
            ])

        return (identity, key)
    }()

    lazy var bootstrapPsk: (identity: Data, key: Data) = {
        let identity = "BOOTSTRAP".data(using: .utf8)!

        let key = Data([
            0x81, 0x06, 0x54, 0xE3, 0x36, 0xAD, 0xCA, 0xB0,
            0xA0, 0x3C, 0x60, 0xF7, 0x4A, 0xA0, 0xB6, 0xFB
            ])

        return (identity, key)
    }()

    let dtlsOptions = DtlsOptions.defaultCipherSuites

    lazy var keyResolver: TlsKeyResolver = {
        TlsKeyResolver(runLoop: runLoop) { [unowned self] identity in
            switch identity {
            case self.helloPsk.identity:
                return self.helloPsk.key
            case self.bootstrapPsk.identity:
                return self.bootstrapPsk.key
            default:
                return nil
            }
        }
    }()

    let globalStackDescriptor = BehaviorRelay(value: StackDescriptor.dtlsSocketNetifGattlinkActivity)

    lazy var globalServiceDescriptor: BehaviorRelay<ServiceDescriptor> = BehaviorRelay(value: .coap)

    let globalBlasterConfiguration = BehaviorRelay(value: BlasterService.Configuration.default)

    lazy var commonPeerParameters: CommonPeer.Parameters = { [rxCentralManager] in
        let transportReadiness: Observable<TransportReadiness> = bluetoothAvailable
            .map { $0 ? .ready : .notReady(reason: BluetoothError(state: rxCentralManager.state) ?? RxError.unknown) }

        return CommonPeer.Parameters(
            runLoop: runLoop,
            globalStackDescriptor: globalStackDescriptor.asObservable(),
            globalServiceDescriptor: globalServiceDescriptor.asObservable(),
            globalBlasterConfiguration: globalBlasterConfiguration.asObservable(),
            defaultPortTransportReadiness: transportReadiness,
            connectionControllerProvider: makeDefaultConnectionController
        )
    }()
}

extension BluetoothError {
    init?(state: BluetoothState) {
        switch state {
        case .unsupported:
            self = .bluetoothUnsupported
        case .unauthorized:
            self = .bluetoothUnauthorized
        case .poweredOff:
            self = .bluetoothPoweredOff
        case .unknown:
            self = .bluetoothInUnknownState
        case .resetting:
            self = .bluetoothResetting
        default:
            return nil
        }
    }
}

extension ComponentBase: StackElementConfigurationProvider {
    func dtlsServerConfiguration() -> SecureDatagramServer.Options? {
        return SecureDatagramServer.Options(
            base: dtlsOptions,
            keyResolver: keyResolver
        )
    }

    func dtlsClientConfiguration() -> SecureDatagramClient.Options? {
        return SecureDatagramClient.Options(
            base: dtlsOptions,
            pskIdentity: bootstrapPsk.identity,
            psk: bootstrapPsk.key
        )
    }

    func gattlinkConfiguration() -> GattlinkParameters? {
        return nil
    }
}

/// Type-erased Connector
private class AnyBluetoothPeerConnector: Connector {
    typealias Descriptor = BluetoothPeerDescriptor
    typealias ConnectionType = Connection

    // swiftlint:disable:next identifier_name
    let _establishConnection: (Descriptor) -> Observable<ConnectionType>

    init<C: Connector>(_ connector: C) where C.Descriptor == Descriptor, C.ConnectionType == ConnectionType {
        self._establishConnection = connector.establishConnection
    }

    func establishConnection(with descriptor: Descriptor) -> Observable<ConnectionType> {
        return _establishConnection(descriptor)
    }
}
