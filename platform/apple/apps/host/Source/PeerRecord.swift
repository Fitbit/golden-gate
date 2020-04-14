//
//  PeerRecord.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation
import GoldenGate

class PeerRecord: Codable {
    let handle: PeerRecordHandle
    let name: String

    var bluetoothPeerDescriptor: BluetoothPeerDescriptor? {
        didSet {
            if oldValue != bluetoothPeerDescriptor {
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
        case bluetoothPeerDescriptor
        case preferredConnectionConfiguration
        case preferredConnectionMode
    }
}
