//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  HelloWorldCoAPClient.swift
//  GoldenGateHost
//
//  Created by Bogdan Vlad on 12/14/17.
//

import GoldenGate
import RxCocoa
import RxSwift

enum HelloWorldCoapError: Error {
    case disconnected
}

class HelloWorldCoapClient {
    private let endpoint: CoapEndpoint
    private let disposeBag = DisposeBag()

    init(endpoint: CoapEndpoint) {
        self.endpoint = endpoint
    }

    public func makeRequest() -> Single<Data> {
        return Single.just(endpoint)
            .flatMap { endpoint -> Single<CoapMessage> in
                let request = CoapRequestBuilder().path(["helloworld"]).build()
                return endpoint.response(request: request)
            }
            .flatMap { $0.body.asData() }
            .timeout(.seconds(5), scheduler: MainScheduler.instance)
    }
}

