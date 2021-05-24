//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockDataSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//

@testable import BluetoothConnection
import Foundation
import RxCocoa

class MockDataSink: DataSink {
    private var closure: ((Buffer, Metadata?) throws -> Void)?

    let dataSinkListener = BehaviorRelay<DataSinkListener?>(value: nil)

    init() {
        self.closure = { _, _ in return }
    }

    init(closure: @escaping (Buffer, Metadata?) throws -> Void) {
        self.closure = closure
    }

    convenience init(closure: @escaping (Buffer) throws -> Void) {
        self.init { buffer, _ in try closure(buffer) }
    }

    func put(_ buffer: Buffer, metadata: Metadata?) throws {
        try closure?(buffer, metadata)
    }

    func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        self.dataSinkListener.accept(dataSinkListener)
    }
}
