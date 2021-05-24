//
//  Log+Rx.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 12/7/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import RxCocoa
import RxSwift

struct LogEvents: OptionSet {
    let rawValue: UInt8

    init(rawValue: UInt8) {
        self.rawValue = rawValue
    }

    static let next = LogEvents(rawValue: 1 << 0)
    static let error = LogEvents(rawValue: 1 << 1)
    static let completed = LogEvents(rawValue: 1 << 2)
    static let disposed = LogEvents(rawValue: 1 << 3)
    static let subscribe = LogEvents(rawValue: 1 << 4)
    static let all = LogEvents(rawValue: RawValue.max)
}

/// Extension that provides a way to integrate the GG logging library
/// inside the RX chain.
///
/// Example of usage:
/// let Foo = Observable<String>.create { observer in
///      observer.onNext("Hello, log!")
///      observer.onComplete()
///      return Disposables.create()
/// }
/// .logDebug("Foo", .domain) // This will log for all events.
/// .logVerbose("Foo", .domain, .disposed) // This will log only disposed events.
/// .subscribe()
extension ObservableConvertibleType {
    /// Log as error the specified events from the current observable.
    ///
    /// - Parameters:
    ///   - identifier: The observable identifier that will be printed in the log.
    ///   - domain: The domain of the message.
    ///             A logger can use this information to group logs under the same namespace.
    ///   - events: An option set that specify what events should be logged.
    ///   - file: The file name of the file where the message was created.
    ///   - function: The function name where the message was created.
    ///   - line: The line number in the file where the message was created.
    /// - Returns: The observables where the events will be forwarded.
    func logError(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Observable<Element> {
        return log(events: events) { LogError("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    /// Log as warning the specified events from the current observable.
    ///
    /// - Parameters:
    ///   - identifier: The observable identifier that will be printed in the log.
    ///   - domain: The domain of the message.
    ///             A logger can use this information to group logs under the same namespace.
    ///   - events: An option set that specify what events should be logged.
    ///   - file: The file name of the file where the message was created.
    ///   - function: The function name where the message was created.
    ///   - line: The line number in the file where the message was created.
    /// - Returns: The observables where the events will be forwarded.
    func logWarning(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Observable<Element> {
        return log(events: events) { LogWarning("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    /// Log as info the specified events from the current observable.
    ///
    /// - Parameters:
    ///   - identifier: The observable identifier that will be printed in the log.
    ///   - domain: The domain of the message.
    ///             A logger can use this information to group logs under the same namespace.
    ///   - events: An option set that specify what events should be logged.
    ///   - file: The file name of the file where the message was created.
    ///   - function: The function name where the message was created.
    ///   - line: The line number in the file where the message was created.
    /// - Returns: The observables where the events will be forwarded.
    func logInfo(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Observable<Element> {
        return log(events: events) { LogInfo("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    /// Log as debug the specified events from the current observable.
    ///
    /// - Parameters:
    ///   - identifier: The observable identifier that will be printed in the log.
    ///   - domain: The domain of the message.
    ///             A logger can use this information to group logs under the same namespace.
    ///   - events: An option set that specify what events should be logged.
    ///   - file: The file name of the file where the message was created.
    ///   - function: The function name where the message was created.
    ///   - line: The line number in the file where the message was created.
    /// - Returns: The observables where the events will be forwarded.
    func logDebug(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Observable<Element> {
        return log(events: events) { LogDebug("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    /// Log as verbose the specified events from the current observable.
    ///
    /// - Parameters:
    ///   - identifier: The observable identifier that will be printed in the log.
    ///   - domain: The domain of the message.
    ///             A logger can use this information to group logs under the same namespace.
    ///   - events: An option set that specify what events should be logged.
    ///   - file: The file name of the file where the message was created.
    ///   - function: The function name where the message was created.
    ///   - line: The line number in the file where the message was created.
    /// - Returns: The observables where the events will be forwarded.
    func logVerbose(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Observable<Element> {
        return log(events: events) { LogVerbose("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    func log(events: LogEvents, logFunction: @escaping (Any) -> Void) -> Observable<Element> {
        guard !events.isEmpty else { return self.asObservable() }

        return asObservable().do(
            onNext: events.contains(.next) ? { logFunction($0) } : nil,
            onError: events.contains(.error) ? { logFunction($0) } : nil,
            onCompleted: events.contains(.completed) ? { logFunction("(completed)") } : nil,
            onSubscribe: events.contains(.subscribe) ? { logFunction("(subscribing)") } : nil,
            onDispose: events.contains(.disposed) ? { logFunction("(disposed)") } : nil
        )
    }
}

/// Extension that provides a way to integrate the GG logging library
/// with Completables inside RX chain.
extension PrimitiveSequence where Trait == CompletableTrait, Element == Swift.Never {
    func logError(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Completable {
        return asObservable()
            .logError(identifier, domain, events, file: file, function: function, line: line)
            .ignoreElements()
    }

    func logWarning(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Completable {
        return asObservable()
            .logWarning(identifier, domain, events, file: file, function: function, line: line)
            .ignoreElements()
    }

    func logInfo(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Completable {
        return asObservable()
            .logInfo(identifier, domain, events, file: file, function: function, line: line)
            .ignoreElements()
    }

    func logDebug(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Completable {
        return asObservable()
            .logDebug(identifier, domain, events, file: file, function: function, line: line)
            .ignoreElements()
    }

    func logVerbose(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Completable {
        return asObservable()
            .logVerbose(identifier, domain, events, file: file, function: function, line: line)
            .ignoreElements()
    }
}

extension SharedSequenceConvertibleType where SharingStrategy == DriverSharingStrategy {
    func logError(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Driver<Element> {
        return log(events: events) { LogError("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    func logWarning(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Driver<Element> {
        return log(events: events) { LogWarning("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    func logInfo(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Driver<Element> {
        return log(events: events) { LogInfo("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    func logDebug(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Driver<Element> {
        return log(events: events) { LogDebug("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    func logVerbose(
        _ identifier: String,
        _ domain: LoggerDomain,
        _ events: LogEvents = .all,
        file: StaticString = #file,
        function: StaticString = #function,
        line: UInt = #line
    ) -> Driver<Element> {
        return log(events: events) { LogVerbose("\(identifier) \($0)", file: file, function: function, line: line, domain: domain) }
    }

    func log(events: LogEvents, logFunction: @escaping (Any) -> Void) -> Driver<Element> {
        guard !events.isEmpty else { return self.asDriver() }

        return asDriver().do(
            onNext: events.contains(.next) ? { logFunction($0) } : nil,
            onCompleted: events.contains(.completed) ? { logFunction("(completed)") } : nil,
            onSubscribe: events.contains(.subscribe) ? { logFunction("(subscribing)") } : nil,
            onDispose: events.contains(.disposed) ? { logFunction("(disposed)") } : nil
        )
    }
}
