//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSource.swift
//  GoldenGate
//
//  Created by Vlad Corneci on 03/06/2020.
//

public protocol DataSource: AnyObject {
    /// Sets the sink to which this source writes data to.
    func setDataSink(_ dataSink: DataSink?) throws
}
