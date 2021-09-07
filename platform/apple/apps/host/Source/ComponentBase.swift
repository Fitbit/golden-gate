//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ComponentBase.swift
//  GoldenGateHost-iOS
//
//  Created by Marcel Jackwerth on 11/1/17.
//

import BluetoothConnection
import CoreBluetooth
import GoldenGate
import GoldenGateXP
import Rxbit
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
    let nodeLinkServiceConfiguration = BluetoothConfiguration.LinkServiceConfiguration.default
    let disposeBag = DisposeBag()

    init(dependencies: ComponentBaseDependencies) {
        self.dependencies = dependencies

        ErrorTrackers.default = LoggingErrorTracker()

        // Publish the LinkConfigurationService if the current role is `Hub`.
        if dependencies.role == .hub {
            peripheralManager
                .publish(service: linkConfigurationService)
                .subscribe()
                .disposed(by: disposeBag)
        }

        do {
            try GoldenGateInitializer.initialize()
        } catch let error {
            assertionFailure("Failed to initialize GoldenGate: \(error)")
        }

        runLoop.start()
    }

    lazy var bluetoothQueue = DispatchQueue(label: "bluetooth")
    lazy var bluetoothScheduler = SerialDispatchQueueScheduler(queue: bluetoothQueue, internalSerialQueueName: bluetoothQueue.label)

    lazy var centralManager: CentralManager = {
        let restoreIdentifier = UUID(uuidString: "1e5c8785-2f5c-4e87-9101-c7df1ebe75a6")!

        var options: [String: AnyObject] = [
            CBCentralManagerOptionShowPowerAlertKey: false as NSNumber
        ]

         #if os(iOS)
             options[CBCentralManagerOptionRestoreIdentifierKey] = restoreIdentifier.uuidString as NSString
         #endif

        return CentralManager(
            queue: bluetoothQueue,
            options: options
        )
    }()

    lazy var peripheralManager: BluetoothConnection.PeripheralManager = {
        let restoreIdentifier = UUID(uuidString: "2dae1d80-b261-4dd5-863c-6b813f3306ba")!

        var options: [String: AnyObject] = [
            CBCentralManagerOptionShowPowerAlertKey: false as NSNumber
        ]

        #if os(iOS)
//            options[CBCentralManagerOptionRestoreIdentifierKey] = restoreIdentifier.uuidString as NSString
        #endif

        return BluetoothConnection.PeripheralManager(
            queue: bluetoothQueue,
            options: options,
            name: bluetoothConfiguration.name
        )
    }()

    private lazy var bluetoothAvailable: Observable<Bool> = {
        centralManager.observeStateWithInitialValue()
            .map { ![.unsupported, .unauthorized, .poweredOff].contains($0) }
    }()

    lazy var gattlinkService: LinkService = {
        let service = LinkService(
            configuration: nodeLinkServiceConfiguration,
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
            configuration: bluetoothConfiguration.linkStatusService,
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
            bluetoothScheduler: bluetoothScheduler,
            protocolScheduler: runLoop,
            peripheralManager: peripheralManager,
            linkService: gattlinkService,
            configuration: bluetoothConfiguration,
            dataSinkBufferSize: UInt(GG_GENERIC_GATTLINK_CLIENT_DEFAULT_MAX_TX_WINDOW_SIZE)
        )
    }()

    lazy var linkConfigurationService: LinkConfigurationServiceType & PeripheralManagerService = {
        LinkConfigurationService(
            configuration: bluetoothConfiguration.linkConfigurationService,
            peripheralManager: peripheralManager
        )
    }()

    lazy var hub: Hub = {
        Hub(
            bluetoothScheduler: bluetoothScheduler,
            protocolScheduler: runLoop,
            configuration: bluetoothConfiguration
        )
    }()

    lazy var discoverer: PeerDiscoverer = {
        PeerDiscoverer(centralManager: centralManager, bluetoothScheduler: bluetoothScheduler)
    }()

    lazy var advertiser: BluetoothAdvertiser = {
        BluetoothAdvertiser(
            peripheralManager: peripheralManager,
            linkService: gattlinkService
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

    func makeConnectionController<R: ConnectionResolver>(resolver: R) -> ConnectionController<R.ConnectionType> {
        // Emit a value whenever we observe * -> PoweredOn
        // which might indicate the user power-cycling iOS's Bluetooth.
        let didEnableBluetooth = centralManager.observeState()
            .map { $0 == .poweredOn }
            .distinctUntilChanged()
            .filter { $0 }

        let reconnectStrategy = ConnectivityRetryStrategy(
            configuration: RetryDelayConfiguration(
                interval: .milliseconds(250),
                resume: Observable.merge(
                    appDidBecomeActiveSubject.map { _ in "Application did become active" },
                    didEnableBluetooth.map { _ in "Bluetooth was powered on" }
                )
            )
        )

        return ConnectionController(
            connector: ConnectionResolvingConnector(
                connector: BluetoothConnector(centralManager: centralManager, scheduler: bluetoothScheduler),
                resolver: resolver
            ),
            descriptor: nil,
            reconnectStrategy: reconnectStrategy,
            scheduler: bluetoothScheduler,
            debugIdentifier: "main"
        )
    }

    func makeStackBuilder(descriptor: Observable<StackDescriptor>) -> StackBuilderType {
        return StackBuilder(
            protocolScheduler: runLoop,
            descriptor: descriptor,
            role: dependencies.role,
            stackProvider: makeStack
        )
    }

    func makeStack(descriptor: StackDescriptor, role: Role, link: NetworkLink) throws -> StackType {
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
        let identity = Data("hello".utf8)

        let key = Data([
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
            0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
            ])

        return (identity, key)
    }()

    lazy var bootstrapPsk: (identity: Data, key: Data) = {
        let identity = Data("BOOTSTRAP".utf8)

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

    lazy var peerParameters: PeerParameters = { [centralManager] in
        let transportReadiness: Observable<TransportReadiness> = bluetoothAvailable
            .map { $0 ? .ready : .notReady(reason: BluetoothError(state: centralManager.state) ?? RxError.unknown) }

        return PeerParameters(
            stackDescriptor: globalStackDescriptor.asObservable(),
            serviceDescriptor: globalServiceDescriptor.asObservable(),
            defaultPortTransportReadiness: transportReadiness,
            stackBuilderProvider: makeStackBuilder
        )
    }()
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
