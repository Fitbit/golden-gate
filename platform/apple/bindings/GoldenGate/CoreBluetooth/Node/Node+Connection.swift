//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Node+Connection.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/5/17.
//

import Foundation
import Rxbit
import RxCocoa
import RxSwift

extension Node {
    /// A Connection to a GoldenGate Hub (a peer that exposes TX/RX).
    ///
    /// This should actually be internal, but made visible for introspection purposes.
    public class Connection: BluetoothConnection {
        // MARK: Connection properties
        public let networkLinkStatus = BehaviorRelay(value: NetworkLinkStatus.disconnected)
        public let remotePreferredConnectionConfiguration = BehaviorRelay<LinkConfigurationService.PreferredConnectionConfiguration?>(value: nil)
        public let remotePreferredConnectionMode = BehaviorRelay<LinkConfigurationService.PreferredConnectionMode?>(value: nil)

        // MARK: BluetoothConnection properties
        public let descriptor: BluetoothPeerDescriptor
        public let peripheralConnectionStatus = BehaviorRelay(value: PeripheralConnectionStatus.disconnected)

        // MARK: Own properties
        private let disposeBag = DisposeBag()

        init(
            networkLinkStatus: Observable<NetworkLinkStatus>,
            descriptor: BluetoothPeerDescriptor,
            peripheralConnectionStatus: Observable<PeripheralConnectionStatus>,
            remotePreferredConnectionConfiguration: Observable<LinkConfigurationService.PreferredConnectionConfiguration>,
            remotePreferredConnectionMode: Observable<LinkConfigurationService.PreferredConnectionMode>
        ) {
            networkLinkStatus
                .catchErrorJustReturn(.disconnected)
                .logDebug("\(descriptor).status", .bindings, .next)
                .subscribe(onNext: self.networkLinkStatus.accept)
                .disposed(by: disposeBag)

            self.descriptor = descriptor

            peripheralConnectionStatus
                .catchErrorJustReturn(.disconnected)
                .logDebug("\(descriptor).peripheralConnection", .bindings, .next)
                .subscribe(onNext: self.peripheralConnectionStatus.accept)
                .disposed(by: disposeBag)

            remotePreferredConnectionConfiguration
                .optionalize()
                .catchErrorJustReturn(nil)
                .logInfo("\(self.descriptor).remotePreferredConnectionConfiguration: ", .bindings, .next)
                .subscribe(onNext: self.remotePreferredConnectionConfiguration.accept)
                .disposed(by: disposeBag)

            remotePreferredConnectionMode
                .optionalize()
                .catchErrorJustReturn(nil)
                .logInfo("\(self.descriptor).remotePreferredConnectionMode: ", .bindings, .next)
                .subscribe(onNext: self.remotePreferredConnectionMode.accept)
                .disposed(by: disposeBag)
        }

        deinit {
            LogBindingsDebug("\(self ??? "Node.Connection") deinited")
        }
    }
}
