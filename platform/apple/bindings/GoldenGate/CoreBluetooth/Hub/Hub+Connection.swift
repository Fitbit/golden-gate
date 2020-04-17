//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Hub+Connection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/5/17.
//

import Foundation
import Rxbit
import RxCocoa
import RxSwift

extension Hub {
    /// A Connection to a GoldenGate Node (a peer that does NOT expose TX/RX).
    ///
    /// This should actually be internal, but made visible for introspection purposes.
    public class Connection: BluetoothConnection {
        public let descriptor: BluetoothPeerDescriptor
        public let peripheralConnection = BehaviorRelay<PeripheralConnectionStatus>(value: .disconnected)
        public let networkLinkStatus = BehaviorRelay<NetworkLinkStatus>(value: .disconnected)
        public let remoteConnectionConfiguration = BehaviorRelay<LinkStatusService.ConnectionConfiguration?>(value: nil)
        public let remoteConnectionStatus = BehaviorRelay<LinkStatusService.ConnectionStatus?>(value: nil)
        public let remoteBatteryLevel = BehaviorRelay<BatteryServiceLevel?>(value: nil)
        public let accessBondSecureCharacteristic: Completable
        public let ancsAuthorized: Observable<Bool>
        private let disposeBag = DisposeBag()

        init(
            descriptor: BluetoothPeerDescriptor,
            peripheralConnection: Observable<PeripheralConnectionStatus>,
            networkLinkStatus: Observable<NetworkLinkStatus>,
            remoteConnectionConfiguration: Observable<LinkStatusService.ConnectionConfiguration>,
            remoteConnectionStatus: Observable<LinkStatusService.ConnectionStatus>,
            accessBondSecureCharacteristic: Completable,
            remoteBatteryLevel: Observable<BatteryServiceLevel>,
            ancsAuthorized: Observable<Bool>
        ) {

            self.descriptor = descriptor

            peripheralConnection
                .catchErrorJustReturn(.disconnected)
                .logDebug("\(descriptor).peripheralConnection: ", .bindings, .next)
                .subscribe(onNext: self.peripheralConnection.accept)
                .disposed(by: disposeBag)

            networkLinkStatus
                .catchErrorJustReturn(.disconnected)
                .logDebug("\(descriptor).connectionStatus: ", .bindings, .next)
                .subscribe(onNext: self.networkLinkStatus.accept)
                .disposed(by: disposeBag)

            remoteConnectionConfiguration
                .optionalize()
                .catchErrorJustReturn(nil)
                .logDebug("\(descriptor).remoteConnectionConfiguration: ", .bindings, .next)
                .subscribe(onNext: self.remoteConnectionConfiguration.accept)
                .disposed(by: disposeBag)

            remoteConnectionStatus
                .optionalize()
                .catchErrorJustReturn(nil)
                .logDebug("\(descriptor).remoteConnectionStatus: ", .bindings, .next)
                .subscribe(onNext: self.remoteConnectionStatus.accept)
                .disposed(by: disposeBag)

            remoteBatteryLevel
                .optionalize()
                .catchErrorJustReturn(nil)
                .logDebug("\(descriptor).remoteBatteryLevel: ", .bindings, .next)
                .subscribe(onNext: self.remoteBatteryLevel.accept)
                .disposed(by: disposeBag)

            self.accessBondSecureCharacteristic = accessBondSecureCharacteristic
            self.ancsAuthorized = ancsAuthorized
        }

        deinit {
            LogBindingsDebug("\(self ??? "Hub.Connection") deinitted")
        }
    }
}
