//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  HubConnection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/5/17.
//

import BluetoothConnection
import Foundation
import RxRelay
import RxSwift

/// A Connection to a GoldenGate Node (a peer that exposes TX/RX).
///
/// This should actually be internal, but made visible for introspection purposes.
public class HubConnection: SecureLinkConnection {
    public let descriptor: PeerDescriptor
    public let ancsAuthorized: Observable<Bool>
    public let networkLink = BehaviorRelay<NetworkLink?>(value: nil)
    public let remoteConnectionConfiguration = BehaviorRelay<LinkStatusService.ConnectionConfiguration?>(value: nil)
    public let remoteConnectionStatus = BehaviorRelay<LinkStatusService.ConnectionStatus?>(value: nil)
    public let accessBondSecureCharacteristic: Completable
    private let disposeBag = DisposeBag()

    public init(
        descriptor: PeerDescriptor,
        ancsAuthorized: Observable<Bool>,
        networkLink: Observable<NetworkLink>,
        remoteConnectionConfiguration: Observable<LinkStatusService.ConnectionConfiguration>,
        remoteConnectionStatus: Observable<LinkStatusService.ConnectionStatus>,
        accessBondSecureCharacteristic: Completable
    ) {

        self.descriptor = descriptor

        networkLink
            .optionalize()
            .catchErrorJustReturn(nil)
            .logDebug("\(descriptor).networkLink: ", .bluetooth, .next)
            .subscribe(onNext: self.networkLink.accept)
            .disposed(by: disposeBag)

        remoteConnectionConfiguration
            .optionalize()
            .catchErrorJustReturn(nil)
            .logDebug("\(descriptor).remoteConnectionConfiguration: ", .bluetooth, .next)
            .subscribe(onNext: self.remoteConnectionConfiguration.accept)
            .disposed(by: disposeBag)

        remoteConnectionStatus
            .optionalize()
            .catchErrorJustReturn(nil)
            .logDebug("\(descriptor).remoteConnectionStatus: ", .bluetooth, .next)
            .subscribe(onNext: self.remoteConnectionStatus.accept)
            .disposed(by: disposeBag)

        self.accessBondSecureCharacteristic = accessBondSecureCharacteristic
        self.ancsAuthorized = ancsAuthorized
    }

    deinit {
        LogBluetoothDebug("HubConnection deinited")
    }
}

