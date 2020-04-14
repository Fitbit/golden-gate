//
//  CoapResponseListener.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 11/27/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import GoldenGateXP
import Rxbit
import RxSwift

enum CoapResponseListenerError: Error {
    case failureWithMessage(GGRawError, message: String)
}

/// A listener used by a CoAP client endpoint to know when a response was received.
final class CoapResponseListener: GGAdaptable {
    typealias GGObject = GG_CoapResponseListener
    typealias GGInterface = GG_CoapResponseListenerInterface

    private let responseCache = SingleUseCacheSubject<CoapMessage>()
    let adapter: Adapter

    private static var interface = GG_CoapResponseListenerInterface(
        OnAck: { ref in
            // ignore
        },
        OnError: { ref, result, messageRef in
            let `self` = Adapter.takeUnretained(ref)

            if let messageRef = messageRef {
                let error = CoapResponseListenerError.failureWithMessage(GGRawError(result), message: String(cString: messageRef))
                self.responseCache.on(.error(error))
            } else {
                self.responseCache.on(.error(GGRawError(result)))
            }
        },
        OnResponse: { ref, messageRef in
            Adapter.takeUnretained(ref).onResponse(
                messageRef: messageRef!
            )
        }
    )

    init() {
        self.adapter = GGAdapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    deinit {
        LogBindingsDebug("\(type(of: self)) deinited.")
    }

    internal var response: Single<CoapMessage> {
        return responseCache.asObservable().take(1).asSingle()
    }

    private func onResponse(messageRef: OpaquePointer) {
        do {
            let rawMessage = try RawCoapMessage(message: messageRef)

            // TODO: [FC-1189] handle unexpected CoAP block-wise response

            let message = CoapMessage(
                code: rawMessage.code,
                options: rawMessage.options,
                body: CoapStaticMessageBody(data: rawMessage.payload)
            )

            responseCache.on(.next(message))
            responseCache.on(.completed)
        } catch {
            responseCache.on(.error(error))
        }
    }
}
