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
    private let coapEndpoint: CoapEndpointRefType

    init(coapEndpoint: CoapEndpointRefType) throws {
        self.coapEndpoint = coapEndpoint

        var ref: Ref?
        try GG_CoapTestService_Create(coapEndpoint.ref, &ref).rethrow()
        self.ref = ref!
    }

    deinit {
        GG_CoapTestService_Destroy(ref)
    }
}
