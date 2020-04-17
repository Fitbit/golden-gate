//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeripheralManagerSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/24/17.
//

import CoreBluetooth
import Foundation
import GoldenGateXP
import RxSwift

public protocol PeripheralManagerSinkWriter {
    /// - Parameter data: The data to write.
    /// - Returns: Returns `false`, if the data could not be written and
    ///     `readyToUpdateSubscribers` will be called once it's possible again.
    /// - Seealso: `CBPeripheralManager.updateValue(forCharacteristic:onSubscribedCentrals:)`
    func write(_ data: Data) -> Bool
}

public class PeripheralManagerSink: DataSink {
    fileprivate let runLoop: RunLoop
    fileprivate let writer: PeripheralManagerSinkWriter
    fileprivate let disposeBag = DisposeBag()

    fileprivate weak var dataSinkListener: DataSinkListener?
    fileprivate var reportedFailure = false

    public init(
        runLoop: RunLoop,
        writer: PeripheralManagerSinkWriter,
        readyToUpdateSubscribers: Observable<Void>) {

        self.runLoop = runLoop
        self.writer = writer

        readyToUpdateSubscribers
            .subscribe(onNext: { [weak self] in self?.notify() })
            .disposed(by: disposeBag)
    }

    public func put(_ buffer: Buffer, metadata: Metadata?) throws {
        guard !reportedFailure else {
            LogBindingsError("PeripheralManagerSink: Would block error.")
            throw GGRawError.wouldBlock
        }

        let success = writer.write(buffer.data)
        // Log header of outbound packet
        LogBindingsDebug("PeripheralManagerSink: Sending \(buffer.debugDescription) \(buffer.data as NSData) success \(success)")

        guard success else {
            reportedFailure = true
            LogBindingsError("PeripheralManagerSink: Would block error.")
            throw GGRawError.wouldBlock
        }
    }

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        self.dataSinkListener = dataSinkListener
    }
}

private extension PeripheralManagerSink {
    func notify() {
        runLoop.async {
            guard self.reportedFailure else { return }
            self.reportedFailure = false
            self.dataSinkListener?.onCanPut()
        }
    }
}

extension PeripheralManagerSink {
    public convenience init(
        runLoop: RunLoop,
        peripheralManager: CBPeripheralManager,
        central: CBCentral,
        characteristic: CBMutableCharacteristic,
        readyToUpdateSubscribers: Observable<Void>) {

        self.init(
            runLoop: runLoop,
            writer: Writer(
                peripheralManager: peripheralManager,
                characteristic: characteristic,
                central: central
            ),
            readyToUpdateSubscribers: readyToUpdateSubscribers
        )
    }
}

private struct Writer: PeripheralManagerSinkWriter {
    let peripheralManager: CBPeripheralManager
    let characteristic: CBMutableCharacteristic
    let central: CBCentral

    func write(_ data: Data) -> Bool {
        let result = peripheralManager.updateValue(
            data,
            for: characteristic,
            onSubscribedCentrals: [central]
        )

        LogBluetoothDebug(
            "PeripheralManagerSink Writer: Updated value \(data as NSData) on characteristic \(characteristic) for centrals \([central]) with result \(result)"
        )

        return result
    }
}
