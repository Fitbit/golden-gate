//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RemoteAPIError.swift
//  GoldenGate
//
//  Created by Vlad-Mihai Corneci on 04/03/2019.
//

public enum RemoteAPIError: Error {
    case invalidNumberOfPairedDevices(Int)
}
