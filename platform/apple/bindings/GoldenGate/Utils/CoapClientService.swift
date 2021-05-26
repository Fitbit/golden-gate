//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapClientService.swift
//  GoldenGate-iOS
//
//  Created by Vlad-Mihai Corneci on 24/10/2018.
//

import GoldenGateXP
import RxSwift

/// Service that sends CoAP requests using a CoAP endpoint.
public class CoapClientService {
    typealias Ref = OpaquePointer

    var ref: Ref
    private let runLoop: GoldenGate.RunLoop
    private let coapEndpoint: CoapEndpointRefType

    init(runLoop: GoldenGate.RunLoop, coapEndpoint: CoapEndpointRefType) throws {
        runLoopPrecondition(condition: .onRunLoop)

        self.runLoop = runLoop
        self.coapEndpoint = coapEndpoint

        var ref: Ref?
        try GG_CoapClientService_Create(runLoop.ref, coapEndpoint.ref, &ref).rethrow()
        self.ref = ref!
    }

    deinit {
        runLoop.async { [ref] in
            GG_CoapClientService_Destroy(ref)
        }
    }
}
