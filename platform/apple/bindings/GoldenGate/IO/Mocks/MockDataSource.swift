//
//  MockDataSource.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 12/7/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import Foundation
@testable import GoldenGate

class MockDataSource: DataSource {
    var dataSink: DataSink?

    func setDataSink(_ dataSink: DataSink?) throws {
        self.dataSink = dataSink
    }
}
