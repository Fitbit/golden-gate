//
//  PeerDiscovererSpec.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 30.06.2021.
//  Copyright © 2021 Fitbit. All rights reserved.
//

import BluetoothConnection
import CoreBluetooth
import Nimble
import Quick
import RxBluetoothKit
import RxRelay
import RxSwift
import RxTest

final class PeerDiscovererSpec: QuickSpec {
    override func spec() {
        var centralManager: CentralManagerMock!
        var discoverer: PeerDiscoverer!
        var scheduler: TestScheduler!
        var disposeBag: DisposeBag!

        beforeEach {
            centralManager = CentralManagerMock()
            scheduler = TestScheduler(initialClock: 0, simulateProcessingDelay: false)
            discoverer = PeerDiscoverer(centralManager: centralManager, bluetoothScheduler: scheduler)
            disposeBag = DisposeBag()

            centralManager.bluetoothState.onNext(.poweredOn)
        }

        describe("discovered peers") {
            it("uses the specified service filters for retrieving connected peers and scanning") {
                let scannedServices = [CBUUID()]
                let connectedServices = [CBUUID()]
                discoverer.discoveredPeers(scannedServices: scannedServices, connectedServices: connectedServices, peerFilter: nil)
                    .subscribe()
                    .disposed(by: disposeBag)

                expect(centralManager.scannedServiceUUIDsFilter) == scannedServices
                expect(centralManager.connectedServiceUUIDsFilter) == connectedServices
            }

            it("emits unique peers") {
                let peripheral1 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                let peripheral2 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)

                centralManager.connectedPeripherals = [peripheral1]

                var peerIdentifiers: [UUID] = []
                discoverer.discoveredPeers(scannedServices: [], connectedServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                peripheral1.connectablePeripheral.onNext(())
                peripheral1.discoveredServices.onNext([])

                expect(peerIdentifiers.count) == 1

                // Ignored, already connected
                centralManager.scannedPeripherals.onNext(ScannedPeripheralMock(peripheral: peripheral1))
                expect(peerIdentifiers.count) == 1

                centralManager.scannedPeripherals.onNext(ScannedPeripheralMock(peripheral: peripheral2))
                expect(peerIdentifiers.count) == 2

                // Ignored, already scanned
                centralManager.connectedPeripherals = [peripheral2]
                scheduler.advance()

                peripheral2.connectablePeripheral.onNext(())
                peripheral2.discoveredServices.onNext([])
                expect(peerIdentifiers.count) == 2
            }

            it("emits only peers matching filter") {
                let peripheral1 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                let peripheral2 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                let peripheral3 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000003")!)
                let peripheral4 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000004")!)

                func peerFilter(_ identifier: UUID) -> Bool {
                    identifier == peripheral3.identifier || identifier == peripheral4.identifier
                }

                centralManager.connectedPeripherals = [peripheral1]

                var peerIdentifiers: [UUID] = []
                discoverer.discoveredPeers(scannedServices: [], connectedServices: [], peerFilter: peerFilter)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                peripheral1.connectablePeripheral.onNext(())
                peripheral1.discoveredServices.onNext([])

                centralManager.scannedPeripherals.onNext(ScannedPeripheralMock(peripheral: peripheral2))
                centralManager.scannedPeripherals.onNext(ScannedPeripheralMock(peripheral: peripheral3))

                centralManager.connectedPeripherals = [peripheral4]
                scheduler.advance()
                peripheral4.connectablePeripheral.onNext(())
                peripheral4.discoveredServices.onNext([])

                expect(peerIdentifiers) == [UUID(uuidString: "00000000-0000-0000-0000-000000000003")!, UUID(uuidString: "00000000-0000-0000-0000-000000000004")!]
            }
        }

        describe("connected peers") {
            it("emits unique peers") {
                let peripheral1 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                let peripheral2 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!) // Ignored
                centralManager.connectedPeripherals = [peripheral1, peripheral2]

                var peerIdentifiers: [UUID] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                peripheral1.connectablePeripheral.onNext(())
                peripheral1.discoveredServices.onNext([])
                peripheral2.connectablePeripheral.onNext(())
                peripheral2.discoveredServices.onNext([])

                let peripheral3 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!) // Ignored
                let peripheral4 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                centralManager.connectedPeripherals = [peripheral3, peripheral4]

                scheduler.advance()

                peripheral3.connectablePeripheral.onNext(())
                peripheral3.discoveredServices.onNext([])
                peripheral4.connectablePeripheral.onNext(())
                peripheral4.discoveredServices.onNext([])

                expect(peerIdentifiers.count) == 2
            }

            it("emits only peers matching filter") {
                func peerFilter(_ identifier: UUID) -> Bool {
                    identifier == UUID(uuidString: "00000000-0000-0000-0000-000000000002")!
                }

                let peripheral1 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                let peripheral2 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                centralManager.connectedPeripherals = [peripheral1, peripheral2]

                var peerIdentifiers: [UUID] = []
                discoverer.connectedPeers(withServices: [], peerFilter: peerFilter)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                peripheral1.connectablePeripheral.onNext(())
                peripheral1.discoveredServices.onNext([])
                peripheral2.connectablePeripheral.onNext(())
                peripheral2.discoveredServices.onNext([])

                expect(peerIdentifiers) == [UUID(uuidString: "00000000-0000-0000-0000-000000000002")!]
            }

            it("establishes connection with a discovered peer") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .subscribe()
                    .disposed(by: disposeBag)

                expect(peripheral.didEstablishConnection) == true
            }

            it("ignores a peer if establishing connection fails") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                var peerIdentifiers: [UUID] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                peripheral.connectablePeripheral.onError(TestError.some)
                peripheral.discoveredServices.onNext([])

                expect(peerIdentifiers) == []
            }

            it("discovers the services of a discovered peer") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .subscribe()
                    .disposed(by: disposeBag)

                peripheral.connectablePeripheral.onNext(())

                expect(peripheral.didDiscoverServices) == true
                expect(peripheral.servicesToDiscover) == []
            }

            it("ignores a peer if service discovery fails") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                var peerIdentifiers: [UUID] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                peripheral.connectablePeripheral.onNext(())
                peripheral.discoveredServices.onError(TestError.some)

                expect(peerIdentifiers) == []
            }

            it("updates rssi for each peer") {
                let peripheral1 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                let peripheral2 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                centralManager.connectedPeripherals = [peripheral1, peripheral2]

                var rssis: [Int] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .flatMap { $0.rssi }
                    .subscribe(onNext: { rssis.append($0) })
                    .disposed(by: disposeBag)

                peripheral1.connectablePeripheral.onNext(())
                peripheral1.discoveredServices.onNext([])
                peripheral2.connectablePeripheral.onNext(())
                peripheral2.discoveredServices.onNext([])

                peripheral1.rssi.onNext(1)
                peripheral2.rssi.onNext(2)

                scheduler.advance(by: 1) // Advance past rssiUpdateInterval

                peripheral1.rssi.onNext(3)
                peripheral2.rssi.onNext(4)

                // The initial RSSI value for connected peers is invalid
                expect(rssis) == [DiscoveredPeer.invalidRssi, DiscoveredPeer.invalidRssi, 1, 2, 3, 4]
            }

            it("skips rssi update if rssi reading fails") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                var rssis: [Int] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .flatMap { $0.rssi }
                    .subscribe(onNext: { rssis.append($0) })
                    .disposed(by: disposeBag)

                peripheral.connectablePeripheral.onNext(())
                peripheral.discoveredServices.onNext([])

                peripheral.rssi.onError(TestError.some)
                peripheral.rssi = PublishSubject() // Reset error and allow for other rssi updates

                scheduler.advance(by: 1) // Advance past rssiUpdateInterval

                peripheral.rssi.onNext(2)

                // The initial RSSI value for connected peers is invalid
                expect(rssis) == [DiscoveredPeer.invalidRssi, 2]
            }

            it("polls rssi only once at a time for the same peer") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                let samePeripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                var rssis: [Int] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .flatMap { $0.rssi }
                    .subscribe(onNext: { rssis.append($0) })
                    .disposed(by: disposeBag)

                peripheral.connectablePeripheral.onNext(())
                peripheral.discoveredServices.onNext([])

                peripheral.rssi.onNext(-50)

                centralManager.connectedPeripherals = [samePeripheral]

                scheduler.advance(by: 2) // Advance past connectedPeripheralsQueryInterval - the same connected peripheral is retrieved again

                samePeripheral.connectablePeripheral.onNext(())
                samePeripheral.discoveredServices.onNext([])

                peripheral.rssi.onNext(-40)
                samePeripheral.rssi.onNext(-40)

                // The initial RSSI value for connected peers is invalid
                expect(rssis) == [DiscoveredPeer.invalidRssi, -50, -40]
            }

            it("stops rssi polling when a peer disconnects") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                var rssis: [Int] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .flatMap { $0.rssi }
                    .subscribe(onNext: { rssis.append($0) })
                    .disposed(by: disposeBag)

                peripheral.connectablePeripheral.onNext(())
                peripheral.discoveredServices.onNext([])

                peripheral.rssi.onNext(-50)

                peripheral.connectablePeripheral.onError(TestError.some)

                scheduler.advance(by: 1) // Advance past rssiUpdateInterval

                // This update should not go through, because the peer is disconnected
                peripheral.rssi.onNext(-40)

                // The initial RSSI value for connected peers is invalid
                expect(rssis) == [DiscoveredPeer.invalidRssi, -50]
            }


            it("restarts rssi polling when a peer reconnects") {
                let peripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                let samePeripheral = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral]

                var rssis: [Int] = []
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .flatMap { $0.rssi }
                    .subscribe(onNext: { rssis.append($0) })
                    .disposed(by: disposeBag)

                peripheral.connectablePeripheral.onNext(())
                peripheral.discoveredServices.onNext([])

                peripheral.rssi.onNext(-50)

                peripheral.connectablePeripheral.onError(TestError.some)

                scheduler.advance(by: 1) // Advance past rssiUpdateInterval

                // This update should not go through, because the peer is disconnected
                peripheral.rssi.onNext(-40)

                centralManager.connectedPeripherals = [samePeripheral]
                scheduler.advance(by: 2) // Advance past connectedPeripheralsQueryInterval - the same connected peripheral is retrieved again

                samePeripheral.connectablePeripheral.onNext(())
                samePeripheral.discoveredServices.onNext([])

                samePeripheral.rssi.onNext(-30)

                // The initial RSSI value for connected peers is invalid
                expect(rssis) == [DiscoveredPeer.invalidRssi, -50, -30]
            }

            it("errors out if bluetooth is not powered on") {
                let peripheral1 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                centralManager.connectedPeripherals = [peripheral1]

                var peerIdentifiers: [UUID] = []
                var error: Error?
                discoverer.connectedPeers(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) }, onError: { error = $0 })
                    .disposed(by: disposeBag)

                peripheral1.connectablePeripheral.onNext(())
                peripheral1.discoveredServices.onNext([])

                centralManager.bluetoothState.onNext(.poweredOff)

                let peripheral2 = PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                centralManager.connectedPeripherals = [peripheral2]

                scheduler.advance() // Advance to retrieve connected peripherals again

                peripheral2.connectablePeripheral.onNext(())
                peripheral2.discoveredServices.onNext([])

                expect(peerIdentifiers) == [UUID(uuidString: "00000000-0000-0000-0000-000000000001")!]
                expect(error).to(matchError(BluetoothError.bluetoothPoweredOff))
            }
        }

        describe("scanned peers") {
            it("emits unique peers") {
                var peerIdentifiers: [UUID] = []
                discoverer.scannedPeers(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!) // Ignored
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                    )
                )

                expect(peerIdentifiers.count) == 2
            }

            it("emits only peers matching filter") {
                func peerFilter(_ identifier: UUID) -> Bool {
                    identifier == UUID(uuidString: "00000000-0000-0000-0000-000000000002")!
                }

                var peerIdentifiers: [UUID] = []
                discoverer.scannedPeers(withServices: [], peerFilter: peerFilter)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) })
                    .disposed(by: disposeBag)

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                    )
                )

                expect(peerIdentifiers) == [UUID(uuidString: "00000000-0000-0000-0000-000000000002")!]
            }

            it("updates rssi for each peer") {
                var rssis: [Int] = []
                discoverer.scannedPeers(withServices: [], peerFilter: nil)
                    .flatMap { $0.rssi }
                    .subscribe(onNext: { rssis.append($0) })
                    .disposed(by: disposeBag)

                // The rssi updates are throttled per peer to avoid spamming

                // Emit the first updates per time window for each peer
                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!),
                        rssi: 1
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!),
                        rssi: 2
                    )
                )

                // Ignore subsequents updates during the same time window for each peer
                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!),
                        rssi: 3
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!),
                        rssi: 4
                    )
                )


                // Emit the last updates per time window for each peer
                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!),
                        rssi: 5
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!),
                        rssi: 6
                    )
                )

                scheduler.advance() // Go to the next throttle time window

                // Emit the first updates per a new time window for each peer
                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!),
                        rssi: 7
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!),
                        rssi: 8
                    )
                )

                expect(rssis) == [1, 2, 5, 6, 7, 8]
            }

            it("errors out if bluetooth is not powered on") {
                var peerIdentifiers: [UUID] = []
                var error: Error?
                discoverer.scannedPeers(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifiers.append($0.identifier) }, onError: { error = $0 })
                    .disposed(by: disposeBag)

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                    )
                )

                centralManager.bluetoothState.onNext(.poweredOff)

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                    )
                )

                expect(peerIdentifiers) == [UUID(uuidString: "00000000-0000-0000-0000-000000000001")!]
                expect(error).to(matchError(BluetoothError.bluetoothPoweredOff))
            }
        }

        describe("peer list") {
            it("emits a list of discovered peers when the list is updated") {
                var peerIdentifierLists: [[UUID]?] = []
                discoverer.peerList(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifierLists.append($0.map { $0.map(\.identifier) }) })
                    .disposed(by: disposeBag)

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                    )
                )

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                    )
                )

                expect(peerIdentifierLists) == [
                    [],
                    [UUID(uuidString: "00000000-0000-0000-0000-000000000001")!],
                    [UUID(uuidString: "00000000-0000-0000-0000-000000000001")!, UUID(uuidString: "00000000-0000-0000-0000-000000000002")!]
                ]
            }

            it("resets the list of discovered peers when bluetooth state is not powered on") {
                var peerIdentifierLists: [[UUID]?] = []
                discoverer.peerList(withServices: [], peerFilter: nil)
                    .subscribe(onNext: { peerIdentifierLists.append($0.map { $0.map(\.identifier) }) })
                    .disposed(by: disposeBag)

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000001")!)
                    )
                )

                centralManager.bluetoothState.onNext(.poweredOff)
                centralManager.bluetoothState.onNext(.poweredOn)

                centralManager.scannedPeripherals.onNext(
                    ScannedPeripheralMock(
                        peripheral: PeripheralMock(identifier: UUID(uuidString: "00000000-0000-0000-0000-000000000002")!)
                    )
                )

                expect(peerIdentifierLists) == [
                    [],
                    [UUID(uuidString: "00000000-0000-0000-0000-000000000001")!],
                    nil,
                    [],
                    [UUID(uuidString: "00000000-0000-0000-0000-000000000002")!]
                ]
            }
        }
    }
}

private extension PeerDiscovererSpec {
    enum TestError: Error {
        case some
    }
}

private final class DiscoveredPeerMock: DiscoveredPeerType {
    var identifier: UUID
    var name = ""
    var serviceUuids: [CBUUID]?
    var advertisementData: AdvertisementData?
    let rssi = BehaviorRelay<Int>(value: 0)

    init(identifier: UUID) {
        self.identifier = identifier
    }
}
