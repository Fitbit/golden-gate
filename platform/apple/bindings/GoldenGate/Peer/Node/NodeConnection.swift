//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  NodeConnection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/5/17.
//

import BluetoothConnection
import Foundation
import RxCocoa
import RxSwift

/// A Connection to a GoldenGate Hub (a peer that does NOT expose TX/RX).
///
/// This should actually be internal, but made visible for introspection purposes.
public class NodeConnection: LinkConnection {
    // MARK: Connection properties
    public let networkLink = BehaviorRelay<NetworkLink?>(value: nil)
    public let modelNumber = Observable<String>.never()
    public let serialNumber = Observable<String>.never()
    public let firmwareRevision = Observable<String>.never()
    public let hardwareRevision = Observable<String>.never()
    public let remotePreferredConnectionConfiguration = BehaviorRelay<LinkConnectionConfiguration?>(value: nil)
    public let remotePreferredConnectionMode = BehaviorRelay<LinkConnectionMode?>(value: nil)

    public let descriptor: PeerDescriptor

    // MARK: Own properties
    private let disposeBag = DisposeBag()

    init(
        descriptor: PeerDescriptor,
        networkLink: Observable<NetworkLink>,
        remotePreferredConnectionConfiguration: Observable<LinkConnectionConfiguration>,
        remotePreferredConnectionMode: Observable<LinkConnectionMode>
    ) {
        self.descriptor = descriptor

        networkLink
            .optionalize()
            .catchAndReturn(nil)
            .logDebug("\(descriptor).networkLink", .bluetooth, .next)
            .subscribe(onNext: self.networkLink.accept)
            .disposed(by: disposeBag)

        remotePreferredConnectionConfiguration
            .optionalize()
            .catchAndReturn(nil)
            .logInfo("\(self.descriptor).remotePreferredConnectionConfiguration: ", .bluetooth, .next)
            .subscribe(onNext: self.remotePreferredConnectionConfiguration.accept)
            .disposed(by: disposeBag)

        remotePreferredConnectionMode
            .optionalize()
            .catchAndReturn(nil)
            .logInfo("\(self.descriptor).remotePreferredConnectionMode: ", .bluetooth, .next)
            .subscribe(onNext: self.remotePreferredConnectionMode.accept)
            .disposed(by: disposeBag)
    }

    deinit {
        LogBluetoothDebug("NodeConnection deinited")
    }
}
