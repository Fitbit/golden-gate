//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CommonPeer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/11/18.
//

import RxCocoa
import RxSwift

/// A facade that launches services on top of a connection controller.
open class CommonPeer {
    /// Simplifies inheriting from `CommonPeer`.
    public struct Parameters {
        fileprivate let runLoop: RunLoop
        fileprivate let globalStackDescriptor: Observable<StackDescriptor>
        fileprivate let globalServiceDescriptor: Observable<ServiceDescriptor>
        fileprivate let globalBlasterConfiguration: Observable<BlasterService.Configuration>
        fileprivate let defaultPortTransportReadiness: Observable<TransportReadiness>
        fileprivate let connectionControllerProvider: (Observable<StackDescriptor>) -> DefaultConnectionController

        public init(
            runLoop: RunLoop,
            globalStackDescriptor: Observable<StackDescriptor>,
            globalServiceDescriptor: Observable<ServiceDescriptor>,
            globalBlasterConfiguration: Observable<BlasterService.Configuration>,
            defaultPortTransportReadiness: Observable<TransportReadiness>,
            connectionControllerProvider: @escaping (Observable<StackDescriptor>) -> DefaultConnectionController
        ) {
            self.runLoop = runLoop
            self.globalStackDescriptor = globalStackDescriptor
            self.globalServiceDescriptor = globalServiceDescriptor
            self.globalBlasterConfiguration = globalBlasterConfiguration
            self.defaultPortTransportReadiness = defaultPortTransportReadiness
            self.connectionControllerProvider = connectionControllerProvider
        }
    }

    /// The current connection state.
    public enum State {
        /// The peer is disconnected.
        case disconnected

        /// System tries to establish a connection.
        case connecting

        /// Connection is established.
        case connected
    }

    /// The peer's current stack setup.
    public let stackDescriptor: Observable<StackDescriptor>

    /// The peer's current service setup.
    public let serviceDescriptor: Observable<ServiceDescriptor>

    /// The peer's connection controller.
    public let connectionController: DefaultConnectionController

    /// The peer's current blaster configuration.
    public let blasterConfiguration: Observable<BlasterService.Configuration>

    /// The peer's current state.
    public let state = BehaviorRelay(value: State.disconnected)

    /// Emits when activity change was detected at the transport layer.
    public let transportActivityChange: Observable<(ActivityMonitorDirection, Bool)>

    /// Emits when gattlink session stalls.
    public let gattlinkSessionStalled: Observable<Void>

    /// Emits when gattlink buffer size is over threshold
    public let gattlinkBufferOverThreshold: Observable<Void>

    /// Emits when gattlink buffer size is under threshold
    public let gattlinkBufferUnderThreshold: Observable<Void>

    /// The current top port that is exposed by the peer's stack.
    public let topPort = BehaviorRelay<Port?>(value: nil)

    /// The peer's blaster service.
    public let blasterService: BlasterService

    /// The peer's CoAP client service.
    private(set) public var coapClientService: CoapClientService?

    /// The peer's CoAP test service.
    private(set) public var coapTestService: CoapTestService?

    // Observable that emits stack events
    private let stackEvents = PublishSubject<StackEvent?>()

    /// The peer's CoAP service.
    public let coapEndpoint: CoapEndpointType

    private let runLoop: RunLoop
    private let customStackDescriptor = BehaviorRelay<StackDescriptor?>(value: nil)
    private let customServiceDescriptor = BehaviorRelay<ServiceDescriptor?>(value: nil)
    private let customBlasterConfiguration = BehaviorRelay<BlasterService.Configuration?>(value: nil)
    private let customPortUrlRelay = BehaviorRelay<URL?>(value: nil)
    private let disposeBag = DisposeBag()

    /// Minimum stalled time threshold beyond which session stalled will be reported.
    private static let gattlinkStalledTimeout: TimeInterval = 60

    // swiftlint:disable:next function_body_length
    public init(_ parameters: Parameters) {
        self.runLoop = parameters.runLoop

        if let urlString = ProcessInfo.processInfo.environment["GG_TOP_PORT"] {
            customPortUrlRelay.accept(URL(string: urlString))
        }

        // Prefer custom service descriptor and fall back to global
        self.stackDescriptor = Observable
            .combineLatest(
                customStackDescriptor,
                parameters.globalStackDescriptor
            )
            .map { $0.0 ?? $0.1 }

        // Prefer custom service descriptor and fall back to global
        self.serviceDescriptor = Observable
            .combineLatest(
                customServiceDescriptor,
                parameters.globalServiceDescriptor
            )
            .map { $0.0 ?? $0.1 }
            .distinctUntilChanged()

        // Prefer custom blaster configuration and fall back to global
        self.blasterConfiguration = Observable
            .combineLatest(
                customBlasterConfiguration,
                parameters.globalBlasterConfiguration
            )
            .map { $0.0 ?? $0.1 }

        self.connectionController = parameters.connectionControllerProvider(
            // Prevent disconnects if nothing changed
            stackDescriptor.asObservable().distinctUntilChanged(==)
        )

        connectionController.stack
            .flatMapLatestForwardingNil { $0.event }
            .bind(to: stackEvents)
            .disposed(by: disposeBag)

        self.transportActivityChange = stackEvents
            .map { event -> (ActivityMonitorDirection, Bool)? in
                if let event = event, case let .activityMonitorChanged(direction, active) = event {
                    return (direction, active)
                }
                return nil
            }
            .filterNil()

        self.gattlinkSessionStalled = stackEvents
            .map { event -> TimeInterval? in
                if let event = event, case .gattlinkSessionStalled(let stalledTime) = event {
                    return stalledTime
                }
                return nil
            }
            .filterNil()
            // Report stalled session after the stalled time exceeds a given threshold.
            .filter { $0 >= CommonPeer.gattlinkStalledTimeout }
            .logWarning("[Common Peer] Gattlink session stalled. Stalled time > \(CommonPeer.gattlinkStalledTimeout) s.", .bindings, .next)
            .map { _ in () }

        self.gattlinkBufferOverThreshold = stackEvents
            .filter { $0 == .gattlinkBufferOverThreshold }
            .map { _ in () }

        self.gattlinkBufferUnderThreshold = stackEvents
            .filter { $0 == .gattlinkBufferUnderThreshold }
            .map { _ in () }

        let transportReadiness = customPortUrlRelay
            .flatMapLatest {
                // Always allow requests when using a custom port.
                $0 == nil ? parameters.defaultPortTransportReadiness : .just(.ready)
            }

        let coapEndpoint = parameters.runLoop.sync { [runLoop, serviceDescriptor, topPort = topPort.asObservable()] in
            // swiftlint:disable:next force_try
            try! CoapEndpoint(
                runLoop: runLoop,
                port: serviceDescriptor
                    .flatMapLatest { descriptor in
                        // Detach topPort from CoapEndpoint when blasting
                        descriptor == .coap ? topPort : .just(nil)
                    }
                    .distinctUntilChanged(nilComparer),
                transportReadiness: transportReadiness
            )
        }

        self.coapEndpoint = coapEndpoint

        self.blasterService = parameters.runLoop.sync { [blasterConfiguration, runLoop, serviceDescriptor, topPort = topPort.asObservable()] in
            // swiftlint:disable:next force_try
            try! BlasterService(
                runLoop: runLoop,
                configuration: blasterConfiguration,
                port: serviceDescriptor
                    .flatMapLatest { descriptor in
                        descriptor == .blasting ? topPort : .just(nil)
                    }
            )
        }

        // Resolve the URL and create a socket from it
        let customPort = customPortUrl
            .observeOn(SerialDispatchQueueScheduler(qos: .background))
            .map { $0.flatMap { SocketAddress(url: $0) } }
            .distinctUntilChanged(==)
            .observeOn(runLoop)
            .logInfo("CustomPortUrl", .bindings, .next)
            .mapForwardingNil { remoteAddress in
                try? GGBsdDatagramSocket(
                    runLoop: parameters.runLoop,
                    localAddress: SocketAddress(address: .any, port: 5683),
                    remoteAddress: remoteAddress
                )
            }
            .logInfo("CustomPortUrl GGBsdDatagramSocket", .bindings, .next)

        // Whenever the service descriptor changes or we are switching to the `customPort`,
        // clear out any references the XP port might have. (See: FC-1046)
        Observable
            .combineLatest(connectionController.stack, customPort, serviceDescriptor) { stack, customPort, _ in
                parameters.runLoop.async {
                    // It would be nice if the services could clean up after themselves,
                    // but they can't know if some other new service already added itself,
                    // leading to potential races.
                    _ = try? stack?.topPort?.dataSink.setDataSinkListener(nil)
                    _ = try? stack?.topPort?.dataSource.setDataSink(nil)
                }

                return customPort ?? stack?.topPort
            }
            .subscribe(onNext: topPort.accept)
            .disposed(by: disposeBag)

        /// Expose the state hiding the inner enum
        connectionController.state.asObservable()
            .map { state -> State in
                switch state {
                case .connected: return .connected
                case .connecting: return .connecting
                case .disconnected: return .disconnected
                }
            }
            .subscribe(onNext: state.accept)
            .disposed(by: disposeBag)
    }

    public func setCustomStackDescriptor(_ descriptor: StackDescriptor?) {
        customStackDescriptor.accept(descriptor)
        customPortUrlRelay.accept(nil)
    }

    public func setCustomServiceDescriptor(_ descriptor: ServiceDescriptor?) {
        customServiceDescriptor.accept(descriptor)
    }

    public func setCustomPortUrl(_ url: URL?) {
        customPortUrlRelay.accept(url)
    }

    public var customPortUrl: Observable<URL?> {
        return customPortUrlRelay.asObservable()
    }

    public func setBlasterPacketSize(_ packetSize: Int) {
        let customConfiguration = BlasterService.Configuration(
            packetSize: packetSize,
            packetFormat: .ip,
            maxPacketCount: 0,
            interval: 0
        )
        customBlasterConfiguration.accept(customConfiguration)
    }
}
