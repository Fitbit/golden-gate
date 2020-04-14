//
//  SystemRemoteApi.swift
//  GoldenGateHost-iOS
//
//  Created by Vlad-Mihai Corneci on 01/11/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
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
