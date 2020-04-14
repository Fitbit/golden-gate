//
//  ManagedHub.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
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
