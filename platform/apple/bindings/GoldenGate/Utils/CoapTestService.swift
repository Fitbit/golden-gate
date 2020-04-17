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

extension CoapTestService: RemoteApiModule {
    public var methods: Set<String> { [] } // Methods are defined in xp

    public func publishHandlers(on remoteShell: RemoteShell) {
        GG_CoapTestService_Register(ref)
        GG_CoapTestService_RegisterSmoHandlers(remoteShell.ref, GG_CoapTestService_AsRemoteSmoHandler(ref))
    }

    public func unpublishHandlers(from remoteShell: RemoteShell) {
        GG_CoapTestService_UnregisterSmoHandlers(remoteShell.ref, GG_CoapTestService_AsRemoteSmoHandler(ref))
        GG_CoapTestService_Unregister(ref)
    }
}
