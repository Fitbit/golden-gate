//
//  PeripheralManagerSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/25/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import CoreBluetooth
import Foundation
import RxSwift

/// Is able to cache a SINGLE write request before the DataSink is set.
public class PeripheralManagerSource: DataSource {
    private var dataSink: DataSink?
    private let incomingDataSubject = PublishSubject<Data>()
    private let disposeBag = DisposeBag()
    private let internalQueue = DispatchQueue(label: "PeripheralManagerSource")

    public init(sinkScheduler: ImmediateSchedulerType, writeRequests: Observable<WriteRequest>) {
        writeRequests
            .subscribe(onNext: { [weak self] request in
                LogBluetoothDebug("PeripheralManagerSource: Write request received \(request)")
                let result = request.value.flatMap { self?.onReceived($0) }
                request.respond(withResult: result ?? .writeNotPermitted)
            })
            .disposed(by: disposeBag)

        incomingDataSubject
            .observeOn(sinkScheduler)
            .subscribe(onNext: { [weak self] data in
                guard let self = self else { return }

                let dataSink = self.internalQueue.sync { self.dataSink }

                LogBluetoothDebug("PeripheralManagerSource.put \(data) to \(dataSink ??? "nil")")

                do {
                    try dataSink?.put(data)
                } catch {
                    LogBluetoothError("PeripheralManagerSource.incomingDataSubject Failed to process incoming data \(error)!")
                }
            })
            .disposed(by: disposeBag)
    }

    private var cachedValue: Data?

    private func onReceived(_ value: Data) -> CBATTError.Code {
        return internalQueue.sync {
            guard cachedValue == nil else {
                LogBluetoothError("PeripheralManagerSource.onReceived another write but couldn't despool previous write yet.")
                return .writeNotPermitted
            }

            guard dataSink != nil else {
                LogBluetoothDebug("PeripheralManagerSource.onReceived Caching incoming Data: \(value as NSData)")
                cachedValue = value
                return .success
            }

            LogBluetoothDebug("PeripheralManagerSource.onReceived \(value as NSData)")
            incomingDataSubject.onNext(value)

            return .success
        }
    }

    public func setDataSink(_ newDataSink: DataSink?) {
        internalQueue.sync {
            LogBluetoothDebug("PeripheralManagerSource.setDataSink \(newDataSink ??? "nil")")
            
            dataSink = newDataSink

            guard
                dataSink != nil,
                let cachedValue = cachedValue
            else { return }

            self.cachedValue = nil

            LogBluetoothDebug("PeripheralManagerSource.setDataSink put cached value \(cachedValue as NSData)")
            incomingDataSubject.onNext(cachedValue)
        }
    }
}
