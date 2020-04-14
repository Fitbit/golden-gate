//
//  ConnectionController.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/3/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import Rxbit
import RxCocoa
import RxSwift

public enum ConnectionControllerState: Int {
    case disconnected, connecting, connected
}

/// Protocol that reports new connections status.
public protocol ConnectionStateEmitter {
    // The connection status.
    var state: Observable<ConnectionControllerState> { get }
}

/// Protocol that reports new connections.
public protocol ConnectionEmitter {
    associatedtype ConnectionType

    // Access to the connection that we are trying to establish.
    // Will be populated before the actual link becomes available.
    var connection: Observable<ConnectionType?> { get }
}

/// Protocol that offer the possibility of establishing and terminate a connection.
public protocol ConnectionEstablisher {
    /// Returns an observable that (when subscribed to) will try to
    /// establish and maintain a connection until the subscription is disposed.
    ///
    /// If multiple clients call this API, the connection will only
    /// be released if all clients disposed of their subscription.
    ///
    /// The observable will only error if the descriptor became invalid,
    /// or the remote device seems to be misbehaving, so retrying is not recommended.
    func establishConnection() -> Completable

    /// Cancels all connection intents.
    func forceDisconnect()

    /// It emits Void when forceDisconnect is called.
    var didForceDisconnect: Observable<Void> { get }
}

/// Protocol that handles the device's descriptor.
public protocol ConnectionDescriptorManager {
    associatedtype Descriptor: Hashable

    // Contains the `descriptor` (some persistent address) of
    // the device that a connection should be established with.
    //
    // It emits `nil` if someone calls `clearDescriptor()`,
    // or when the previous `descriptor` becomes invalid
    // and will be useless from now on.
    var descriptor: Observable<Descriptor?> { get }

    /// Cancels all connection intents, sets the `descriptor` to `nil`.
    func clearDescriptor()

    /// Sets the `descriptor`.
    /// This is useful when the `descriptor` has become invalid,
    /// but some other system was able to retrieve a new `descriptor`
    /// that can be used from now on again.
    func update(descriptor: Descriptor)
}

/// Protocol that emits stacks.
public protocol ConnectionStackEmitter {
    associatedtype Stack: StackType

    /// Allows access to the potentially custom stack.
    var stack: Observable<Stack?> { get }
}

/// Provides easy management of a connection to a peer.
public protocol ConnectionControllerType: ConnectionStackEmitter, ConnectionDescriptorManager, ConnectionEmitter, ConnectionStateEmitter, ConnectionEstablisher {}

public final class ConnectionController<D: Hashable, S: StackType>: ConnectionControllerType {
    public typealias Descriptor = D
    public typealias Stack = S

    private let runLoop: RunLoop
    private let connector: (Descriptor) -> Observable<Connection>
    private let disposeBag = DisposeBag()
    private let scheduler: SchedulerType

    fileprivate let didForceDisconnectSubject = PublishSubject<Void>()
    public var didForceDisconnect: Observable<Void> {
        return didForceDisconnectSubject.asObservable()
    }

    fileprivate let stateRelay = BehaviorRelay(value: ConnectionControllerState.disconnected)
    public var state: Observable<ConnectionControllerState> {
        return stateRelay.asObservable()
    }

    fileprivate let descriptorRelay: BehaviorRelay<Descriptor?>
    public var descriptor: Observable<Descriptor?> {
        return descriptorRelay.asObservable()
    }

    fileprivate let connectionRelay = BehaviorRelay<Connection?>(value: nil)
    public var connection: Observable<Connection?> {
        return connectionRelay.asObservable()
            .distinctUntilChanged(nilComparer)
    }

    fileprivate let networkLinkRelay = BehaviorRelay<NetworkLink?>(value: nil)
    fileprivate var networkLink: Observable<NetworkLink?> {
        return networkLinkRelay.asObservable()
            .distinctUntilChanged(nilComparer)
    }

    fileprivate let stackRelay = BehaviorRelay<Stack?>(value: nil)
    public var stack: Observable<Stack?> {
        return stackRelay.asObservable()
            .distinctUntilChanged(nilComparer)
    }

    public init<C: Connector, SB: StackBuilderType>(
        runLoop: RunLoop,
        connector: C,
        stackBuilder: SB,
        descriptor: Descriptor?,
        scheduler: SchedulerType = SerialDispatchQueueScheduler(qos: .default)
    ) throws where C.Descriptor == Descriptor, SB.Stack == Stack, C.ConnectionType == ConnectionType {
        self.runLoop = runLoop
        self.connector = connector.establishConnection
        self.scheduler = scheduler
        self.descriptorRelay = BehaviorRelay<Descriptor?>(value: descriptor)

        // Create new stack as links come up
        stackBuilder.build(link: networkLink)
            .do(onError: { [weak self] error in
                LogBindingsError("\(self ??? "ConnectionController"): Failed to start stack with \(error)")
            })
            .catchErrorJustReturn(nil)
            .bind(to: stackRelay)
            .disposed(by: disposeBag)

        // Expose a simplified state
        connection
            .flatMapLatest { connection -> Observable<ConnectionControllerState> in
                return connection?.networkLinkStatus.asObservable().map {
                    if case .connected = $0 {
                        return .connected
                    } else {
                        return .connecting
                    }
                } ?? .just(.disconnected)
            }
            .distinctUntilChanged()
            .do(onNext: { [weak self] state in
                LogBindingsInfo("\(self ??? "ConnectionController"): Connection state \(state)")
            })
            .subscribe(onNext: stateRelay.accept)
            .disposed(by: disposeBag)

        // Create new links as connections are established
        connection
            .do(onNext: { [weak self] connection in
                LogBindingsInfo("\(self ??? "ConnectionController"): Connection status \(connection ??? "nil")")
            })
            .flatMapLatestForwardingNil { $0.networkLinkStatus }
            .map { $0?.networkLink }
            .subscribe(onNext: networkLinkRelay.accept)
            .disposed(by: disposeBag)
    }

    public func establishConnection() -> Completable {
        return sharedConnection
            .asObservable()
            .takeUntil(didForceDisconnect.asObservable())
            .ignoreElements()
    }

    public func forceDisconnect() {
        LogBindingsWarning("\(self): Forcing a disconnect.")
        didForceDisconnectSubject.on(.next(()))
    }

    public func update(descriptor: Descriptor) {
        LogBindingsInfo("\(self): Updating descriptor for connection \(descriptor)")
        self.descriptorRelay.accept(descriptor)
    }

    public func clearDescriptor() {
        descriptorRelay.accept(nil)
    }

    private lazy var sharedConnection: Completable = {
        return descriptor
            .observeOn(SerialDispatchQueueScheduler(qos: .default))
            .distinctUntilChanged(==)
            .flatMapLatest { [weak self, connector = self.connector] descriptor -> Observable<Connection?> in
                LogBindingsDebug("\(self ??? "ConnectionController"): Received new descriptor.")

                return Observable.deferred { () -> Observable<Connection?> in
                    if let descriptor = descriptor {
                        return connector(descriptor).optionalize().startWith(nil)
                    } else {
                        return .just(nil)
                    }
                }.catchError { [weak self] error in
                    if case CentralManagerError.identifierUnknown = error {
                        LogBindingsWarning("\(self ??? "ConnectionController"): Identifier unknown - clearing descriptor.")
                        self?.clearDescriptor()
                        return .just(nil as Connection?)
                    } else {
                        return .error(error)
                    }
                }
            }
            .do(onNext: { [weak self] value in
                self?.connectionRelay.accept(value)
                LogBindingsDebug("\(self ??? "ConnectionController"): SharedConnection new value \(String(describing: value))")
            }, onError: { [weak self] error in
                LogBindingsError("\(self ??? "ConnectionController"): SharedConnection failed with \(error)")
            }, onDispose: { [weak self] in
                LogBindingsDebug("\(self ??? "ConnectionController"): SharedConnection got disposed")
                self?.connectionRelay.accept(nil)
            })
            .share()
            .ignoreElements()
    }()
}

extension ConnectionController: CustomStringConvertible {
    public var description: String {
        return "ConnectionController(descriptor=\(descriptorRelay.value ??? "nil"))"
    }
}
