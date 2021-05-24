//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MockDataSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//

@testable import BluetoothConnection
import Foundation

class MockDataSource: DataSource {
    var dataSink: DataSink?

    func setDataSink(_ dataSink: DataSink?) throws {
        self.dataSink = dataSink
    }
}
