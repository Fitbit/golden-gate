//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapTests.swift
//  GoldenGateHost-macOS
//
//  Created by Sylvain Rebaud on 1/4/18.
//

import Foundation
import GoldenGate
import RxSwift

// swiftlint:disable no_print

func testCoAPClient() throws -> Observable<CoapMessage> {
    return Observable
        .deferred {
            return .just(try CoapEndpoint(
                runLoop: Component.instance.runLoop,
                port: .just(try GGBsdDatagramSocket(
                    runLoop: Component.instance.runLoop,
                    localAddress: nil,
                    remoteAddress: SocketAddress(url: URL(string: "//127.0.0.1:7777")!)!
                ))
            ))
        }
        .flatMapLatest { endpoint -> Observable<CoapMessage> in
            let request = CoapRequestBuilder().path(["wifi", "status"]).build()
            return endpoint.response(request: request).asObservable()
        }
        .do(onNext: { Log("Response: \($0)", .debug) })
}

/// Test the CoAP server
///
/// - Returns: We return the instances that needs to be kept alive in a tuple.
func testCoAPServer() throws -> Any {
    let coapEndpoint = try CoapEndpoint(
        runLoop: Component.instance.runLoop,
        port: .just(try GGBsdDatagramSocket(
            runLoop: Component.instance.runLoop,
            localAddress: SocketAddress(url: URL(string: "//127.0.0.1:7778")!)!,
            remoteAddress: nil
        ))
    )

    // Keep the handler around forever.
    _ = HelloWorldResource().register(on: coapEndpoint)

    return (coapEndpoint) as Any
}
