//
//  CentralManagerSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/24/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxSwift

/// On platforms that support `canSendWriteWithoutResponse`, it will communicate back-pressure.
public class CentralManagerSink: NSObject, DataSink {
    fileprivate let listenerScheduler: ImmediateSchedulerType
    fileprivate let characteristic: CharacteristicType
    fileprivate let writeType: CBCharacteristicWriteType

    fileprivate weak var dataSinkListener: DataSinkListener?
    fileprivate var reportedFailure = false
    fileprivate var waitingForReponse = false

    public init(listenerScheduler: ImmediateSchedulerType, characteristic: CharacteristicType) {
        self.listenerScheduler = listenerScheduler
        self.characteristic = characteristic
        self.writeType = characteristic.properties.contains(.writeWithoutResponse) ?
            .withoutResponse : .withResponse

        super.init()
    }

    public func put(_ buffer: Buffer, metadata: Metadata?) throws {
        switch writeType {
        case .withResponse:
            guard !waitingForReponse else {
                reportedFailure = true
                throw BluetoothConnectionError.wouldBlock
            }

            waitingForReponse = true
        case .withoutResponse:
            // TODO: IPD-81539 Hack to make it work for now. It should be removed once we implement the support for peripheralIsReadyToSendWriteWithoutResponse.
//            if #available(iOS 11, OSX 10.13, *) {
//                guard peripheral.cbPeripheral.canSendWriteWithoutResponse else {
//                    reportedFailure = true
//                    throw BluetoothConnectionError.wouldBlock
//                }
//            }
            _ = ()
        @unknown default:
            break
        }

        // Log header of outbound packet
        LogBluetoothVerbose("CentralManagerSink: \(buffer.debugDescription)")

        _ = characteristic.service().peripheral().writeValue(buffer.data, for: characteristic, type: writeType, canSendWriteWithoutResponseCheckEnabled: false)
            .catchErrorJustReturn(characteristic)
            .do(onSuccess: { _ in self.waitingForReponse = false })
            .observeOn(listenerScheduler)
            .subscribe(onSuccess: { _ in
                switch self.writeType {
                case .withResponse:
                    self.notify()
                case .withoutResponse:
                    // nothing to do
                    break
                @unknown default:
                    break
                }
            })
    }

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        self.dataSinkListener = dataSinkListener
    }

    private func notify() {
        guard reportedFailure else { return }

        reportedFailure = false
        dataSinkListener?.onCanPut()
    }
}
