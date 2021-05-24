//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  HubSpec.swift
//  GoldenGateTests
//
//  Created by Emanuel Jarnea on 12/01/2021.
//

// swiftlint:disable function_body_length
// swiftlint:disable identifier_name

import BluetoothConnection
import CoreBluetooth
@testable import GoldenGate
import Nimble
import Quick
import RxSwift
import RxTest

final class HubSpec: QuickSpec {
    override func spec() {
        let descriptor = PeerDescriptor(identifier: UUID(uuidString: "6B66A7A9-C4F3-4C2A-811A-79EFB7B8A85F")!)
        let configuration = BluetoothConfiguration.test
        let unexpectedUuid = CBUUID()
        
        var peripheralMock: PeripheralMock!
        var serviceMock: ServiceMock!

        // Link service characteristics
        var rxCharacteristic: CharacteristicMock!
        var txCharacteristic: CharacteristicMock!

        // Link status service characteristics
        var bondSecureCharacteristic: CharacteristicMock!
        var currentConnectionConfigurationCharacteristic: CharacteristicMock!
        var currentConnectionStatusCharacteristic: CharacteristicMock!

        // Confirmation service characteristics
        var ephemeralCharacteristicPointerCharacteristic: CharacteristicMock!
        var ephemeralCharacteristic: CharacteristicMock!

        var scheduler: TestScheduler!
        var disposeBag: DisposeBag!

        var hub: Hub!

        func makeCharacteristic(_ uuid: CBUUID, value: Data? = nil) -> CharacteristicMock {
            CharacteristicMock(uuid: uuid, service: serviceMock, value: value)
        }

        func makeLinkService() -> BluetoothConnector.DiscoveredService {
            BluetoothConnector.DiscoveredService(
                uuid: configuration.linkService[0].serviceUUID,
                characteristics: .just(CharacteristicCollection([rxCharacteristic, txCharacteristic]))
            )
        }

        func makeLinkStatusService() -> BluetoothConnector.DiscoveredService {
            BluetoothConnector.DiscoveredService(
                uuid: configuration.linkStatusService.serviceUUID,
                characteristics: .just(CharacteristicCollection([bondSecureCharacteristic, currentConnectionConfigurationCharacteristic, currentConnectionStatusCharacteristic]))
            )
        }

        func makeConfirmationService() -> BluetoothConnector.DiscoveredService {
            BluetoothConnector.DiscoveredService(
                uuid: configuration.confirmationService.serviceUUID,
                characteristics: .just(CharacteristicCollection([ephemeralCharacteristicPointerCharacteristic, ephemeralCharacteristic]))
            )
        }

        func makeAllServices() -> [BluetoothConnector.DiscoveredService] {
            [makeLinkService(), makeLinkStatusService(), makeConfirmationService()]
        }

        beforeEach {
            peripheralMock = PeripheralMock()
            serviceMock = ServiceMock(peripheral: peripheralMock)

            rxCharacteristic = makeCharacteristic(configuration.linkService[0].rxUUID)
            txCharacteristic = makeCharacteristic(configuration.linkService[0].txUUID)

            bondSecureCharacteristic = makeCharacteristic(configuration.linkStatusService.bondSecureUUID)
            currentConnectionConfigurationCharacteristic = makeCharacteristic(configuration.linkStatusService.currentConnectionConfigurationUUID, value: Data(repeating: 1, count: 9))
            currentConnectionStatusCharacteristic = makeCharacteristic(configuration.linkStatusService.currentConnectionStatusUUID, value: Data(repeating: 1, count: 7))

            ephemeralCharacteristicPointerCharacteristic = makeCharacteristic(configuration.confirmationService.ephemeralCharacteristicPointerUUID, value: BluetoothConfiguration.ephemeralCharacteristicUuid.data)
            ephemeralCharacteristic = makeCharacteristic(BluetoothConfiguration.ephemeralCharacteristicUuid)

            scheduler = TestScheduler(initialClock: 0, simulateProcessingDelay: false)
            disposeBag = DisposeBag()

            hub = Hub(
                bluetoothScheduler: scheduler,
                protocolScheduler: scheduler,
                configuration: configuration
            )
        }

        describe("ensure expected characteristics") {
            it("finds all services and characteristics") {
                var connection: HubConnection?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onNext: { connection = $0 })
                    .disposed(by: disposeBag)

                expect(connection).notTo(beNil())
            }

            it("fails when the link service is missing") {
                var error: Error?

                hub.resolveConnection(services: [makeLinkStatusService(), makeConfirmationService()], descriptor: descriptor)
                    .subscribe(onError: { error = $0 })
                    .disposed(by: disposeBag)

                expect(error).to(matchError(ConnectionResolverError.couldNotResolveConnection))
            }

            it("fails when link service's rx characteristic is missing") {
                rxCharacteristic = makeCharacteristic(unexpectedUuid)

                var error: Error?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onError: { error = $0 })
                    .disposed(by: disposeBag)

                expect(error).to(matchError(ConnectionResolverError.couldNotResolveConnection))
            }

            it("fails when link service's tx characteristic is missing") {
                txCharacteristic = makeCharacteristic(unexpectedUuid)

                var error: Error?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onError: { error = $0 })
                    .disposed(by: disposeBag)

                expect(error).to(matchError(ConnectionResolverError.couldNotResolveConnection))
            }

            it("doesn't fail when the link status service is missing") {
                var connection: HubConnection?

                hub.resolveConnection(services: [makeLinkService(), makeConfirmationService()], descriptor: descriptor)
                    .subscribe(onNext: { connection = $0 })
                    .disposed(by: disposeBag)

                expect(connection).notTo(beNil())
            }

            it("fails when link status service's bondSecure characteristic is missing") {
                bondSecureCharacteristic = makeCharacteristic(unexpectedUuid)

                var error: Error?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onError: { error = $0 })
                    .disposed(by: disposeBag)

                expect(error).to(matchError(ConnectionResolverError.couldNotResolveConnection))
            }

            it("fails when link status service's currentConnectionConfiguration characteristic is missing") {
                currentConnectionConfigurationCharacteristic = makeCharacteristic(unexpectedUuid)

                var error: Error?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onError: { error = $0 })
                    .disposed(by: disposeBag)

                expect(error).to(matchError(ConnectionResolverError.couldNotResolveConnection))
            }

            it("fails when link status service's currentConnectionStatus characteristic is missing") {
                currentConnectionStatusCharacteristic = makeCharacteristic(unexpectedUuid)

                var error: Error?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onError: { error = $0 })
                    .disposed(by: disposeBag)

                expect(error).to(matchError(ConnectionResolverError.couldNotResolveConnection))
            }

            it("doesn't fail when the confirmation service is missing") {
                var connection: HubConnection?

                hub.resolveConnection(services: [makeLinkService(), makeLinkStatusService()], descriptor: descriptor)
                    .subscribe(onNext: { connection = $0 })
                    .disposed(by: disposeBag)

                expect(connection).notTo(beNil())
            }

            it("fails when confirmation service's ephemeralCharacteristicPointer characteristic is missing") {
                ephemeralCharacteristicPointerCharacteristic = makeCharacteristic(unexpectedUuid)

                var error: Error?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onError: { error = $0 })
                    .disposed(by: disposeBag)

                expect(error).to(matchError(ConnectionResolverError.couldNotResolveConnection))
            }
        }

        describe("create connection") {
            it("returns an observable that emits once and never completes") {
                var connection: HubConnection?
                var completed = false

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onNext: { connection = $0 }, onCompleted: { completed = true })
                    .disposed(by: disposeBag)

                expect(connection).notTo(beNil())
                expect(completed) == false
            }

            it("has the correct descriptor") {
                var connection: HubConnection?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onNext: { connection = $0 })
                    .disposed(by: disposeBag)

                expect(connection?.descriptor) == descriptor
            }

            it("emits ancs authorization status") {
                var connection: HubConnection?

                hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                    .subscribe(onNext: { connection = $0 })
                    .disposed(by: disposeBag)

                var statuses: [Bool] = []

                connection?.ancsAuthorized
                    .subscribe(onNext: { statuses.append($0) })
                    .disposed(by: disposeBag)

                peripheralMock.isANCSAuthorizedSubject.onNext(false)
                peripheralMock.isANCSAuthorizedSubject.onNext(true)

                expect(statuses) == [false, true]
            }

            context("network link") {
                it("fails connection if the rx characteristic is not writable") {
                    rxCharacteristic.properties = []

                    var error: Error?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onError: { error = $0 })
                        .disposed(by: disposeBag)

                    expect(error).to(matchError(PeripheralError.unexpectedCharacteristicProperties(configuration.linkService[0].rxUUID)))
                }

                it("has a data sink that writes to the rx characteristic") {
                    var connection: HubConnection?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onNext: { connection = $0 })
                        .disposed(by: disposeBag)

                    guard let networkLink = connection?.networkLink.value else {
                        fail("The connection must have a non nil network link")
                        return
                    }

                    let someData = Data([0x01])

                    expect { try networkLink.dataSink.put(someData) }.notTo(throwError())
                    expect(peripheralMock.writtenData) == someData
                }

                it("has a data source that reads from the tx characteristic") {
                    var connection: HubConnection?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onNext: { connection = $0 })
                        .disposed(by: disposeBag)

                    guard let networkLink = connection?.networkLink.value else {
                        fail("The connection must have a non nil network link")
                        return
                    }

                    let someData = Data([0x01])
                    var receivedData: Data?

                    networkLink.dataSource.asObservable()
                        .subscribe(onNext: { receivedData = $0.data })
                        .disposed(by: disposeBag)

                    txCharacteristic.value = someData
                    txCharacteristic.didUpdateValueSubject.onNext(()) ~~> scheduler

                    expect(receivedData) == someData
                }

                it("has an initial mtu read from the peripheral") {
                    let mtu = 128
                    peripheralMock.maxWriteValueLength = mtu

                    var connection: HubConnection?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onNext: { connection = $0 })
                        .disposed(by: disposeBag)

                    guard let networkLink = connection?.networkLink.value else {
                        fail("The connection must have a non nil network link")
                        return
                    }

                    expect(networkLink.mtu.value) == UInt(mtu)
                }
            }

            context("link status") {
                it("has an observable that emits connection configurations") {
                    var connection: HubConnection?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onNext: { connection = $0 })
                        .disposed(by: disposeBag)

                    expect(connection?.remoteConnectionConfiguration.value).notTo(beNil())
                }

                it("has an observable that emits connection statuses") {
                    var connection: HubConnection?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onNext: { connection = $0 })
                        .disposed(by: disposeBag)

                    expect(connection?.remoteConnectionStatus.value).notTo(beNil())
                }

                it("fails connection if currentConnectionConfiguration characteristic starts with invalid data") {
                    currentConnectionConfigurationCharacteristic.value = Data([0x01])

                    var error: Error?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onError: { error = $0 })
                        .disposed(by: disposeBag)

                    expect(error).to(matchError(HubError.illegalConnectionConfiguration))
                }

                it("fails connection if currentConnectionConfiguration characteristic updates value with invalid data") {
                    var error: Error?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onError: { error = $0 })
                        .disposed(by: disposeBag)

                    currentConnectionConfigurationCharacteristic.value = Data([0x01])
                    currentConnectionConfigurationCharacteristic.didUpdateValueSubject.onNext(())

                    expect(error).to(matchError(HubError.illegalConnectionConfiguration))
                }

                it("fails connection if currentConnectionStatus characteristic starts with invalid data") {
                    currentConnectionStatusCharacteristic.value = Data([0x01])

                    var error: Error?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onError: { error = $0 })
                        .disposed(by: disposeBag)

                    expect(error).to(matchError(HubError.illegalConnectionStatus))
                }

                it("fails connection if currentConnectionStatus characteristic updates value with invalid data") {
                    var error: Error?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onError: { error = $0 })
                        .disposed(by: disposeBag)

                    currentConnectionStatusCharacteristic.value = Data([0x01])
                    currentConnectionStatusCharacteristic.didUpdateValueSubject.onNext(())

                    expect(error).to(matchError(HubError.illegalConnectionStatus))
                }

                it("has a completable that accesses the bond characteristic when subscribed") {
                    var connection: HubConnection?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onNext: { connection = $0 })
                        .disposed(by: disposeBag)

                    connection?.accessBondSecureCharacteristic
                        .subscribe()
                        .disposed(by: disposeBag)

                    expect(bondSecureCharacteristic.didAccessSecurely) == true
                }
            }

            context("ephemeral characteristic") {
                it("doesn't fail connection if ephemeral characteristic exposes invalid data") {
                    ephemeralCharacteristicPointerCharacteristic.value = Data([0x01])

                    var connection: HubConnection?

                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe(onNext: { connection = $0 })
                        .disposed(by: disposeBag)

                    expect(connection).notTo(beNil())
                    expect(ephemeralCharacteristic.lastWrittenData).to(beNil())
                }

                it("writes to the ephemeral characteristic") {
                    hub.resolveConnection(services: makeAllServices(), descriptor: descriptor)
                        .subscribe()
                        .disposed(by: disposeBag)

                    expect(ephemeralCharacteristic.lastWrittenData) == Data([0x00])
                    expect(ephemeralCharacteristic.lastWriteType) == .withResponse
                }
            }
        }
    }
}

private extension BluetoothConfiguration {
    static let test = BluetoothConfiguration(
        name: "TEST-CONFIG",
        linkStatusService: BluetoothConfiguration.LinkStatusServiceConfiguration(
            serviceUUID: CBUUID(string: "0001"),
            currentConnectionConfigurationUUID: CBUUID(string: "0002"),
            currentConnectionStatusUUID: CBUUID(string: "0003"),
            bondSecureUUID: CBUUID(string: "0004")
        ),
        linkService: [
            BluetoothConfiguration.LinkServiceConfiguration(
                serviceUUID: CBUUID(string: "0005"),
                rxUUID: CBUUID(string: "0006"),
                txUUID: CBUUID(string: "0007")
            )
        ],
        linkConfigurationService: BluetoothConfiguration.LinkConfigurationServiceConfiguration(
            serviceUUID: CBUUID(string: "0008"),
            preferredConnectionConfigurationUUID: CBUUID(string: "0009"),
            preferredConnectionModeUUID: CBUUID(string: "000A"),
            generalPurposeCommandUUID: CBUUID(string: "000B")
        ),
        deviceInfoService: BluetoothConfiguration.DeviceInfoServiceConfiguration(
            serviceUUID: CBUUID(string: "000C"),
            modelNumberUUID: CBUUID(string: "000D"),
            serialNumberUUID: CBUUID(string: "000E"),
            firmwareRevisionUUID: CBUUID(string: "000F"),
            hardwareRevisionUUID: CBUUID(string: "0010"),
            softwareRevisionUUID: CBUUID(string: "0011")
        ),
        confirmationService: BluetoothConfiguration.ConfirmationServiceConfiguration(
            serviceUUID: CBUUID(string: "0014"),
            ephemeralCharacteristicPointerUUID: CBUUID(string: "0015")
        )
    )

    static let ephemeralCharacteristicUuid = CBUUID(string: "0016")
}
