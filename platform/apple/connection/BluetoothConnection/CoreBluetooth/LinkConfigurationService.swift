//
//  LinkConfigurationService.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/19/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxSwift

/// A type that exposes APIs to *notify* or *receive read requests from* centrals subscribed to the link configuration GATT service. A Node
/// uses information provided by a Hub through the link configuration service to configure various aspects of the Bluetooth connection.
public protocol LinkConfigurationServiceType {
    /// The incoming read requests for the preferred connection configuration charateristic exposed by the service.
    var preferredConnectionConfigurationReadRequests: Observable<ReadRequest> { get }

    /// The incoming read requests for the preferred connection mode charateristic exposed by the service.
    var preferredConnectionModeReadRequests: Observable<ReadRequest> { get }

    /// Notify a specified central with an update on the preferred connection configuration charateristic.
    func update(preferredConnectionConfigurationValue: LinkConfigurationService.PreferredConnectionConfiguration, onSubscribedCentralUuid centralUuid: UUID)

    /// Notify a specified central with an update on the preferred connection mode charateristic.
    func update(preferredConnectionModeValue: LinkConfigurationService.PreferredConnectionMode, onSubscribedCentralUuid centralUuid: UUID)

    /// Notify a specified central with an update on the general purpose command charateristic.
    func send(command: LinkConfigurationService.GeneralPurposeCommand, onSubscribedCentralUuid centralUuid: UUID)
}

public class LinkConfigurationService: LinkConfigurationServiceType, PeripheralManagerService {
    public let advertise: Bool = false

    public weak var peripheralManager: PeripheralManager?

    public let didReceiveRead = PublishSubject<ReadRequest>()

    public let didReceiveWrite = PublishSubject<WriteRequest>()

    public let service: CBMutableService

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
    
    public init(configuration: BluetoothConfiguration.LinkConfigurationServiceConfiguration, peripheralManager: PeripheralManager) {
        preferredConnectionConfiguration = CBMutableCharacteristic(
            type: configuration.preferredConnectionConfigurationUUID,
            properties: [.read, .notify],
            value: nil,
            permissions: [.readable]
        )

        preferredConnectionMode = CBMutableCharacteristic(
            type: configuration.preferredConnectionModeUUID,
            properties: [.read, .notify],
            value: nil,
            permissions: [.readable]
        )

        generalPurposeCommand = CBMutableCharacteristic(
            type: configuration.generalPurposeCommandUUID,
            properties: [.notify],
            value: nil,
            permissions: []
        )

        service = CBMutableService(
            type: configuration.serviceUUID,
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
            identifier: String(describing: Self.self),
            peripheralManager: peripheralManager.peripheralManager,
            readyToUpdateSubscribers: peripheralManager.readyToUpdateSubscribers,
            bluetoothScheduler: peripheralManager.scheduler
        )
    }

    public func update(preferredConnectionConfigurationValue: PreferredConnectionConfiguration, onSubscribedCentralUuid centralUuid: UUID) {
        characteristicUpdater.updateValue(
            preferredConnectionConfigurationValue.rawValue,
            forCharacteristic: preferredConnectionConfiguration,
            onSubscribedCentralUuid: centralUuid
        )
    }

    public func update(preferredConnectionModeValue: PreferredConnectionMode, onSubscribedCentralUuid centralUuid: UUID) {
        characteristicUpdater.updateValue(
            preferredConnectionModeValue.rawValue,
            forCharacteristic: preferredConnectionMode,
            onSubscribedCentralUuid: centralUuid
        )
    }

    public func send(command: GeneralPurposeCommand, onSubscribedCentralUuid centralUuid: UUID) {
        characteristicUpdater.updateValue(
            command.rawValue,
            forCharacteristic: generalPurposeCommand,
            onSubscribedCentralUuid: centralUuid
        )
    }
}

/// Default Hub Configurations
extension LinkConfigurationService.PreferredConnectionConfiguration.ModeConfiguration {
    public static let fast = LinkConfigurationService.PreferredConnectionConfiguration.ModeConfiguration(
        min: 12,
        max: 12,
        slaveLatency: 0,
        supervisionTimeout: 20
    )

    public static let slow = LinkConfigurationService.PreferredConnectionConfiguration.ModeConfiguration(
        min: 96,
        max: 116,
        slaveLatency: 3,
        supervisionTimeout: 40
    )
}

extension LinkConfigurationService.PreferredConnectionConfiguration.DLEConfiguration {
    public static let `default` = LinkConfigurationService.PreferredConnectionConfiguration.DLEConfiguration(
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
