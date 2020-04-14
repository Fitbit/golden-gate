//
//  MockDataSink.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
@testable import GoldenGate
import GoldenGateXP
import RxCocoa

class MockDataSink: DataSink {
    private var closure: ((Buffer, Metadata?) throws -> Void)?

    let dataSinkListener = BehaviorRelay<DataSinkListener?>(value: nil)
    var result: GG_Result = GG_SUCCESS

    init() {
        self.closure = { [unowned self] _, _ in try self.result.rethrow() }
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
