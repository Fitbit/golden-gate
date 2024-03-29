//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ConnectionController.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 11/3/17.
//

import Foundation
import Rxbit
import RxRelay
import RxSwift

/// Protocol that reports connection statuses. When connected, the status holds some connection.
public protocol ConnectionEmitter {
    associatedtype ConnectionType

    /// Access to the connection status and the underlying connection that we are trying to establish. Replays the last value (if any) on subscribe.
    var connectionStatus: Observable<ConnectionStatus<ConnectionType>> { get }

    /// Observable that emits a non-nil value when a connection error occurs or nil otherwise. Most of these errors will be handled
    /// by the reconnect strategy, but some are unrecoverable. Replays the last value (if any) on subscribe.
    var connectionError: Observable<Error?> { get }
}

/// A descriptor for a connection request.
public struct ConnectionTrigger: Hashable, CustomStringConvertible, RawRepresentable, ExpressibleByStringLiteral {
    public let rawValue: String

    public init(rawValue: String) {
        self.rawValue = rawValue
    }

    public init(stringLiteral value: StringLiteralType) {
        self.init(rawValue: value)
    }

    public var description: String {
        return "(trigger=\(rawValue))"
    }
}

/// Protocol that offers the possibility of establishing or terminating a connection.
public protocol ConnectionEstablisher {
    /// Attempts to establish and maintain a connection until encountering an unrecoverable
    /// failure or disconnection intent. The trigger helps collect better metrics.
    ///
    /// Only one connection at a time is allowed; that means calling this method has no effect
    /// when another connection attempt is in progress.
    ///
    /// Observe `ConnectionEmitter.connectionErrors` to be notified when connection
    /// errors occur.
    func establishConnection(trigger: ConnectionTrigger)

    /// Cancels all connection intents. The trigger helps collect better metrics.
    func disconnect(trigger: ConnectionTrigger)
}

/// Protocol that handles the device's descriptor.
public protocol ConnectionDescriptorManager {
    // Contains the `descriptor` (some persistent address) of
    // the device that a connection should be established with.
    //
    // It emits `nil` if someone calls `clearDescriptor()`,
    // or when the previous `descriptor` becomes invalid
    // and will be useless from now on.
    var descriptor: Observable<PeerDescriptor?> { get }

    /// Cancels all connection intents, sets the `descriptor` to `nil`.
    func clearDescriptor()

    /// Sets the `descriptor`.
    /// This is useful when the `descriptor` has become invalid,
    /// but some other system was able to retrieve a new `descriptor`
    /// that can be used from now on again.
    func update(descriptor: PeerDescriptor)
}

/// Provides easy management of a connection to a peer.
public protocol ConnectionControllerType: ConnectionDescriptorManager, ConnectionEmitter, ConnectionEstablisher {}

public enum ConnectionControllerMetricsEvent: MetricsEvent {
    case connectionRequested(trigger: ConnectionTrigger)
    case disconnectionRequested(trigger: ConnectionTrigger)

    case stateChangedToConnecting
    case stateChangedToConnected
    case stateChangedToDisconnected

    case encounteredError(Error)
    case determinedReconnectStrategyAction(RetryStrategyAction)
}

/// An auto-connecting ConnectionController.
public final class ConnectionController<ConnectionType>: ConnectionControllerType, Instrumentable {
    private let scheduler: SchedulerType
    private let debugIdentifier: String
    private let disposeBag = DisposeBag()

    /// Subject that broadcasts demands to establish or cancel a connection.
    private let connectionRequested = PublishSubject<Bool>()

    fileprivate let descriptorRelay: BehaviorRelay<PeerDescriptor?>
    public var descriptor: Observable<PeerDescriptor?> {
        return descriptorRelay.asObservable()
    }

    fileprivate let connectionStatusSubject = ReplaySubject<ConnectionStatus<ConnectionType>>.create(bufferSize: 1)
    public var connectionStatus: Observable<ConnectionStatus<ConnectionType>> {
        return connectionStatusSubject.asObservable()
    }

    private let connectionErrorSubject = ReplaySubject<Error?>.create(bufferSize: 1)
    public var connectionError: Observable<Error?> {
        return connectionErrorSubject.asObservable()
    }

    private let metricsSubject = PublishSubject<ConnectionControllerMetricsEvent>()
    public var metrics: Observable<ConnectionControllerMetricsEvent> {
        metricsSubject.asObservable()
            .observe(on: scheduler)
    }

    /// Creates an auto-connecting connection controller.
    /// - Parameters:
    ///   - connector: An object that can establish a connection to a peripheral
    ///   - descriptor: The descriptor used by the peripheral
    ///   - reconnectStrategy: Strategy that dictates the connection retry behavior
    ///   - scheduler: Scheduler to establish the connection on
    ///   - debugIdentifier: A human readable string used to identify logs from a specific instance
    public init<C: Connector>(
        connector: C,
        descriptor: PeerDescriptor?,
        reconnectStrategy: RetryStrategy,
        scheduler: SchedulerType,
        debugIdentifier: String
    ) where C.ConnectionType == ConnectionType {
        self.scheduler = scheduler
        self.debugIdentifier = debugIdentifier
        self.descriptorRelay = BehaviorRelay<PeerDescriptor?>(value: descriptor)

        let encounteredUnrecoverableFailure = PublishSubject<Void>()

        // Observable that establishes a connection to the specified peripheral and maintains
        // a connection according with the policy dictated by the reconnect strategy until
        // an unrecoverable failure is encountered. The observable emits connection statuses
        // and writes all the errors into another subject.
        let establishConnection: Observable<ConnectionStatus<ConnectionType>> = self.descriptor
            .distinctUntilChanged()
            .logInfo("ConnectionController<\(debugIdentifier)>: Establish connection using descriptor", .bluetooth, .next)
            .observe(on: scheduler)
            .flatMapLatest { descriptor -> Observable<ConnectionStatus<ConnectionType>> in
                guard let descriptor = descriptor else { return .just(.disconnected) }

                return connector.establishConnection(descriptor: descriptor)
                    .map { status -> ConnectionStatus<ConnectionType> in
                        switch status {
                        // Disconnections must be caught and retried
                        case .disconnected: throw ConnectorError.disconnected
                        default: return status
                        }
                    }
                    .startWith(.connecting)
                    .catch { Observable.just(.disconnected).concat(Observable.error($0)) }
                    .logError("ConnectionController<\(debugIdentifier)>: Connection error", .bluetooth, .error)
            }
            .do(
                onError: { [weak self] error in
                    self?.connectionErrorSubject.onNext(error)

                    if case CentralManagerError.identifierUnknown = error {
                        self?.clearDescriptor()
                    }
                },
                onSubscribe: { [connectionErrorSubject] in connectionErrorSubject.onNext(nil) }
            )
            .retry { [metricsSubject] errors in
                errors.map(reconnectStrategy.action)
                    .logError("ConnectionController<\(debugIdentifier)>: Reconnect strategy action", .bluetooth, .next)
                    .do(onNext: { metricsSubject.onNext(.determinedReconnectStrategyAction($0)) })
                    .flatMap { $0.asSingle(scheduler: scheduler) }
                    .logInfo("ConnectionController<\(debugIdentifier)>: Resuming reconnects...", .bluetooth, .next)
            }
            .do(afterError: { _ in encounteredUnrecoverableFailure.onNext(()) })
            .catchAndReturn(.disconnected)

        // Observable that emits `true` if a connection is required and can be established and
        // maintained or `false` otherwise.
        let shouldConnect = Observable.merge(connectionRequested, encounteredUnrecoverableFailure.map { _ in false })
            .logInfo("ConnectionController<\(debugIdentifier)>: Should connect", .bluetooth, .next)
            .observe(on: scheduler)
            .distinctUntilChanged()

        Observable.if(shouldConnect, then: establishConnection, else: .just(.disconnected))
            .startWith(.disconnected)
            .distinctUntilChanged()
            .logInfo("ConnectionController<\(debugIdentifier)>: New connection status", .bluetooth, .next)
            .bind(to: connectionStatusSubject)
            .disposed(by: disposeBag)

        // Reset reconnection failure history on successful connection
        connectionStatus.filter { $0.connected }
            .do(onNext: { _ in reconnectStrategy.resetFailureHistory() })
            .subscribe()
            .disposed(by: disposeBag)

        connectionStatus
            .map { status -> ConnectionControllerMetricsEvent in
                switch status {
                case .disconnected: return .stateChangedToDisconnected
                case .connecting: return .stateChangedToConnecting
                case .connected: return .stateChangedToConnected
                }
            }
            .bind(to: metricsSubject)
            .disposed(by: disposeBag)

        connectionError
            .filterNil()
            .map { ConnectionControllerMetricsEvent.encounteredError($0) }
            .bind(to: metricsSubject)
            .disposed(by: disposeBag)
    }

    public func establishConnection(trigger: ConnectionTrigger) {
        LogBluetoothInfo("ConnectionController<\(debugIdentifier)>: Connection requested with \(trigger)")
        metricsSubject.onNext(.connectionRequested(trigger: trigger))
        connectionRequested.onNext(true)
    }

    public func disconnect(trigger: ConnectionTrigger) {
        LogBluetoothWarning("ConnectionController<\(debugIdentifier)>: Disconnection requested with \(trigger)")
        metricsSubject.onNext(.disconnectionRequested(trigger: trigger))
        connectionRequested.onNext(false)
    }

    public func update(descriptor: PeerDescriptor) {
        LogBluetoothInfo("ConnectionController<\(debugIdentifier)>: Updating descriptor for connection \(descriptor)")
        self.descriptorRelay.accept(descriptor)
    }

    public func clearDescriptor() {
        descriptorRelay.accept(nil)
    }

    deinit {
        LogBluetoothInfo("ConnectionController<\(debugIdentifier)>: deinited")
    }
}

extension ConnectionController: CustomStringConvertible {
    public var description: String {
        return "ConnectionController<\(debugIdentifier)>(descriptor=\(descriptorRelay.value ??? "nil"))"
    }
}

extension ConnectionEmitter {
    /// The peripheral's half bonded state. iOS will not connect to a peripheral which is
    /// bonded only on the phone side. Usually, peripherals that lose the long term keys
    /// get into half bonded state.
    public var isHalfBonded: Observable<Bool> {
        // Assuming the peripheral is half bonded (not connectable) when a
        // `CBError.peerRemovedPairingInformation` error was received while trying to connect
        Observable.merge(
            connectionStatus
                .filter { $0.connected }
                .map { _ in false },
            connectionError
                .filter { $0?.isBluetoothHalfBondedError == true }
                .map { _ in true }
        )
    }
}
