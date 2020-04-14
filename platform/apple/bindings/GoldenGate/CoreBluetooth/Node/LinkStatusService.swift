//
//  LinkStatusService.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 3/23/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxSwift

public class LinkStatusService: PeripheralManagerService {
    public let advertise: Bool = true

    public weak var peripheralManager: PeripheralManager?

    public let service: CBMutableService

    public let didReceiveRead = PublishSubject<ReadRequest>()

    public let didReceiveWrite = PublishSubject<WriteRequest>()

    private let currentConnectionConfiguration: CBMutableCharacteristic

    private let currentConnectionStatus: CBMutableCharacteristic

    private let security: CBMutableCharacteristic

    private let characteristicUpdater: CharacteristicUpdater

    private let disposeBag = DisposeBag()

    public init(configuration: BluetoothConfiguration, peripheralManager: PeripheralManager) {
        currentConnectionConfiguration = CBMutableCharacteristic(
            type: configuration.linkStatusService.currentConnectionConfigurationUUID,
            properties: [.read, .notify],
            value: nil,
            permissions: [.readable]
        )

        currentConnectionStatus = CBMutableCharacteristic(
            type: configuration.linkStatusService.currentConnectionStatusUUID,
            properties: [.read, .notify],
            value: nil,
            permissions: [.readable]
        )

        security = CBMutableCharacteristic(
            type: configuration.linkStatusService.bondSecureUUID,
            properties: [.read],
            value: nil,
            permissions: [.readEncryptionRequired]
        )

        service = CBMutableService(
            type: configuration.linkStatusService.serviceUUID,
            primary: true
        )

        service.characteristics = [
            currentConnectionConfiguration,
            currentConnectionStatus,
            security
        ]

        self.peripheralManager = peripheralManager

        self.characteristicUpdater = CharacteristicUpdater(
            peripheralManager: peripheralManager.peripheralManager,
            readyToUpdateSubscribers: peripheralManager.readyToUpdateSubscribers,
            scheduler: peripheralManager.scheduler
        )

        didReceiveRead
            .subscribe(onNext: { [weak self] request in
                let result = self?.didReceive(request)
                request.respond(withResult: result ?? .failure(.readNotPermitted))
            })
            .disposed(by: disposeBag)
    }

    public var subscribedCentrals: [SubscribedCentral] {
        let characteristics = [currentConnectionStatus]

        return characteristics.flatMap { characteristic -> [SubscribedCentral] in
            return (characteristic.subscribedCentrals ?? []).map {
                (central: $0, characteristic: characteristic as CBCharacteristic)
            }
        }
    }

    func didReceive(_ request: ReadRequest) -> ReadRequest.Result {
        switch request.characteristic.uuid {
        case currentConnectionConfiguration.uuid:
            return .success(LinkStatusService.ConnectionConfiguration.default.rawValue)
        case currentConnectionStatus.uuid:
            return .success(currentConnectionStatusValue.rawValue)
        default:
            return .failure(.readNotPermitted)
        }
    }

    var currentConnectionStatusValue: LinkStatusService.ConnectionStatus = .default {
        didSet {
            characteristicUpdater.updateValue(
                currentConnectionStatusValue.rawValue,
                for: currentConnectionStatus,
                onSubscribedCentral: nil
            )
        }
    }
}

extension LinkStatusService.ConnectionConfiguration {
    static let `default` = LinkStatusService.ConnectionConfiguration(connectionInterval: 12, slaveLatency: 0, supervisionTimeout: 20, MTU: 185, mode: .fast)
}

extension LinkStatusService.ConnectionStatus {
    static let `default` = LinkStatusService.ConnectionStatus(
        bonded: false,
        encrypted: false,
        dleEnabled: false,
        dleRebootRequired: false,
        halfBonded: false,
        maxTxPayloadLength: 0,
        maxTxTime: 0,
        maxRxPayloadLength: 0,
        maxRxTime: 0
    )
}
