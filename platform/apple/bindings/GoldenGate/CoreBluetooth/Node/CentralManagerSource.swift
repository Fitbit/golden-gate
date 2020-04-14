//
//  CentralManagerSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/24/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
import GoldenGateXP
import Rxbit
import RxBluetoothKit
import RxSwift

private enum CentralManagerSourceError: Error {
    case missingValue
}

public class CentralManagerSource: NSObject, DataSource {
    fileprivate let runLoop: RunLoop
    fileprivate let peripheral: Peripheral
    fileprivate let characteristic: Characteristic

    private let invalidateSubject = PublishSubject<Void>()

    fileprivate var dataSink: DataSink?

    public init(runLoop: RunLoop, peripheral: Peripheral, characteristic: Characteristic) {
        self.runLoop = runLoop
        self.peripheral = peripheral
        self.characteristic = characteristic

        super.init()

        LogBindingsInfo("CentralManagerSource: Created with characteristic: \(characteristic).")

        characteristic
            .observeValueUpdateAndSetNotification()
            .map { characteristic in
                guard let value = characteristic.value else {
                    throw CentralManagerSourceError.missingValue
                }

                return value
            }
            .observeOn(runLoop)
            .takeUntil(invalidateSubject)
            .subscribe(
                onNext: { [weak self] (value: Data) in
                    LogBindingsVerbose("CentralManagerSource: \(NSDataBuffer(value as NSData).debugDescription)")
                    _ = try? self?.dataSink?.put(value)
                },
                onError: { LogBindingsWarning("\($0)") }
            )
            .disposed(by: disposeBag)
    }

    /// Unsubscribe from receiving any more data on this source.
    public func invalidate() {
        LogBindingsInfo("CentralManagerSource: Invalidate central manager source with \(characteristic).")
        invalidateSubject.onNext(())
    }

    public func setDataSink(_ dataSink: DataSink?) {
        self.dataSink = dataSink
    }

    deinit {
        LogBindingsInfo("CentralManagerSource: Deinited \(characteristic).")
    }
}
