//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CentralManagerSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/24/17.
//

import Foundation
import RxBluetoothKit
import RxSwift

private enum CentralManagerSourceError: Error {
    case missingValue
}

public class CentralManagerSource: NSObject, DataSource {
    fileprivate let characteristic: CharacteristicType

    private let invalidateSubject = PublishSubject<Void>()
    private let disposeBag = DisposeBag()

    fileprivate var dataSink: DataSink?

    public init(sinkScheduler: ImmediateSchedulerType, characteristic: CharacteristicType) {
        self.characteristic = characteristic

        super.init()

        LogBluetoothInfo("CentralManagerSource: Created with characteristic: \(characteristic).")

        characteristic
            .observeValueUpdateAndSetNotification()
            .map { characteristic in
                guard let value = characteristic.value else {
                    throw CentralManagerSourceError.missingValue
                }

                return value
            }
            .observeOn(sinkScheduler)
            .takeUntil(invalidateSubject)
            .catchError { _ in .empty() }
            .subscribe(onNext: { [weak self] (value: Data) in
                LogBluetoothVerbose("CentralManagerSource: \(NSDataBuffer(value as NSData).debugDescription)")
                _ = try? self?.dataSink?.put(value)
            })
            .disposed(by: disposeBag)
    }

    /// Unsubscribe from receiving any more data on this source.
    public func invalidate() {
        LogBluetoothInfo("CentralManagerSource: Invalidate central manager source with \(characteristic).")
        invalidateSubject.onNext(())
    }

    public func setDataSink(_ dataSink: DataSink?) {
        self.dataSink = dataSink
    }

    deinit {
        LogBluetoothInfo("CentralManagerSource: Deinited \(characteristic).")
    }
}
