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
    public let modelNumber: Observable<String>
    public let serialNumber: Observable<String>
    public let firmwareRevision: Observable<String>
    public let hardwareRevision: Observable<String>
    public let remoteConnectionConfiguration = BehaviorRelay<LinkStatusService.ConnectionConfiguration?>(value: nil)
    public let remoteConnectionStatus = BehaviorRelay<LinkStatusService.ConnectionStatus?>(value: nil)
    public let accessBondSecureCharacteristic: Completable
    private let disposeBag = DisposeBag()

    public init(
        descriptor: PeerDescriptor,
        ancsAuthorized: Observable<Bool>,
        networkLink: Observable<NetworkLink>,
        modelNumber: Observable<String>,
        serialNumber: Observable<String>,
        firmwareRevision: Observable<String>,
        hardwareRevision: Observable<String>,
        remoteConnectionConfiguration: Observable<LinkStatusService.ConnectionConfiguration>,
        remoteConnectionStatus: Observable<LinkStatusService.ConnectionStatus>,
        accessBondSecureCharacteristic: Completable
    ) {

        self.descriptor = descriptor

        networkLink
            .optionalize()
            .catchAndReturn(nil)
            .logDebug("\(descriptor).networkLink: ", .bluetooth, .next)
            .subscribe(onNext: self.networkLink.accept)
            .disposed(by: disposeBag)

        self.modelNumber = modelNumber
        self.serialNumber = serialNumber
        self.firmwareRevision = firmwareRevision
        self.hardwareRevision = hardwareRevision

        remoteConnectionConfiguration
            .optionalize()
            .catchAndReturn(nil)
            .logDebug("\(descriptor).remoteConnectionConfiguration: ", .bluetooth, .next)
            .subscribe(onNext: self.remoteConnectionConfiguration.accept)
            .disposed(by: disposeBag)

        remoteConnectionStatus
            .optionalize()
            .catchAndReturn(nil)
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

