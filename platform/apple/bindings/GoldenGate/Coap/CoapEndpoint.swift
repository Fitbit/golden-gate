//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapEndpoint.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/21/17.
//

import BluetoothConnection
import Foundation
import GoldenGateXP
import RxSwift

// swiftlint:disable file_length

public typealias CoapResponseHandler = (CoapMessage, CoapResponseBuilder) throws -> CoapServer.Response

/// Coap endpoint that acts as a client.
public protocol CoapClientEndpoint: AnyObject {
    /// Makes a CoAP request and returns the response as a Single.
    func response(request: CoapRequest) -> Single<CoapMessage>
}

/// Coap endpoint that acts as a server.
public protocol CoapServerEndpoint: AnyObject {
    /// Registers a coap handler for a certain path. When the subscription to this completable is disposed, the coap handler is unregistered.
    ///
    /// - Parameters:
    ///   - path: The path where the coap handler will be registered.
    ///   - handler: The handler that will get called when a coap request is received.
    /// - Returns: Completable
    ///   - Error: If the coap endpoint registration fails.
    ///   - Complete: The coap handler is unregistered.
    func register(for path: String, resourceHandler: CoapResourceHandler) -> Completable

    /// Change request filter group. A `CoapRequestFilter` filters out requests based on the group memberships
    /// of the handler that is selected to handle a request.
    func setRequestFilterGroup(_ requestFilterGroup: CoapRequestFilterGroup) -> Completable
}

/// Coap endpoint that can act as both client and server.
public typealias CoapEndpointType = CoapClientEndpoint & CoapServerEndpoint

/// Coap endpoint that exposes the underlying XP implementation.
protocol CoapEndpointRefType: CoapEndpointType {
    typealias Ref = OpaquePointer
    /// The underlying XP coap endpoint.
    var ref: Ref { get }
}

/// The actual implementation of a Coap endpoint which can act as both client and server.
public class CoapEndpoint: CoapEndpointType {
    typealias Ref = OpaquePointer

    internal let ref: Ref
    fileprivate let runLoop: RunLoop
    private let port: Observable<Port?>
    private let transferStrategy: CoapTransferStrategy
    private var portDataSink: UnsafeDataSink?
    private var portDataSource: UnsafeDataSource?
    private let requestFilter: CoapGroupRequestFilter
    private let transportReadiness: Observable<TransportReadiness>
    private var handlers = [CoapRequestHandler]()
    private let disposeBag = DisposeBag()

    /// Creates a new CoapEndpoint.
    ///
    /// - Parameters:
    ///     - runLoop: The transport layer run loop.
    ///     - port: An observable that emits transport stack top port.
    ///     - transferStrategy: The Coap transfer strategy (e.g. blockwise, non-blockwise).
    ///     - transportReadiness: An observable that emits the transport layer readiness.
    public init(
        runLoop: RunLoop,
        port: Observable<Port?>,
        transferStrategy: CoapTransferStrategy? = nil,
        transportReadiness: Observable<TransportReadiness> = .just(.ready)
    ) throws {
        runLoopPrecondition(condition: .onRunLoop)

        var ref: Ref?
        try GG_CoapEndpoint_Create(
            runLoop.timerSchedulerRef,
            nil,
            nil,
            &ref
        ).rethrow()
        self.ref = ref!

        self.runLoop = runLoop
        self.port = port
        self.transferStrategy = transferStrategy ?? CoapRequestDependentTransferStrategy(transportReadiness: transportReadiness)
        self.requestFilter = try CoapGroupRequestFilter(runLoop)
        self.transportReadiness = transportReadiness.observe(on: runLoop)

        // register the CoAP group request filter
        register(requestFilter: requestFilter)
            .andThen(setRequestFilterGroup(.group1))
            .subscribe()
            .disposed(by: disposeBag)

        port
            .subscribe(onNext: { [weak self] port in
                LogBindingsInfo("[CoAP] CoapEndpoint new top port: \(port ??? "nil")")
                guard let self = self else { return }

                let portDataSink = port?.dataSink.gg
                let portDataSource = port?.dataSource.gg

                // The port bindings updates must be in sync (on the run loop) with the stack creation.
                // `.observe(on: runLoop)` should be avoided in these situations since it breaks the
                // synchronization by executing the subsequent events async.
                runLoop.sync {
                    let dataSource = GG_CoapEndpoint_AsDataSource(self.ref)

                    GG_DataSource_SetDataSink(dataSource, portDataSink?.ref)
                    if let source = portDataSource {
                        GG_DataSource_SetDataSink(source.ref, GG_CoapEndpoint_AsDataSink(self.ref))
                    }
                }

                self.portDataSink = portDataSink
                self.portDataSource = portDataSource
            })
            .disposed(by: disposeBag)
    }

    deinit {
        LogBindingsInfo("[CoAP] CoapEndpoint \(self) deinited")

        runLoop.async { [portDataSource, portDataSink, ref] in
            _ = try? portDataSource?.setDataSink(nil)
            _ = try? portDataSink?.setDataSinkListener(nil)

            GG_CoapEndpoint_Destroy(ref)
        }
    }
}

extension CoapEndpoint {
    private func register(requestFilter: CoapGroupRequestFilter) -> Completable {
        return Completable.create { [ref, runLoop] completable in
            runLoop.async {
                do {
                    try GG_CoapEndpoint_RegisterRequestFilter(
                        ref,
                        GG_CoapGroupRequestFilter_AsCoapRequestFilter(requestFilter.ref)
                    ).rethrow()
                } catch {
                    completable(.error(error))
                }
            }

            return Disposables.create()
        }
    }

    public func setRequestFilterGroup(_ requestFilterGroup: CoapRequestFilterGroup) -> Completable {
        return requestFilter.setGroup(requestFilterGroup)
    }
}

// MARK: - Client Side APIs

extension CoapEndpoint {
    /// Makes a CoAP request and returns the response as a `Single<CoapMessage>`.
    /// Data is not allowed to go through if the transport is not ready.
    ///
    /// - Parameters:
    ///   - request: A CoAP request.
    ///
    /// - Returns: An observable that will emit the response.
    public func response(request: CoapRequest) -> Single<CoapMessage> {
        let transportUnavailable = transportReadiness.filter { readiness in
            switch readiness {
            case .ready:
                return false
            case .notReady(let reason):
                LogBindingsWarning("[CoAP] Request \(request) couldn't be fulfilled (transport unavailable: \(reason))")
                throw CoapRequestError.transportUnavailable(reason: reason)
            }
        }

        return transferStrategy.response(request: request, endpoint: self)
            .subscribe(on: runLoop)
            .asObservable()
            .take(until: transportUnavailable)
            .asSingle()
            // Move any response processing off the run-loop
            .observe(on: SerialDispatchQueueScheduler(qos: .default))
            .flatMap { [expectsSuccess = request.expectsSuccess] response -> Single<CoapMessage> in
                // Error observable if response wasn't successful
                // but the client is only expecting success.
                switch (response.code, expectsSuccess) {
                case (.response(.success), _), (_, false):
                    return .just(response)
                case (_, true):
                    // Forward extended error if any
                    return response.extendedError
                        .map { throw CoapRequestError.responseNotSuccessful(response.code, $0) }
                }
            }
    }
}

// MARK: - Server Side APIs

extension CoapEndpoint {
    /// Registers a coap handler for a certain path. When the subscription to this completable is disposed, the coap handler is unregistered.
    ///
    /// - Parameters:
    ///   - path: The path where the coap handler will be registered.
    ///   - handler: The handler that will get called when a coap request is received.
    /// - Returns: Completable
    ///   - Error: If the coap endpoint registration fails.
    ///   - Complete: The coap handler is unregistered.
    private func register(for path: String, _ handler: @escaping CoapResponseHandler) -> Completable {
        return Completable.create { completable in
            let handler = CoapRequestHandler(responseHandler: handler, endpoint: self)

            self.runLoop.async {
                do {
                    try GG_CoapEndpoint_RegisterRequestHandler(
                        self.ref,
                        path,
                        UInt32(GG_COAP_REQUEST_HANDLER_FLAGS_ALLOW_ALL),
                        handler.ref
                    ).rethrow()
                } catch {
                    completable(.error(error))
                }
            }

            return Disposables.create {
                // Ensure potential last reference to `handler` is released on `runLoop`.
                self.runLoop.async {
                    GG_CoapEndpoint_UnregisterRequestHandler(self.ref, nil, handler.ref)
                }
            }
        }
    }

    public func register(for path: String, resourceHandler: CoapResourceHandler) -> Completable {
        return register(for: path) { message, responseBuilder in
            let response: Single<CoapServer.Response>

            // Default to standard response codes matching the method.
            // They can still be modified in the respective handlers.
            switch message.code {
            case .request(.get):
                response = resourceHandler.onGet(request: message, responseBuilder: responseBuilder.responseCode(.success(.content)))
            case .request(.post):
                // We can't really know if this will "create" or "change" something.
                response = resourceHandler.onPost(request: message, responseBuilder: responseBuilder)
            case .request(.put):
                // We can't really know if this will "create" or "change" something.
                response = resourceHandler.onPut(request: message, responseBuilder: responseBuilder)
            case .request(.delete):
                response = resourceHandler.onDelete(request: message, responseBuilder: responseBuilder.responseCode(.success(.deleted)))
            case .response:
                fatalError("Unexpected message code: \(message.code)")
            }

            let semaphore = DispatchSemaphore(value: 0)
            var result: SingleEvent<CoapServer.Response>?

            _ = response.subscribe { event in
                result = event
                semaphore.signal()
            }

            semaphore.wait()

            switch result! {
            case .success(let value):
                return value
            case .failure(let error):
                throw error
            }
        }
    }
}

extension CoapEndpoint: CoapEndpointRefType {}

// MARK: - Transfer Strategies

public protocol CoapTransferStrategy {
    func response(request: CoapRequest, endpoint: CoapEndpoint) -> Single<CoapMessage>
}

private final class CoapRequestDependentTransferStrategy: CoapTransferStrategy {
    private let transportReadiness: Observable<TransportReadiness>
    private let simpleStrategy: CoapTransferStrategy
    private let blockwiseStrategy: CoapTransferStrategy

    init(transportReadiness: Observable<TransportReadiness>) {
        self.transportReadiness = transportReadiness
        self.simpleStrategy = CoapSimpleTransferStrategy()
        self.blockwiseStrategy = CoapBlockwiseTransferStrategy(transportReadiness: transportReadiness)
    }

    func response(request: CoapRequest, endpoint: CoapEndpoint) -> Single<CoapMessage> {
        if request.acceptsBlockwiseTransfer {
            return blockwiseStrategy.response(request: request, endpoint: endpoint)
        } else {
            return simpleStrategy.response(request: request, endpoint: endpoint)
        }
    }
}

/// Doesn't allow block-wise requests or block-wise responses.
///
/// Slightly smaller memory footprint.
private final class CoapSimpleTransferStrategy: CoapTransferStrategy {
    func response(request: CoapRequest, endpoint: CoapEndpoint) -> Single<CoapMessage> {
        return Single.deferred { [runLoop = endpoint.runLoop] in
            let listener = RunLoopGuardedValue(CoapResponseListener(), runLoop: runLoop)
            let options = CoapMessageOptionParam.from(request.options)
            let payload = request.body.data ?? Data()

            var handle: GG_CoapRequestHandle = 0
            try withExtendedLifetime(options) {
                try payload.withUnsafeBytes { (payloadBytes: UnsafeRawBufferPointer) in
                    try GG_CoapEndpoint_SendRequest(
                        endpoint.ref,
                        GG_CoapMethod(request.method),
                        options?.ref,
                        options?.count ?? 0,
                        payloadBytes.baseAddress?.assumingMemoryBound(to: UInt8.self),
                        Int(payload.count),
                        request.parameters?.ref,
                        listener.value.ref,
                        &handle
                    ).rethrow()
                }
            }

            return listener.value.response
                .do(onDispose: { [listener] in
                    runLoop.async {
                        GG_CoapEndpoint_CancelRequest(endpoint.ref, handle)
                    }

                    // Keep listener alive as long as the subscription is alive.
                    // The listener will be deallocated on the run looop after the cancelation happens.
                    _ = listener
                })
        }
    }
}

/// Allows block-wise requests or block-wise responses.
///
/// Slightly larger memory footprint.
private final class CoapBlockwiseTransferStrategy: CoapTransferStrategy {
    private let transportReadiness: Observable<TransportReadiness>

    init(transportReadiness: Observable<TransportReadiness>) {
        self.transportReadiness = transportReadiness
    }

    func response(request: CoapRequest, endpoint: CoapEndpoint) -> Single<CoapMessage> {
        return Single.deferred { [runLoop = endpoint.runLoop, transportReadiness] in
            let blockSource: CoapStaticBlockSource?
            switch request.body {
            case .data(let data, let progress):
                blockSource = CoapStaticBlockSource(data: data, progress: progress)
            case .none:
                blockSource = nil
            }
            let runLoopGuardedBlockSource = blockSource.map { RunLoopGuardedValue($0, runLoop: runLoop) }

            let listener = RunLoopGuardedValue(
                CoapBlockwiseResponseListener(
                    transportReadiness: transportReadiness,
                    requestIdentifier: "\(request)"
                ),
                runLoop: runLoop
            )
            let options = CoapMessageOptionParam.from(request.options)

            var handle: GG_CoapRequestHandle = 0
            try withExtendedLifetime(options) {
                try GG_CoapEndpoint_SendBlockwiseRequest(
                    endpoint.ref,
                    GG_CoapMethod(request.method),
                    options?.ref,
                    options?.count ?? 0,
                    runLoopGuardedBlockSource?.value.ref,
                    0,
                    request.parameters?.ref,
                    listener.value.ref,
                    &handle
                ).rethrow()
            }

            let disposable = Disposables.create { [listener] in
                endpoint.runLoop.async {
                    do {
                        try GG_CoapEndpoint_CancelBlockwiseRequest(endpoint.ref, handle).rethrow()
                    } catch let error as GGRawError where error.ggResult == GG_ERROR_NO_SUCH_ITEM {
                        // ignore, request was removed by XP already
                    } catch {
                        LogBindingsError("[CoAP] GG_CoapEndpoint_CancelBlockwiseRequest failed: \(error)")
                    }
                }

                // Keep listener alive as long as the subscription is alive.
                // The listener will be deallocated on the run loop after the cancelation happens.
                _ = listener
            }

            return listener.value.response(using: disposable)
                .do(onDispose: { [blockSource] in
                    // Once we get the first real response or an error,
                    // we can release the BlockSource.
                    _ = blockSource
                })
        }
    }
}

// MARK: - Utilities

private extension GG_CoapMethod {
    /// Create a `GG_CoapMethod` from a `CoapCode.Request`.
    ///
    /// Needed for `GG_CoapEndpoint_SendRequest` API.
    init(_ request: CoapCode.Request) {
        switch request {
        case .put:
            self = GG_COAP_METHOD_PUT
        case .post:
            self = GG_COAP_METHOD_POST
        case .get:
            self = GG_COAP_METHOD_GET
        case .delete:
            self = GG_COAP_METHOD_DELETE
        }
    }
}
