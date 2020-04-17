//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RemoteShell.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/6/17.
//

import FbSmo
import GoldenGateXP
import RxSwift

public enum RemoteShellError: Error {
    case invalidParameters
}

public class RemoteShell {
    typealias Ref = OpaquePointer?

    internal var ref: Ref = nil
    var transport: RemoteTransport
    var handlers: [String: RemoteShellHandler] = [:]

    /// Queue used to run the shell. The shell waits for requests in an infinite loop, effectively blocking the queue it runs on.
    private let runnerQueue = DispatchQueue(label: "remote_shell_runner")
    private let synchronizationQueue = DispatchQueue(label: "remote_shell_synchronization")

    /// Create a remote shell with a given transport.
    ///
    /// - Parameter transportLayer: The transport that will be used for sending and receiving requests.
    public init(transportLayer: TransportLayer) {
        self.transport = RemoteTransport(transportLayer: transportLayer)

        _ = synchronizationQueue.sync {
            GG_RemoteShell_Create(transport.ref, &ref)
        }
    }

    deinit {
        GG_RemoteShell_Destroy(ref)
    }

    /// Start the remote shell.
    public func start() {
        self.transport.connect()

        // The shell will block the runner queue forever
        runnerQueue.async { [weak self] in
            guard let `self` = self else { return }
            GG_RemoteShell_Run(self.ref)
        }
    }

    /// Register a handler that will be called when a request is received.
    /// The handler is responsible for providing the response.
    ///
    /// - Parameters:
    ///   - method: The method with which the handler is registered.
    ///   - handler: The handler that will handle the request and provide a response.
    ///              The handler receives a CBOR object representing the parameters of the request and
    ///              it returns a Single. The response will be sent when the Single will emit.
    ///              If an error occured, the Single must emit that error.
    public func register(_ method: String, handler: @escaping RequestHandler) {
        register(method, handler: RemoteShellHandler(callback: handler))
    }
}

// MARK: - Parameterless

extension RemoteShell {
    public func register(_ method: String, handler: @escaping () throws -> Void) {
        register(method) { () -> Completable in
            return .deferred {
                try handler()
                return Completable.empty()
            }
        }
    }

    public func register(_ method: String, handler: @escaping () throws -> Completable) {
        let handler = RemoteShellHandler(callback: { _ in
            return try handler().andThen(Maybe.empty())
        })

        register(method, handler: handler)
    }

    public func register<Result: Encodable>(_ method: String, handler: @escaping () throws -> Single<Result>) {
        let handler = RemoteShellHandler(callback: { _ in
            return try handler()
                .map(encode)
                .asObservable()
                .asMaybe()
        })

        register(method, handler: handler)
    }
}

// MARK: - With Parameters

extension RemoteShell {
    public func register<Params: Decodable>(_ method: String, handler: @escaping (Params) throws -> Void) {
        register(method) { (params: Params) -> Completable in
            return .deferred {
                try handler(params)
                return Completable.empty()
            }
        }
    }

    public func register<Params: Decodable>(_ method: String, handler: @escaping (Params) throws -> Completable) {
        let handler = RemoteShellHandler(callback: { rawInput in
            return try handler(try decode(rawInput))
                .andThen(Maybe.empty())
        })

        register(method, handler: handler)
    }

    public func register<Params: Decodable, Result: Encodable>(_ method: String, handler: @escaping (Params) throws -> Single<Result>) {
        let handler = RemoteShellHandler(callback: { rawInput in
            return try handler(try decode(rawInput))
                .map(encode)
                .asObservable()
                .asMaybe()
        })

        register(method, handler: handler)
    }
}

private extension RemoteShell {
    func register(_ method: String, handler: RemoteShellHandler) {
        synchronizationQueue.async { [weak self] in
            guard let self = self else { return }
            self.handlers[method] = handler

            do {
                try method.withCString { bytes in
                    try GG_RemoteShell_RegisterCborHandler(self.ref, bytes, handler.ref).rethrow()
                }
            } catch {
                LogBindingsError("Could not register handler for \(method): \(error)")
            }
        }
    }

    func unregister(_ method: String) {
        synchronizationQueue.async { [weak self] in
            guard
                let self = self,
                let handler = self.handlers[method]
            else { return }

            do {
                try method.withCString { bytes in
                    try GG_RemoteShell_UnregisterCborHandler(self.ref, bytes, handler.ref).rethrow()
                }
            } catch {
                LogBindingsError("Could not unregister handler for \(method): \(error)")
            }
            self.handlers.removeValue(forKey: method)
        }
    }
}

// MARK: - RemoteApiModule

/// An object that contains and publishes/unpublishes RemoteApi handlers.
public protocol RemoteApiModule: class {
    /// List of methods publishable by the module
    var methods: Set<String> { get }

    /// Publish the `RemoteApiModule`'s handlers to a `RemoteShell`.
    /// - Parameter remoteShell: The `RemoteShell` from where the `RemoteApiModule`'s
    ///                          handlers will be published to.
    func publishHandlers(on remoteShell: RemoteShell)

    /// Unpublish the `RemoteApiModule`'s handlers from a `RemoteShell`.
    /// - Parameter remoteShell: The `RemoteShell` from where the `RemoteApiModule`'s
    ///                          handlers will be unpublished from.
    func unpublishHandlers(from remoteShell: RemoteShell)
}

public extension RemoteApiModule {
    // Provide a default implementation that unregisters all publishable methods
    func unpublishHandlers(from remoteShell: RemoteShell) {
        methods.forEach(remoteShell.unregister)
    }
}

// MARK: - Coding helpers
// Not on RemoteShell to avoid capturing `self`

private func encode<T: Encodable>(_ value: T) throws -> Data {
    let encoder = FbSmoEncoder()
    encoder.keyEncodingStrategy = .convertToSnakeCase

    return try encoder.encode(value, using: .cbor)
}

private func decode<T: Decodable>(_ data: Data?) throws -> T {
    let decoder = FbSmoDecoder()
    decoder.keyDecodingStrategy = .convertFromSnakeCase

    if let data = data {
        return try decoder.decode(T.self, from: data, using: .cbor)
    } else {
        return try decoder.decode(T.self, from: .object([:]))
    }
}
