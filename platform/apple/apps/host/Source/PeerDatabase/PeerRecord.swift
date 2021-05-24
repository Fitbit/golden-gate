//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeerRecord.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import BluetoothConnection
import Foundation

class PeerRecord: Codable {
    let handle: PeerRecordHandle
    let name: String

    var peerDescriptor: PeerDescriptor? {
        didSet {
            if oldValue != peerDescriptor {
                context?.recordDidChange()
            }
        }
    }

    var preferredConnectionConfiguration: LinkConfigurationService.PreferredConnectionConfiguration = .default {
        didSet {
            if oldValue != preferredConnectionConfiguration {
                context?.recordDidChange()
            }
        }
    }

    var preferredConnectionMode: LinkConfigurationService.PreferredConnectionMode = .default {
        didSet {
            if oldValue != preferredConnectionMode {
                context?.recordDidChange()
            }
        }
    }

    // swiftlint:disable:next redundant_optional_initialization - explicit nil needed for Codable
    weak var context: DatabaseContext? = nil

    init(handle: PeerRecordHandle, name: String) {
        self.handle = handle
        self.name = name
    }

    enum CodingKeys: CodingKey {
        case handle
        case name
        case peerDescriptor
        case preferredConnectionConfiguration
        case preferredConnectionMode
    }
}
