//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapBlockwiseResponseListener.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 5/30/18.
//

import GoldenGateXP
import Rxbit
import RxSwift

enum CoapBlockwiseResponseListenerError: Error {
    case failureWithMessage(GGRawError, message: String)
}

protocol UnsafeCoapBlockSource {
    var ref: UnsafeMutablePointer<GG_CoapBlockSource> { get }
}

/// A listener used by a CoAP client endpoint to receive
/// callbacks whenever a new response block is received
/// for an earlier request.
final class CoapBlockwiseResponseListener: GGAdaptable {
    typealias GGObject = GG_CoapBlockwiseResponseListener
    typealias GGInterface = GG_CoapBlockwiseResponseListenerInterface

    private enum State {
        case waitingForResponse
        case waitingForEndOfStream(CoapStreamingMessageBody)
        case done
    }

    let adapter: Adapter

    private var state = State.waitingForResponse
    private let responseCache = SingleUseCacheSubject<CoapMessage>()

    private let transportReadiness: Observable<TransportReadiness>
    private let requestIdentifier: String

    private static var interface = GG_CoapBlockwiseResponseListenerInterface(
        OnResponseBlock: { ref, blockInfoRef, messageRef in
            Adapter.takeUnretained(ref).onResponse(
                blockInfo: blockInfoRef!.pointee,
                messageRef: messageRef!
            )
        },
        OnError: { ref, result, errorMessageRef in
            let `self` = Adapter.takeUnretained(ref)

            if let errorMessageRef = errorMessageRef {
                let error = CoapBlockwiseResponseListenerError.failureWithMessage(GGRawError(result), message: String(cString: errorMessageRef))
                self.onError(error)
            } else {
                self.onError(GGRawError(result))
            }
        }
    )

    internal init(transportReadiness: Observable<TransportReadiness>, requestIdentifier: String) {
        self.transportReadiness = transportReadiness
        self.requestIdentifier = requestIdentifier

        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    deinit {
        LogBindingsVerbose("\(type(of: self)) deinited.")
    }

    /// - Parameter disposabe: Will cancel the request on disposal.
    internal func response(using resource: Disposable) -> Single<CoapMessage> {
        // Use a DisposeBag to allow dispose on deallocation.
        let disposeBag = DisposeBag()
        resource.disposed(by: disposeBag)

        return responseCache
            .asObservable()
            .take(1)
            .asSingle()
            .map { message in
                // Wrap the message with one that keeps the request alive
                // as long as someone holds onto the body.
                //
                // We can't store it in the body earlier, as that would lead
                // to a ref-cycle (body -> disposable -> listener -> body)
                return CoapMessage(
                    code: message.code,
                    options: message.options,
                    body: CoapMessageBodyWithResource(message.body, disposeBag: disposeBag)
                )
            }
    }

    // visible for testing
    internal func onResponse(blockInfo: GG_CoapMessageBlockInfo, messageRef: OpaquePointer) {
        let rawMessage: RawCoapMessage
        do {
            rawMessage = try RawCoapMessage(message: messageRef)
        } catch {
            onError(error)
            return
        }

        let body: CoapStreamingMessageBody

        switch state {
        case .waitingForResponse:
            body = CoapStreamingMessageBody(transportReadiness: transportReadiness, requestIdentifier: requestIdentifier)
            state = .waitingForEndOfStream(body)

            let message = CoapMessage(
                code: rawMessage.code,
                options: rawMessage.options,
                body: body
            )

            responseCache.on(.next(message))
            responseCache.on(.completed)
        case let .waitingForEndOfStream(wrappedBody):
            body = wrappedBody

            // Error body if subsequent messages are failures
            guard rawMessage.code.codeClass == .successfulResponse else {
                let extendedError = CoapExtendedError(rawMessage)
                body.onError(CoapRequestError.responseNotSuccessful(rawMessage.code, extendedError))
                state = .done
                return
            }

        case .done:
            LogBindingsWarning("Received a response, but not interested in it anymore.")
            return
        }

        if let payload = rawMessage.payload {
            body.onNext(payload)
        }

        // Complete if this was the last block or an unsuccessful first block
        if !blockInfo.more || rawMessage.code.codeClass != .successfulResponse {
            body.onComplete()
            state = .done
        }
    }

    // visible for testing
    internal func onError(_ error: Error) {
        switch self.state {
        case let .waitingForEndOfStream(body):
            body.onError(error)
            responseCache.on(.error(error))
        case .waitingForResponse, .done:
            responseCache.on(.error(error))
        }

        self.state = .done
    }
}

/// Utility to keep a reference to a DisposeBag with resources
/// inside that should be kept alive until the body was consumed.
private final class CoapMessageBodyWithResource: CoapMessageBody {
    private let body: CoapMessageBody
    private let disposeBag: DisposeBag

    init(_ body: CoapMessageBody, disposeBag: DisposeBag) {
        self.body = body
        self.disposeBag = disposeBag
    }

    func asData() -> Single<Data> {
        return body.asData().do(onDispose: {
            withExtendedLifetime(self) { /* noop */ }
        })
    }

    func asStream() -> Observable<Data> {
        return body.asStream().do(onDispose: {
            withExtendedLifetime(self) { /* noop */ }
        })
    }
}

final class CoapStreamingMessageBody: CoapMessageBody {
    private var cache = SingleUseCacheSubject<Data>()
    private let transportReadiness: Observable<TransportReadiness>
    private let requestIdentifier: String

    init(transportReadiness: Observable<TransportReadiness>, requestIdentifier: String) {
        self.transportReadiness = transportReadiness
        self.requestIdentifier = requestIdentifier
    }

    func onNext(_ data: Data) {
        self.cache.on(.next(data))
    }

    func onComplete() {
        self.cache.on(.completed)
    }

    func onError(_ error: Error) {
        self.cache.on(.error(error))
    }

    public func asData() -> Single<Data> {
        let transportUnavailable = transportReadiness.filter { [requestIdentifier] readiness in
            switch readiness {
            case .ready:
                return false
            case .notReady(let reason):
                LogBindingsWarning("[CoAP] [asData()] Request \(requestIdentifier) couldn't be fulfilled (transport unavailable: \(reason))")
                throw CoapRequestError.transportUnavailable(reason: reason)
            }
        }

        return cache.asSingle()
            .map { availableData in
                availableData.reduce(Data()) { (result, newElement) -> Data in
                    var newResult = result
                    newResult.append(newElement)

                    return newResult
                }
            }
            .asObservable()
            .takeUntil(transportUnavailable)
            .asSingle()
    }

    public func asStream() -> Observable<Data> {
        let transportUnavailable = transportReadiness.filter { [requestIdentifier] readiness in
            switch readiness {
            case .ready:
                return false
            case .notReady(let reason):
                LogBindingsWarning("[CoAP] [asStream()] Request \(requestIdentifier) couldn't be fulfilled (transport unavailable: \(reason))")
                throw CoapRequestError.transportUnavailable(reason: reason)
            }
        }

        return cache.asObservable()
            .takeUntil(transportUnavailable)
    }
}
