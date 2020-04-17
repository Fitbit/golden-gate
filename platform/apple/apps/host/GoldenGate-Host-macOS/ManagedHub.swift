//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ManagedHub.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import GoldenGate
import RxCocoa
import RxSwift

class ManagedHub: ManagedPeer {
    private let disposeBag = DisposeBag()

    override init(record: PeerRecord, commonPeerParameters: CommonPeer.Parameters, remoteTestServer: RemoteTestServerType?) {
        super.init(
            record: record,
            commonPeerParameters: commonPeerParameters,
            remoteTestServer: remoteTestServer
        )
        let registerHelloWorld = HelloWorldResource()
            .register(on: coapEndpoint)

        coapEndpoint
            .setRequestFilterGroup(.group0)
            .andThen(registerHelloWorld)
            .subscribe()
            .disposed(by: disposeBag)
    }
}
