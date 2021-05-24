//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BluetoothConnectorSpec.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 19/11/2020.
//

import BluetoothConnection
import Nimble
import Quick
import RxSwift
import RxTest

final class BluetoothConnectorSpec: QuickSpec {
    override func spec() {
        let descriptor = PeerDescriptor(identifier: UUID())
        var centralManager: CentralManagerMock!
        var connector: BluetoothConnector!
        var scheduler: TestScheduler!
        var disposeBag: DisposeBag!

        beforeEach {
            scheduler = TestScheduler(initialClock: 0, simulateProcessingDelay: false)
            centralManager = CentralManagerMock()
            connector = BluetoothConnector(centralManager: centralManager, scheduler: scheduler)
            disposeBag = DisposeBag()
        }

        describe("establish connection") {
            it("reflects the status dictated by the central manager") {
                var events: [Event<ConnectionStatus<[BluetoothConnector.DiscoveredService]>>] = []

                connector.establishConnection(descriptor: descriptor).subscribe { events.append($0) }.disposed(by: disposeBag)

                centralManager.connectionStatus.onNext(.disconnected)
                centralManager.connectionStatus.onNext(.connecting)

                let peripheral = PeripheralMock()
                centralManager.connectionStatus.onNext(.connected(peripheral))
                peripheral.discoveredServices.onNext([])

                centralManager.connectionStatus.onError(TestError.someError)

                expect(events) == [.next(.disconnected), .next(.connecting), .next(.connected([])), .error(TestError.someError)]
            }

            it("discovers all services") {
                connector.establishConnection(descriptor: descriptor).subscribe().disposed(by: disposeBag)

                let peripheral = PeripheralMock()
                centralManager.connectionStatus.onNext(.connected(peripheral))

                expect(peripheral.didDiscoverAllServices) == true
            }

            it("fails when service discovery fails") {
                var events: [Event<ConnectionStatus<[BluetoothConnector.DiscoveredService]>>] = []

                connector.establishConnection(descriptor: descriptor).subscribe { events.append($0) }.disposed(by: disposeBag)

                let peripheral = PeripheralMock()
                centralManager.connectionStatus.onNext(.connected(peripheral))

                peripheral.discoveredServices.onError(TestError.someError)

                expect(events) == [.error(TestError.someError)]
            }

            it("fails when services change") {
                var events: [Event<ConnectionStatus<[BluetoothConnector.DiscoveredService]>>] = []

                connector.establishConnection(descriptor: descriptor).subscribe { events.append($0) }.disposed(by: disposeBag)

                let peripheral = PeripheralMock()
                centralManager.connectionStatus.onNext(.connected(peripheral))
                peripheral.discoveredServices.onNext([])

                peripheral.servicesModification.onNext([])

                expect(events) == [.next(.connected([])), .error(PeripheralError.servicesChanged)]
            }

            it("fails when service change indication fails") {
                var events: [Event<ConnectionStatus<[BluetoothConnector.DiscoveredService]>>] = []

                connector.establishConnection(descriptor: descriptor).subscribe { events.append($0) }.disposed(by: disposeBag)

                let peripheral = PeripheralMock()
                centralManager.connectionStatus.onNext(.connected(peripheral))
                peripheral.discoveredServices.onNext([])

                peripheral.servicesModification.onError(TestError.someError)

                expect(events) == [.next(.connected([])), .error(TestError.someError)]
            }

            it("emits the discovered services correctly") {
                var discoveredServices: [BluetoothConnector.DiscoveredService] = []

                connector.establishConnection(descriptor: descriptor)
                    .compactMap(\.connection)
                    .subscribe(onNext: { discoveredServices = $0 })
                    .disposed(by: disposeBag)

                let peripheral = PeripheralMock()
                centralManager.connectionStatus.onNext(.connected(peripheral))
                let services = [ServiceMock(peripheral: peripheral), ServiceMock(peripheral: peripheral)]
                peripheral.discoveredServices.onNext(services)

                expect(discoveredServices).to(haveCount(2))
                expect(discoveredServices[0].uuid) == services[0].uuid
                expect(discoveredServices[1].uuid) == services[1].uuid
            }

            describe("a discovered service") {
                it("discovers all characteristics") {
                    var discoveredServices: [BluetoothConnector.DiscoveredService] = []

                    connector.establishConnection(descriptor: descriptor)
                        .compactMap(\.connection)
                        .subscribe(onNext: { discoveredServices = $0 })
                        .disposed(by: disposeBag)

                    let peripheral = PeripheralMock()
                    centralManager.connectionStatus.onNext(.connected(peripheral))
                    let service = ServiceMock(peripheral: peripheral)
                    peripheral.discoveredServices.onNext([service])

                    discoveredServices[0].characteristics.subscribe().disposed(by: disposeBag)

                    expect(service.didDiscoverAllCharacteristics) == true
                }

                it("fails the characteristics when characteristic discovery fails") {
                    var discoveredServices: [BluetoothConnector.DiscoveredService] = []

                    connector.establishConnection(descriptor: descriptor)
                       .compactMap(\.connection)
                       .subscribe(onNext: { discoveredServices = $0 })
                       .disposed(by: disposeBag)

                    let peripheral = PeripheralMock()
                    centralManager.connectionStatus.onNext(.connected(peripheral))
                    let service = ServiceMock(peripheral: peripheral)
                    peripheral.discoveredServices.onNext([service])

                    var event: SingleEvent<CharacteristicCollection>?

                    discoveredServices[0].characteristics
                        .subscribe { event = $0 }
                        .disposed(by: disposeBag)

                    service.discoveredCharacteristics.onError(TestError.someError)

                    expect(event?.error).to(matchError(TestError.someError))
                }

                it("emits the discovered characteristics correctly") {
                    var discoveredServices: [BluetoothConnector.DiscoveredService] = []

                    connector.establishConnection(descriptor: descriptor)
                       .compactMap(\.connection)
                       .subscribe(onNext: { discoveredServices = $0 })
                       .disposed(by: disposeBag)

                    let peripheral = PeripheralMock()
                    centralManager.connectionStatus.onNext(.connected(peripheral))
                    let service = ServiceMock(peripheral: peripheral)
                    peripheral.discoveredServices.onNext([service])

                    var characteristicCollection = CharacteristicCollection()

                    discoveredServices[0].characteristics
                        .subscribe(onSuccess: { characteristicCollection = $0 })
                        .disposed(by: disposeBag)

                    let characteristics = [CharacteristicMock(service: service), CharacteristicMock(service: service)]
                    service.discoveredCharacteristics.onNext(characteristics)

                    expect(characteristicCollection.optional(characteristics[0].uuid)).notTo(beNil())
                    expect(characteristicCollection.optional(characteristics[1].uuid)).notTo(beNil())
                }
            }
        }

        describe("metrics") {
            typealias MetricsEvent = BluetoothConnectorMetricsEvent

            it("emits metrics events in the correct order") {
                var events: [MetricsEvent] = []

                connector.metrics.subscribe(onNext: { events.append($0) }).disposed(by: disposeBag)

                var discoveredServices: [BluetoothConnector.DiscoveredService] = []

                connector.establishConnection(descriptor: descriptor)
                   .compactMap(\.connection)
                   .subscribe(onNext: { discoveredServices = $0 })
                   .disposed(by: disposeBag)

                centralManager.connectionStatus.onNext(.connecting)

                let peripheral = PeripheralMock()
                centralManager.connectionStatus.onNext(.connected(peripheral))

                let service = ServiceMock(peripheral: peripheral)
                peripheral.discoveredServices.onNext([service])

                discoveredServices.first?.characteristics
                    .subscribe()
                    .disposed(by: disposeBag)

                let characteristic = CharacteristicMock(service: service)
                service.discoveredCharacteristics.onNext([characteristic])

                centralManager.connectionStatus.onNext(.disconnected)

                expect(events) == [
                    MetricsEvent.stateChangedToConnecting,
                    .stateChangedToConnected,
                    .stateChangedToDiscoveredServices,
                    .stateChangedToDiscoveredCharacteristics,
                    .stateChangedToDisconnected
                ]
            }
        }
    }
}

private extension BluetoothConnectorSpec {
    enum TestError: Error {
        case someError
    }
}

private extension SingleEvent {
    var error: Error? {
        switch self {
        case .success:
            return nil
        case .error(let error):
            return error
        }
    }
}
