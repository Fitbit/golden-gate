//
//  ReconnectingConnectionController.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/8/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import RxCocoa
import RxSwift

/// Decorates ConnectionControllerTypes with an autoconnect mechanism
/// to stay connected to a tracker even if something goes wrong.
public class ReconnectingConnectionController<Wrapped: ConnectionControllerType> {
    private let wrapped: Wrapped
    private let reconnectStrategy: ReconnectStrategy
    private let scheduler: SchedulerType

    /// An element is sent when `establishConnection` is called.
    private let resumeSubject = PublishSubject<Void>()

    public init(
        _ wrapped: Wrapped,
        reconnectStrategy: ReconnectStrategy,
        scheduler: SchedulerType = SerialDispatchQueueScheduler(qos: .default)
    ) {
        self.wrapped = wrapped
        self.reconnectStrategy = reconnectStrategy
        self.scheduler = scheduler
    }

    private lazy var sharedConnection: Completable = {
        let errorHandler = { [reconnectStrategy, resumeSubject, scheduler] (error: Error) -> Observable<Void> in
            // Wait for the event indicated by the action from the strategy
            // or `resumeSubject` (caused by `establishConnection()`) to emit a value.
            let action = reconnectStrategy.action(for: error)
            LogBindingsError("Connection encountered error: '\(error)'. Reconnect Strategy action is: \(action).")

            return Observable
                .merge(
                    action.asObservable(scheduler: scheduler)
                        .logInfo("Reconnect Strategy action triggered", .bindings, .next)
                    ,
                    resumeSubject
                        .logInfo("Resume subject triggered", .bindings, .next)
                )
                .take(1)
                .logInfo("Resuming reconnects...", .bindings, .next)
        }

        return wrapped.establishConnection()
            .asObservable()
            .retryWhen { errors in errors.flatMap(errorHandler) }
            .takeUntil(wrapped.didForceDisconnect)
            .share()
            .ignoreElements()
    }()
}

extension ReconnectingConnectionController: ConnectionControllerType {
    public typealias Stack = Wrapped.Stack
    public typealias Descriptor = Wrapped.Descriptor
    public typealias ConnectionType = Wrapped.ConnectionType

    public func establishConnection() -> Completable {
        return Completable.deferred {
            self.resumeSubject.on(.next(()))
            return self.sharedConnection
        }
    }

    public func forceDisconnect() {
        wrapped.forceDisconnect()
    }

    public func clearDescriptor() {
        wrapped.clearDescriptor()
    }

    public var didForceDisconnect: Observable<Void> {
        return wrapped.didForceDisconnect
    }
    
    public var state: Observable<ConnectionControllerState> {
        return wrapped.state
    }

    public var descriptor: Observable<Wrapped.Descriptor?> {
        return wrapped.descriptor
    }

    public var connection: Observable<ConnectionType?> {
        return wrapped.connection
    }

    public var stack: Observable<Wrapped.Stack?> {
        return wrapped.stack
    }

    public func update(descriptor: Wrapped.Descriptor) {
        wrapped.update(descriptor: descriptor)
    }
}

private extension ReconnectStrategyAction {
    func asObservable(scheduler: SchedulerType) -> Observable<Void> {
        switch self {
        case .stop(let error):
            return Observable.error(error)
        case .retryWithDelay(let delay) where delay != .never:
            return Observable.just(()).delay(delay, scheduler: scheduler)
        case .retryWithDelay:
            return Observable.just(())
        case .suspendUntil(let trigger):
            return trigger
        }
    }
}
