//
//  PeerRecordHandle.swift
//  GoldenGateHost
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation

/// A unique handle to a peer record (used for serialization and uniqueness checks).
struct PeerRecordHandle: Codable {
    private let uuid: UUID

    /// Creates a new peer.
    init() {
        uuid = UUID()
    }

    init(from decoder: Decoder) throws {
        let container = try decoder.singleValueContainer()
        uuid = try container.decode(UUID.self)
    }

    func encode(to encoder: Encoder) throws {
        var container = encoder.singleValueContainer()
        try container.encode(uuid)
    }
}

extension PeerRecordHandle: Hashable {
    func hash(into hasher: inout Hasher) {
        hasher.combine(uuid)
    }

    static func == (lhs: PeerRecordHandle, rhs: PeerRecordHandle) -> Bool {
        return lhs.uuid == rhs.uuid
    }
}
