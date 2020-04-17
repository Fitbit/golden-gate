//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  LinkConfigurationService.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//

import CoreBluetooth
import Foundation
import RxSwift

public class LinkConfigurationService: PeripheralManagerService {
    public let advertise: Bool = false

    public weak var peripheralManager: PeripheralManager?

    public let didReceiveRead = PublishSubject<ReadRequest>()

    public let didReceiveWrite = PublishSubject<WriteRequest>()

    public let service: CBMutableService

    /// The incoming read requests for all charateristics exposed
    /// by the service.
    // swiftlint:disable:next identifier_name
    public let preferredConnectionConfigurationReadRequests: Observable<ReadRequest>
    public let preferredConnectionModeReadRequests: Observable<ReadRequest>

    /// The characterstic on which preferred connection configuration
    /// is sent to other peers.
    let preferredConnectionConfiguration: CBMutableCharacteristic

    /// The characterstic on which preferred connection mode
    /// is sent to other peers.
    let preferredConnectionMode: CBMutableCharacteristic

    /// The characterstic on which general purpose commands
    /// are sent to other peers.
    let generalPurposeCommand: CBMutableCharacteristic

    private let characteristicUpdater: CharacteristicUpdater

    private let disposeBag = DisposeBag()

    public init(configuration: BluetoothConfiguration, peripheralManager: PeripheralManager) {
        preferredConnectionConfiguration = CBMutableCharacteristic(
            type: configuration.linkConfigurationService.preferredConnectionConfigurationUUID,
            properties: [.read, .notify],
            value: nil,
            permissions: [.readable]
        )

        preferredConnectionMode = CBMutableCharacteristic(
            type: configuration.linkConfigurationService.preferredConnectionModeUUID,
            properties: [.read, .notify],
            value: nil,
            permissions: [.readable]
        )

        generalPurposeCommand = CBMutableCharacteristic(
            type: configuration.linkConfigurationService.generalPurposeCommandUUID,
            properties: [.notify],
            value: nil,
            permissions: []
        )

        service = CBMutableService(
            type: configuration.linkConfigurationService.serviceUUID,
            primary: true
        )

        service.characteristics = [
            preferredConnectionConfiguration,
            preferredConnectionMode,
            generalPurposeCommand
        ]

        self.peripheralManager = peripheralManager

        self.preferredConnectionConfigurationReadRequests = didReceiveRead
            .filter { [preferredConnectionConfiguration] in
                $0.characteristic == preferredConnectionConfiguration
            }

        self.preferredConnectionModeReadRequests = didReceiveRead
            .filter { [preferredConnectionMode] in
                $0.characteristic == preferredConnectionMode
            }

        didReceiveWrite
            .subscribe(onNext: {
                $0.respond(withResult: .writeNotPermitted)
            })
            .disposed(by: disposeBag)

        self.characteristicUpdater = CharacteristicUpdater(
            peripheralManager: peripheralManager.peripheralManager,
            readyToUpdateSubscribers: peripheralManager.readyToUpdateSubscribers,
            scheduler: peripheralManager.scheduler
        )
    }

    public func update(preferredConnectionConfigurationValue: PreferredConnectionConfiguration, onSubscribedCentral central: UUID) {
        updateValue(preferredConnectionConfigurationValue.rawValue, for: preferredConnectionConfiguration, onSubscribedCentral: central)
    }

    public func update(preferredConnectionModeValue: PreferredConnectionMode, onSubscribedCentral central: UUID) {
        updateValue(preferredConnectionModeValue.rawValue, for: preferredConnectionMode, onSubscribedCentral: central)
    }

    public func send(command: GeneralPurposeCommand, onSubscribedCentral central: UUID) {
        updateValue(command.rawValue, for: generalPurposeCommand, onSubscribedCentral: central)
    }

    private func updateValue(_ value: Data, for characteristic: CBMutableCharacteristic, onSubscribedCentral centralId: UUID) {
        guard let central = characteristic.subscribedCentrals?.first(where: { $0.hackyIdentifier == centralId }) else {
            LogBluetoothError(
                """
                [LinkConfigurationService] Couldn't update characteristic \(characteristic.uuid) \
                because no subscribed central with id \(centralId) could be found.
                Subscribed centrals are: \(String(describing: characteristic.subscribedCentrals))
                """
            )
            return
        }

        characteristicUpdater.updateValue(value, for: characteristic, onSubscribedCentral: central)
    }
}

/// Default Hub Configurations

extension LinkConfigurationService.PreferredConnectionConfiguration.ModeConfiguration {
    static let fast = LinkConfigurationService.PreferredConnectionConfiguration.ModeConfiguration(
        min: 12,
        max: 12,
        slaveLatency: 0,
        supervisionTimeout: 20
    )

    static let slow = LinkConfigurationService.PreferredConnectionConfiguration.ModeConfiguration(
        min: 96,
        max: 116,
        slaveLatency: 3,
        supervisionTimeout: 40
    )
}

extension LinkConfigurationService.PreferredConnectionConfiguration.DLEConfiguration {
    static let `default` = LinkConfigurationService.PreferredConnectionConfiguration.DLEConfiguration(
        maxTxPDU: 0,
        maxTxTime: 0
    )
}

extension LinkConfigurationService.PreferredConnectionConfiguration {
    public static let `default` = LinkConfigurationService.PreferredConnectionConfiguration(
        fastModeConfiguration: .fast,
        slowModeConfiguration: nil,
        dleConfiguration: nil,
        MTU: nil
    )
}
