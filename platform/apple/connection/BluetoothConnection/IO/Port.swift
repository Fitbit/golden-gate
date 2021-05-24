//
//  Port.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 3/7/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

/// A port that can send and receive data.
public protocol Port {
    var dataSink: DataSink { get }
    var dataSource: DataSource { get }
}
