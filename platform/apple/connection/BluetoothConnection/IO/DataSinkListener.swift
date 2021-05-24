//
//  DataSinkListener.swift
//  GoldenGate
//
//  Created by Vlad Corneci on 03/06/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

public protocol DataSinkListener: class {
    /// Called when the DataSink accepts more data after reporting `GG_WOULD_BLOCK`
    func onCanPut()
}
