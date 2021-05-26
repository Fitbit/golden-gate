//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DataSinkListener.swift
//  GoldenGate
//
//  Created by Vlad Corneci on 03/06/2020.
//

public protocol DataSinkListener: class {
    /// Called when the DataSink accepts more data after reporting `GG_WOULD_BLOCK`
    func onCanPut()
}
