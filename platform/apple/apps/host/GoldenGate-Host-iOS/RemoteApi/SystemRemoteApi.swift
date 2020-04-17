//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  SystemRemoteApi.swift
//  GoldenGateHost-iOS
//
//  Created by Vlad-Mihai Corneci on 01/11/2019.
//

import GoldenGate
import RxSwift

class SystemRemoteApi: RemoteApiModule {
    public let methods: Set<String> = Method.allUniqueRawValues

    func publishHandlers(on remoteShell: RemoteShell) {
        remoteShell.register(Method.kill.rawValue, handler: kill)
    }

    func kill() {
        DispatchQueue.main.async {
            UIApplication.shared.perform(NSSelectorFromString("terminateWithSuccess"))
        }
    }
}

private extension SystemRemoteApi {
    enum Method: String, CaseIterable {
        case kill = "sys/kill"
    }
}
