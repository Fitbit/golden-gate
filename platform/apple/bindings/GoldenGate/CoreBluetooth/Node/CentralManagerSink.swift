//
//  CentralManagerSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/24/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import GoldenGateXP
import RxBluetoothKit
import RxSwift

/// On platforms that support `canSendWriteWithoutResponse`, it will communicate back-pressure.
public class CentralManagerSink: NSObject, DataSink {
    fileprivate let runLoop: RunLoop
    fileprivate let peripheral: Peripheral
    fileprivate let characteristic: Characteristic
    fileprivate let writeType: CBCharacteristicWriteType

    fileprivate weak var dataSinkListener: DataSinkListener?
    fileprivate var reportedFailure = false
    fileprivate var waitingForReponse = false

    public init(runLoop: RunLoop, peripheral: Peripheral, characteristic: Characteristic) {
        self.runLoop = runLoop
        self.peripheral = peripheral
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
                throw GGRawError.wouldBlock
            }

            waitingForReponse = true
        case .withoutResponse:
            // TODO: IPD-81539 Hack to make it work for now. It should be removed once we implement the support for peripheralIsReadyToSendWriteWithoutResponse.
//            if #available(iOS 11, OSX 10.13, *) {
//                guard peripheral.cbPeripheral.canSendWriteWithoutResponse else {
//                    reportedFailure = true
//                    throw GGRawError.wouldBlock
//                }
//            }
            _ = ()
        @unknown default:
            break
        }

        // Log header of outbound packet
        LogBindingsVerbose("CentralManagerSink: \(buffer.debugDescription)")

        _ = peripheral.writeValue(buffer.data, for: characteristic, type: writeType, canSendWriteWithoutResponseCheckEnabled: false)
            .catchErrorJustReturn(characteristic)
            .subscribe(onSuccess: { _ in
                switch self.writeType {
                case .withResponse:
                    self.waitingForReponse = false
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

        runLoop.async {
            self.reportedFailure = false
            self.dataSinkListener?.onCanPut()
        }
    }
}
