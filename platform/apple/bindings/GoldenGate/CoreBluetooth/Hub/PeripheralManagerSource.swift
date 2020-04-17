//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeripheralManagerSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 10/25/17.
//

import CoreBluetooth
import Foundation
import GoldenGateXP
import Rxbit
import RxSwift

/// Is able to cache a SINGLE write request before the DataSink is set.
public class PeripheralManagerSource: DataSource {
    private let runLoop: RunLoop
    private let wrappedDataSink = WrappedDataSink()
    private let disposeBag = DisposeBag()
    private let internalQueue = DispatchQueue(label: "PeripheralManagerSource")

    public init(runLoop: RunLoop, writeRequests: Observable<WriteRequest>) {
        self.runLoop = runLoop

        writeRequests
            .subscribe(onNext: { [weak self] request in
                LogBindingsDebug("PeripheralManagerSource: Write request received \(request)")
                let result = request.value.flatMap { self?.onReceived($0) }
                request.respond(withResult: result ?? .writeNotPermitted)
            })
            .disposed(by: disposeBag)
    }

    private var cachedValue: Data?

    private func onReceived(_ value: Data) -> CBATTError.Code {
        return internalQueue.sync {
            guard cachedValue == nil else {
                LogBindingsError("\(self).onReceived another write but couldn't despool previous write yet.")
                return .writeNotPermitted
            }

            guard wrappedDataSink.dataSink != nil else {
                LogBindingsDebug("\(self).onReceived Caching incoming Data: \(value as NSData)")
                cachedValue = value
                return .success
            }

            runLoop.async { [wrappedDataSink] in
                do {
                    LogBindingsDebug("\(self).onReceived \(value as NSData)")
                    try wrappedDataSink.put(value)
                } catch {
                    LogBindingsError("\(self).onReceived Failed to process incoming data \(0)!")
                }
            }

            return .success
        }
    }

    public func setDataSink(_ newDataSink: DataSink?) {
        internalQueue.sync {
            LogBindingsDebug("\(self).setDataSink \(newDataSink ??? "nil")")

            wrappedDataSink.dataSink = newDataSink

            guard
                wrappedDataSink.dataSink != nil,
                let cachedValue = cachedValue
            else { return }

            self.cachedValue = nil

            runLoop.async { [wrappedDataSink] in
                do {
                    LogBindingsDebug("\(self).setDataSink putconnect \(cachedValue as NSData)")
                    try wrappedDataSink.put(cachedValue)
                } catch {
                    LogBindingsError("\(self).setDataSink Failed to despool cached value \(0)!")
                }
            }
        }
    }
}

private class WrappedDataSink: DataSink {
    private let internalQueue = DispatchQueue(label: "SafeDataSink")
    private var wrapped: DataSink?

    fileprivate var dataSink: DataSink? {
        set {
            objc_sync_enter(self)
            defer { objc_sync_exit(self) }

            wrapped = newValue
        }
        get {
            objc_sync_enter(self)
            defer { objc_sync_exit(self) }

            return wrapped
        }
    }

    init() {}

    func put(_ buffer: Buffer, metadata: DataSink.Metadata?) throws {
        objc_sync_enter(self)
        defer { objc_sync_exit(self) }

        LogBindingsDebug("\(self).put \(buffer) to \(wrapped ??? "nil")")
        try wrapped?.put(buffer, metadata: metadata)
    }

    func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        objc_sync_enter(self)
        defer { objc_sync_exit(self) }

        try wrapped?.setDataSinkListener(dataSinkListener)
    }
}
