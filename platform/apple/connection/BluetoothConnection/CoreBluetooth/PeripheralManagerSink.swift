//
//  PeripheralManagerSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/24/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxSwift

public protocol PeripheralManagerSinkWriter {
    /// - Parameter data: The data to write.
    /// - Returns: Returns `false`, if the data could not be written and
    ///     `readyToUpdateSubscribers` will be called once it's possible again.
    /// - Seealso: `CBPeripheralManager.updateValue(forCharacteristic:onSubscribedCentrals:)`
    func write(_ data: Data) -> Bool
}

public class PeripheralManagerSink: DataSink {
    fileprivate let writer: PeripheralManagerSinkWriter
    fileprivate let disposeBag = DisposeBag()

    fileprivate weak var dataSinkListener: DataSinkListener?
    fileprivate var reportedFailure = false

    public init(
        listenerScheduler: ImmediateSchedulerType,
        writer: PeripheralManagerSinkWriter,
        readyToUpdateSubscribers: Observable<Void>
    ) {
        self.writer = writer

        readyToUpdateSubscribers
            .observeOn(listenerScheduler)
            .subscribe(onNext: { [weak self] in self?.notify() })
            .disposed(by: disposeBag)
    }

    public func put(_ buffer: Buffer, metadata: Metadata?) throws {
        guard !reportedFailure else {
            LogBluetoothError("PeripheralManagerSink: Would block error.")
            throw BluetoothConnectionError.wouldBlock
        }

        let success = writer.write(buffer.data)
        // Log header of outbound packet
        LogBluetoothDebug("PeripheralManagerSink: Sending \(buffer.debugDescription) \(buffer.data as NSData) success \(success)")

        guard success else {
            reportedFailure = true
            LogBluetoothError("PeripheralManagerSink: Would block error.")
            throw BluetoothConnectionError.wouldBlock
        }
    }

    public func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        self.dataSinkListener = dataSinkListener
    }
}

private extension PeripheralManagerSink {
    func notify() {
        guard self.reportedFailure else { return }
        
        self.reportedFailure = false
        self.dataSinkListener?.onCanPut()
    }
}

extension PeripheralManagerSink {
    public convenience init(
        listenerScheduler: ImmediateSchedulerType,
        peripheralManager: CBPeripheralManager,
        central: CBCentral,
        characteristic: CBMutableCharacteristic,
        readyToUpdateSubscribers: Observable<Void>) {

        self.init(
            listenerScheduler: listenerScheduler,
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
