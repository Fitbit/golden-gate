//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapEndpoint+Rx.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 12/15/17.
//

import RxSwift

public extension ObservableConvertibleType where E == CoapEndpoint.Response {
    
    /// An operator that skips the ACK from the CoAP Response.
    ///
    /// - Returns: An Observable that ignores the CoAP ack.
    public func skipAck() -> Single<CoapMessage> {
        return asObservable().flatMap { response -> Observable<CoapMessage> in
            switch response {
            case .ack: return .empty()
            case .content(let message): return .just(message)
            }
        }
        .take(1)
        .asSingle()
    }
}

