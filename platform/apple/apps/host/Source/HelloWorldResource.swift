//
//  HelloWorldResource.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGate
import RxCocoa
import RxSwift

/// Simple resource hosted on "/helloworld".
class HelloWorldResource: BaseCoapResourceHandler {
    override init() {
        super.init()
    }

    /// Registers resource on the given endpoint.
    func register(on endpoint: CoapServerEndpoint) -> Completable {
        return endpoint.register(for: "helloworld", resourceHandler: self)
    }

    /// Responds with a simple "Hello there!" message
    override func onGet(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response> {
        let response = responseBuilder
            .body(data: "Hello there!".data(using: .utf8)!)
            .responseCode(.success(.created))
            .build()

        return .just(response)
    }

    /// Responds with a message, containing the payload from the request interpreted as a UTF-8 string.
    override func onPut(request: CoapServer.Request, responseBuilder: CoapResponseBuilder) -> Single<CoapServer.Response> {
        return request.body.asData().map { bytes in
            let name = String(decoding: bytes, as: UTF8.self)

            return responseBuilder
                .body(data: "Hello there, \(name)!".data(using: .utf8)!)
                .responseCode(.success(.created))
                .build()
        }
    }
}
