//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  DiscoveredPeer.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 11/6/17.
//

import CoreBluetooth
import Foundation
import RxBluetoothKit
import RxRelay
import RxSwift

/// A nearby Bluetooth peer discovered by the peer discoverer, either by scanning or retrieving connected peers.
/// The peer's attributes can be updated as the discovery process continues.
public protocol DiscoveredPeerType {
    /// Peer's unique identifier
    var identifier: UUID { get }
    /// Peer's name
    var name: String { get }
    /// A list of services implemented by the peer, if available
    var serviceUuids: [CBUUID]? { get }
    /// Information advertised by the peer. Not available for already connected peers.
    var advertisementData: AdvertisementData? { get }
    /// The signal strength of the peer. It gets updated periodically as the discovery process continues.
    var rssi: BehaviorRelay<Int> { get }
}

public final class DiscoveredPeer: DiscoveredPeerType {
    /// Faulty RSSI readings are indicated by 127.
    public static let invalidRssi = 127

    private var peripheral: PeripheralType

    public var identifier: UUID {
        peripheral.identifier
    }

    public var name: String {
        advertisementData?.localName ?? peripheral.name ?? identifier.uuidString
    }

    public var serviceUuids: [CBUUID]? {
        peripheral.services()?.map(\.uuid) ?? advertisementData?.serviceUUIDs
    }

    public private(set) var advertisementData: AdvertisementData?

    public let rssi: BehaviorRelay<Int>

    init(peripheral: PeripheralType) {
        self.peripheral = peripheral
        self.advertisementData = nil
        self.rssi = BehaviorRelay(value: Self.invalidRssi)
    }

    func update(peripheral: PeripheralType, advertisementData: AdvertisementData?, rssi: Int) {
        self.peripheral = peripheral

        if let advertisementData = advertisementData {
            self.advertisementData = advertisementData
        }

        self.rssi.accept(rssi)
    }
}

extension DiscoveredPeer: Hashable {
    public static func == (lhs: DiscoveredPeer, rhs: DiscoveredPeer) -> Bool {
        lhs.identifier == rhs.identifier
    }

    public func hash(into hasher: inout Hasher) {
        hasher.combine(identifier)
    }
}

extension DiscoveredPeer: CustomStringConvertible {
    public var description: String {
        "\(peripheral)"
    }
}
