//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGPeer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/11/18.
//

import BluetoothConnection
import Foundation
import RxRelay
import RxSwift

/// A peer that exposes CoAP and Blaster services running on top of a Golden Gate stack.
open class GGPeer: Peer {
    /// The peer's current stack setup.
    public let stackDescriptor: Observable<StackDescriptor>

    /// The peer's current service setup.
    public let serviceDescriptor: Observable<ServiceDescriptor>

    /// The peer's current blaster configuration.
    public let blasterConfiguration: Observable<BlasterService.Configuration>

    /// Emits when activity change was detected at the transport layer.
    public let transportActivityChange: Observable<(ActivityMonitorDirection, Bool)>

    /// Emits when gattlink session stalls.
    public let disconnectionRequired: Observable<Void>

    /// Emits when gattlink buffer size crosses the fullness threshold
    public let bufferFullness: Observable<BufferFullness>

    /// The peer's blaster service.
    public let blasterService: BlasterService

    /// The peer's CoAP client service.
    private(set) public var coapClientService: CoapClientService?

    // Observable that emits stack events
    private let stackEvents = PublishSubject<StackEvent?>()

    /// The peer's CoAP service.
    public let coapEndpoint: CoapEndpointType

    private let runLoop: RunLoop
    private let customStackDescriptor = BehaviorRelay<StackDescriptor?>(value: nil)
    private let customServiceDescriptor = BehaviorRelay<ServiceDescriptor?>(value: nil)
    private let customBlasterConfiguration = BehaviorRelay<BlasterService.Configuration?>(value: nil)
    private let customPortUrlRelay = BehaviorRelay<URL?>(value: nil)

    /// The current port used by application level protocols, i.e. the stack's top port or a custom port.
    private let portRelay = BehaviorRelay<Port?>(value: nil)

    private let disposeBag = DisposeBag()

    /// Minimum stalled time threshold beyond which session stalled will be reported.
    private static let gattlinkStalledTimeout: TimeInterval = 60

    fileprivate let stackRelay = BehaviorRelay<StackType?>(value: nil)
    public var stack: Observable<StackType?> {
        return stackRelay.asObservable()
            .distinctUntilChanged(nilComparer)
    }

    // swiftlint:disable:next function_body_length
    public init(
        _ parameters: PeerParameters,
        networkLink: Observable<NetworkLink?>,
        runLoop: RunLoop,
        globalBlasterConfiguration: Observable<BlasterService.Configuration>
    ) {
        self.runLoop = runLoop

        if let urlString = ProcessInfo.processInfo.environment["GG_TOP_PORT"] {
            customPortUrlRelay.accept(URL(string: urlString))
        }

        // Prefer custom service descriptor and fall back to global
        self.stackDescriptor = Observable
            .combineLatest(
                customStackDescriptor,
                parameters.stackDescriptor
            )
            .map { $0.0 ?? $0.1 }

        // Prefer custom service descriptor and fall back to global
        self.serviceDescriptor = Observable
            .combineLatest(
                customServiceDescriptor,
                parameters.serviceDescriptor
            )
            .map { $0.0 ?? $0.1 }
            .distinctUntilChanged()

        // Prefer custom blaster configuration and fall back to global
        self.blasterConfiguration = Observable
            .combineLatest(
                customBlasterConfiguration,
                globalBlasterConfiguration
            )
            .map { $0.0 ?? $0.1 }

        self.transportActivityChange = stackEvents
            .compactMap { event -> (ActivityMonitorDirection, Bool)? in
                if let event = event, case let .activityMonitorChanged(direction, active) = event {
                    return (direction, active)
                }
                return nil
            }

        self.disconnectionRequired = stackEvents
            .compactMap { event -> TimeInterval? in
                if let event = event, case .gattlinkSessionStalled(let stalledTime) = event {
                    return stalledTime
                }
                return nil
            }
            // Report stalled session after the stalled time exceeds a given threshold.
            .filter { $0 >= Self.gattlinkStalledTimeout }
            .logWarning("GGPeer: Gattlink session stalled. Stalled time > \(Self.gattlinkStalledTimeout) s.", .bindings, .next)
            .map { _ in () }

        self.bufferFullness = stackEvents
            .compactMap { event -> BufferFullness? in
                switch event {
                case .gattlinkBufferOverThreshold: return .overThreshold
                case .gattlinkBufferUnderThreshold: return .underThreshold
                default: return nil
                }
            }

        let transportReadiness = customPortUrlRelay
            .flatMapLatest {
                // Always allow requests when using a custom port.
                $0 == nil ? parameters.defaultPortTransportReadiness : .just(.ready)
            }

        let coapEndpoint = runLoop.sync { [runLoop, serviceDescriptor, port = portRelay.asObservable()] in
            // swiftlint:disable:next force_try
            try! CoapEndpoint(
                runLoop: runLoop,
                port: serviceDescriptor
                    .flatMapLatest { descriptor in
                        // Detach port from CoapEndpoint when blasting
                        descriptor == .coap ? port : .just(nil)
                    }
                    .distinctUntilChanged(nilComparer),
                transportReadiness: transportReadiness
            )
        }

        self.coapEndpoint = coapEndpoint

        self.blasterService = runLoop.sync { [blasterConfiguration, runLoop, serviceDescriptor, port = portRelay.asObservable()] in
            // swiftlint:disable:next force_try
            let service = try! BlasterService(
                runLoop: runLoop,
                configuration: blasterConfiguration,
                port: serviceDescriptor
                    .flatMapLatest { descriptor in
                        descriptor == .blasting ? port : .just(nil)
                    }
            )

            return service
        }

        let stackBuilder = parameters.stackBuilderProvider(stackDescriptor.asObservable())

        // Create new stack as links come up
        stackBuilder.build(link: networkLink)
            .logError("GGPeer: Failed to start stack with: ", .bindings, .error)
            .catchErrorJustReturn(nil)
            .bind(to: stackRelay)
            .disposed(by: disposeBag)

        // Forward stack events.
        stack
            .map { $0 as? StackEventEmitter }
            .flatMapLatestForwardingNil { $0.event }
            .bind(to: stackEvents)
            .disposed(by: disposeBag)

        // Resolve the URL and create a socket from it
        let customPort = customPortUrl
            .observeOn(SerialDispatchQueueScheduler(qos: .background))
            .map { $0.flatMap { SocketAddress(url: $0) } }
            .distinctUntilChanged(==)
            .observeOn(runLoop)
            .logInfo("GGPeer: CustomPortUrl", .bindings, .next)
            .mapForwardingNil { remoteAddress in
                try? GGBsdDatagramSocket(
                    runLoop: runLoop,
                    localAddress: SocketAddress(address: .any, port: 5683),
                    remoteAddress: remoteAddress
                )
            }
            .logInfo("GGPeer: CustomPortUrl GGBsdDatagramSocket", .bindings, .next)

        // Whenever the service descriptor changes or we are switching to the `customPort`,
        // clear out any references the XP port might have. (See: FC-1046)
        Observable
            .combineLatest(stack, customPort, serviceDescriptor)
            .observeOn(runLoop)
            .map { stack, customPort, _ in
                // It would be nice if the services could clean up after themselves,
                // but they can't know if some other new service already added itself,
                // leading to potential races.
                _ = try? stack?.topPort?.dataSink.setDataSinkListener(nil)
                _ = try? stack?.topPort?.dataSource.setDataSink(nil)

                return customPort ?? stack?.topPort
            }
            .subscribe(onNext: portRelay.accept)
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

extension GGPeer {
    public static var supportedStackDescriptors: [StackDescriptor] = [
        .dtlsSocketNetifGattlinkActivity,
        .dtlsSocketNetifGattlink,
        .socketNetifGattlink,
        .netifGattlink,
        .gattlink
    ]

    public static var supportedServiceDescriptors: [ServiceDescriptor] = [
        ServiceDescriptor.none,
        .coap,
        .blasting
    ]
}
