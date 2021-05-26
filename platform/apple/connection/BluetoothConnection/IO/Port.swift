//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Port.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 3/7/18.
//

/// A port that can send and receive data.
public protocol Port {
    var dataSink: DataSink { get }
    var dataSource: DataSource { get }
}
