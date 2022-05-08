//
//  DiscoveredPeerSpec.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 30.06.2021.
//  Copyright © 2021 Fitbit. All rights reserved.
//

@testable import BluetoothConnection
import CoreBluetooth
import Nimble
import Quick
import RxBluetoothKit
import RxSwift
import RxTest

final class DiscoveredPeerSpec: QuickSpec {
    override func spec() {
        describe("identifier") {
            it("matches the identifier of the peripheral") {
                let peripheral = PeripheralMock()
                let peer = DiscoveredPeer(peripheral: peripheral)

                expect(peer.identifier) == peripheral.identifier
            }
        }

        describe("name") {
            it("matches the advertised local name if not nil") {
                let name = "ADVERTISED_NAME"
                let peripheral = PeripheralMock()
                let advertisementData = AdvertisementData(advertisementData: [CBAdvertisementDataLocalNameKey: name])

                let peer = DiscoveredPeer(peripheral: peripheral)
                peer.update(peripheral: peripheral, advertisementData: advertisementData, rssi: 0)

                expect(peer.name) == name
            }

            it("otherwise matches the periperal name if not nil") {
                let name = "PERIPHERAL_NAME"
                let peripheral = PeripheralMock()
                peripheral.name = name

                let peer = DiscoveredPeer(peripheral: peripheral)

                expect(peer.name) == name
            }

            it("otherwise matches the periperal identifier") {
                let peripheral = PeripheralMock()
                let peer = DiscoveredPeer(peripheral: peripheral)

                expect(peer.name) == peripheral.identifier.uuidString
            }
        }

        describe("service uuids") {
            it("returns the identifiers for the discovered services if not nil") {
                let peripheral = PeripheralMock()
                let service = ServiceMock(peripheral: peripheral)
                peripheral.servicesList = [service]

                let peer = DiscoveredPeer(peripheral: peripheral)

                expect(peer.serviceUuids) == [service.uuid]
            }

            it("otherwise returns the identifiers for the advertised services if not nil") {
                let peripheral = PeripheralMock()
                let identifier = CBUUID(string: "0001")
                let advertisementData = AdvertisementData(advertisementData: [CBAdvertisementDataServiceUUIDsKey: [identifier]])

                let peer = DiscoveredPeer(peripheral: peripheral)
                peer.update(peripheral: peripheral, advertisementData: advertisementData, rssi: 0)

                expect(peer.serviceUuids) == [identifier]
            }

            it("otherwise returns nil") {
                let peripheral = PeripheralMock()
                let peer = DiscoveredPeer(peripheral: peripheral)

                expect(peer.serviceUuids).to(beNil())
            }
        }

        describe("rssi") {
            var disposeBag: DisposeBag!

            beforeEach {
                disposeBag = DisposeBag()
            }

            it("starts with the initial value if not nil") {
                let peripheral = PeripheralMock()
                let rssi = -45
                let peer = DiscoveredPeer(peripheral: peripheral)
                peer.update(peripheral: peripheral, advertisementData: nil, rssi: rssi)

                var events: [Int] = []
                peer.rssi.subscribe(onNext: { events.append($0) }).disposed(by: disposeBag)

                expect(events) == [rssi]
            }

            it("starts with invalid if the initial value is nil") {
                let peripheral = PeripheralMock()
                let peer = DiscoveredPeer(peripheral: peripheral)

                var events: [Int] = []
                peer.rssi.subscribe(onNext: { events.append($0) }).disposed(by: disposeBag)

                expect(events) == [DiscoveredPeer.invalidRssi]
            }

            it("updates with a new value") {
                let peripheral = PeripheralMock()
                let rssi = -45
                let peer = DiscoveredPeer(peripheral: peripheral)
                peer.update(peripheral: peripheral, advertisementData: nil, rssi: rssi)

                var events: [Int] = []
                peer.rssi.subscribe(onNext: { events.append($0) }).disposed(by: disposeBag)

                let newRssi = -50

                peer.update(peripheral: peripheral, advertisementData: nil, rssi: newRssi)

                expect(events) == [rssi, newRssi]
            }
        }

        it("checks two peers for equality based on identifier") {
            let peripheral1 = PeripheralMock()
            peripheral1.identifier = UUID(uuidString: "00000000-0000-0000-0000-000000000001")!

            let peripheral2 = PeripheralMock()
            peripheral2.identifier = UUID(uuidString: "00000000-0000-0000-0000-000000000002")!

            let peripheral1Again = PeripheralMock()
            peripheral1Again.identifier = UUID(uuidString: "00000000-0000-0000-0000-000000000001")!

            let peer1 = DiscoveredPeer(peripheral: peripheral1)
            let peer2 = DiscoveredPeer(peripheral: peripheral2)
            let peer3 = DiscoveredPeer(peripheral: peripheral1Again)

            expect(peer1) != peer2
            expect(peer2) != peer3
            expect(peer1) == peer3
        }
    }
}
