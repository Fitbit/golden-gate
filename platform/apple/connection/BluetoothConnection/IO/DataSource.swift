//
//  DataSource.swift
//  GoldenGate
//
//  Created by Vlad Corneci on 03/06/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

public protocol DataSource: class {
    /// Sets the sink to which this source writes data to.
    func setDataSink(_ dataSink: DataSink?) throws
}
