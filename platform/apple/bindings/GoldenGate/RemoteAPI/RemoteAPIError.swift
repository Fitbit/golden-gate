//
//  RemoteAPIError.swift
//  GoldenGate
//
//  Created by Vlad-Mihai Corneci on 04/03/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
//

public enum RemoteAPIError: Error {
    case invalidNumberOfPairedDevices(Int)
}
