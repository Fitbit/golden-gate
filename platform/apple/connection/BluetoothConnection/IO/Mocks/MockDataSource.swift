//
//  MockDataSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

@testable import BluetoothConnection
import Foundation

class MockDataSource: DataSource {
    var dataSink: DataSink?

    func setDataSink(_ dataSink: DataSink?) throws {
        self.dataSink = dataSink
    }
}
