//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapResourceHandler.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 6/15/18.
//

import RxSwift

public protocol CoapResourceHandler {
    func onGet(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response>
    func onPost(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response>
    func onPut(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response>
    func onDelete(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response>
}

extension CoapResourceHandler {
    public func methodNotAllowed(responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response> {
        return Single.just(responseBuilder.responseCode(.clientError(.methodNotAllowed)).build())
    }
}

open class BaseCoapResourceHandler: CoapResourceHandler {
    public init() {}

    open func onGet(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response> {
        return methodNotAllowed(responseBuilder: responseBuilder)
    }

    open func onPost(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response> {
        return methodNotAllowed(responseBuilder: responseBuilder)
    }

    open func onPut(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response> {
        return methodNotAllowed(responseBuilder: responseBuilder)
    }

    open func onDelete(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response> {
        return methodNotAllowed(responseBuilder: responseBuilder)
    }
}
