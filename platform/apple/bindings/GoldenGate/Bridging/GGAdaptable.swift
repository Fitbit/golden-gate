//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GGAdaptable.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 10/23/17.
//

import Foundation
import GoldenGateXP

/// Utility for classes that implement just a single GoldenGate interface.
///
/// Use this as inspiration if you have a class that implements multiple interfaces at once.
internal protocol GGAdaptable: AnyObject {
    associatedtype GGObject
    associatedtype GGInterface
    typealias Adapter = GGAdapter<GGObject, GGInterface, Self>

    var adapter: Adapter { get }
}

extension GGAdaptable {
    var ref: UnsafeMutablePointer<GGObject> {
        return Adapter.passUnretained(adapter)
    }
}
