//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapTestService.swift
//  GoldenGate
//
//  Created by Vlad-Mihai Corneci on 13/11/2018.
//

import GoldenGateXP
import RxSwift

public class CoapTestService {
    typealias Ref = OpaquePointer

    private var ref: Ref
    private let runLoop: RunLoop
    private let coapEndpoint: CoapEndpointRefType

    init(runLoop: RunLoop, coapEndpoint: CoapEndpointRefType) throws {
        runLoopPrecondition(condition: .onRunLoop)

        self.runLoop = runLoop
        self.coapEndpoint = coapEndpoint
        
        var ref: Ref?
        try GG_CoapTestService_Create(coapEndpoint.ref, &ref).rethrow()
        self.ref = ref!
    }

    deinit {
        runLoop.async { [ref] in
            GG_CoapTestService_Destroy(ref)
        }
    }
}
