//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RemoteTestServer.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/26/18.
//

// swiftlint:disable multiple_closures_with_trailing_closure

import GoldenGateXP
import RxCocoa
import RxSwift
import Starscream

/// An object that allows registration of RPC handlers.
///
/// Can be used to allow an external client (e.g. Test Automation)
/// to trigger behavior inside the app without going through the UI.
public protocol RemoteTestServerType {
    /// Modules which export methods that can be invoked by external
    /// clients via RPC. Each module provides handlers for each
    /// method they export.
    var modules: [RemoteApiModule] { get }

    /// Registers a `RemoteApiModule` on the server.
    /// - Parameter module: The module to register.
    func register(module: RemoteApiModule)

    /// Unregisters a `RemoteApiModule` on the server.
    /// - Parameter module: The module to unregister.
    func unregister(module: RemoteApiModule)

    /// Start receiving and dispatching methods.
    func start()
}

public class RemoteTestServer: RemoteTestServerType {
    public private(set) var modules: [RemoteApiModule] = []
    private let transport: TransportLayer
    private let queue: DispatchQueue
    private let shell: RemoteShell
    private var running = false

    /// Creates a new instance that connects to a WebSocket server
    /// only if an environment variable is set to a URL.
    ///
    /// - Parameter environmentVariableName: The name of the environment variable to look up.
    public convenience init?(environmentVariableName: String = "RELAY_URL") {
        let environment = ProcessInfo.processInfo.environment

        guard
            let relayUrlString = environment[environmentVariableName] ??
                // Backwards-compatibility with `relay_url`
                environment[environmentVariableName.lowercased()]
        else {
            return nil
        }

        guard let relayUrl = URL(string: relayUrlString) else {
            LogBindingsError("Couldn't parse ENV[\(environmentVariableName)] as URL: \(relayUrlString)")
            return nil
        }

        self.init(url: relayUrl)
    }

    /// Creates a new instance that connects to a WebSocket server.
    ///
    /// It will retry to connect to this endpoint forever.
    ///
    /// - Parameter url: The URL to the WebSocket server.
    public convenience init(url: URL) {
        let transport = LazyReconnectingWebsocketTransport(url: url)
        let queue = DispatchQueue(label: "RemoteTestServer-\(url.absoluteString)")
        self.init(transport: transport, queue: queue)
    }

    /// Creates a new instance.
    ///
    /// - Parameters:
    ///   - transport: The transport for sending and receiving raw data.
    ///   - queue: The dispatch queue on which raw data should be sent and received.
    init(transport: TransportLayer, queue: DispatchQueue) {
        self.transport = transport
        self.queue = queue
        self.shell = RemoteShell(transportLayer: transport)

        // For debug purposes:
        shell.register("helloworld", handler: helloworld)
    }

    public func register(module: RemoteApiModule) {
        modules.append(module)
        module.publishHandlers(on: shell)
    }

    public func unregister(module: RemoteApiModule) {
        module.unpublishHandlers(from: shell)
        modules.removeAll { $0 === module }
    }

    public func start() {
        queue.async {
            guard !self.running else { return }
            defer { self.running = true }

            self.shell.start()
        }
    }

    /// Register a single method on the server.
    ///
    /// - Parameters:
    ///   - method: The name of the method.
    ///   - handler: The method to execute.
    public func register(_ method: String, handler: @escaping RequestHandler) {
        queue.async {
            self.shell.register(method, handler: handler)
        }
    }

    private func helloworld() -> Single<String> {
        LogBindingsInfo("Hello world!")
        return .just("helloworld!")
    }
}

/// A TransportLayer that connects (and keeps reconnecting)
/// to a WebSocket server.
private class LazyReconnectingWebsocketTransport: TransportLayer {
    private let url: URL
    private var transport: WebsocketTransport?

    init(url: URL) {
        self.url = url
    }

    public var didReceiveData: Observable<Data> {
        return Observable
            .deferred { .just(WebsocketTransport(url: self.url)) }
            .do(onNext: { $0.connect() })
            // wait for connection event
            .flatMap { transport in
                transport.websocketDidConnectSubject
                    .inheritError(from: transport.websocketDidDisconnectSubject)
                    .map { transport }
            }
            .do(
                onNext: {
                    LogBindingsInfo("Established connection to \(self.url)")
                    self.transport = $0
                },
                onDispose: {
                    if self.transport != nil {
                        LogBindingsWarning("Lost connection to \(self.url)")
                        self.transport = nil
                    }
                }
            )
            // Not using `flatMapLatest` so that `onDispose` is not called.
            .flatMap { transport in
                // Ensure transport is not deallocated
                Observable.using({ transport }) { $0.didReceiveData }
            }
            // Retry every 5 seconds if we disconnected
            .retryWhen { (errors: Observable<Error>) in
                errors.flatMapLatest { error -> Observable<Void> in
                    LogBindingsError("Connection failed: \(error)")
                    return Observable.just(()).delay(
                        .seconds(5),
                        scheduler: MainScheduler.instance
                    )
                }
            }
    }

    func send(data: Data) throws {
        guard let transport = transport else {
            throw TransportLayerError.disconnected
        }

        try transport.send(data: data)
    }
}

private enum WebsocketTransportError: Error {
    case disconnected
}

private class WebsocketTransport: WebSocketDelegate, Disposable {
    private let webSocket: WebSocket
    private let internalQueue = DispatchQueue(label: "gg.websocket.internal")

    fileprivate let didReceiveData = PublishSubject<Data>()

    init(url: URL) {
        webSocket = WebSocket(url: url)
        webSocket.callbackQueue = internalQueue
        webSocket.delegate = self
    }

    func connect() {
        LogBindingsVerbose("Connecting...")
        webSocket.connect()
    }

    func send(data: Data) throws {
        LogBindingsVerbose("Send: \(data as NSData)")

        guard webSocket.isConnected else {
            throw TransportLayerError.disconnected
        }

        webSocket.write(data: data)
    }

    // MARK: Disposable

    func dispose() {
        webSocket.disconnect()
    }

    // MARK: WebSocketDelegate

    let websocketDidConnectSubject = ReplaySubject<Void>.create(bufferSize: 1)
    func websocketDidConnect(socket: WebSocketClient) {
        websocketDidConnectSubject.on(.next(()))
    }

    let websocketDidDisconnectSubject = PublishSubject<Void>()
    func websocketDidDisconnect(socket: WebSocketClient, error: Error?) {
        websocketDidDisconnectSubject.on(.error(error ?? WebsocketTransportError.disconnected))
    }

    func websocketDidReceiveMessage(socket: WebSocketClient, text: String) {
        let data = text.data(using: .utf8)!
        websocketDidReceiveData(socket: socket, data: data)
    }

    func websocketDidReceiveData(socket: WebSocketClient, data: Data) {
        LogBindingsVerbose("Receive: \(data as NSData)")
        didReceiveData.on(.next(data))
    }
}
