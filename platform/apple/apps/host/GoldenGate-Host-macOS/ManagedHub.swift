//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ManagedHub.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import BluetoothConnection
import GoldenGate
import RxCocoa
import RxSwift

class ManagedHub: ManagedPeer<NodeConnection> {
    private let disposeBag = DisposeBag()

    override init(
        connectionController: AnyConnectionController<NodeConnection>,
        record: PeerRecord,
        peerParameters: PeerParameters,
        runLoop: GoldenGate.RunLoop,
        globalBlasterConfiguration: Observable<BlasterService.Configuration>
    ) {
        super.init(
            connectionController: connectionController,
            record: record,
            peerParameters: peerParameters,
            runLoop: runLoop,
            globalBlasterConfiguration: globalBlasterConfiguration
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
