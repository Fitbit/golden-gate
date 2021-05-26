//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CharacteristicTypeSpec.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 15/01/2021.
//

@testable import BluetoothConnection
import CoreBluetooth
import Nimble
import Quick
import RxSwift

final class CharacteristicTypeSpec: QuickSpec {
    override func spec() {
        var peripheralMock: PeripheralMock!
        var serviceMock: ServiceMock!

        var characteristic: CharacteristicMock!
        var disposeBag: DisposeBag!

        beforeEach {
            peripheralMock = PeripheralMock()
            serviceMock = ServiceMock(peripheral: peripheralMock)
            characteristic = CharacteristicMock(service: serviceMock)
            disposeBag = DisposeBag()
        }

        describe("readAndObserveValue()") {
            it("emits the current value if the characteristic is readable") {
                characteristic.properties = [.read]
                characteristic.value = Data([0x01])

                var data: Data?

                characteristic.readAndObserveValue()
                    .subscribe(onNext: { data = $0 })
                    .disposed(by: disposeBag)

                expect(data) == Data([0x01])
            }

            it("doesn't emit the current value if the characteristic is not readable") {
                characteristic.properties = []
                characteristic.value = Data([0x01])

                var data: Data?

                characteristic.readAndObserveValue()
                    .subscribe(onNext: { data = $0 })
                    .disposed(by: disposeBag)

                expect(data).to(beNil())
            }

            it("emits when the value changes") {
                characteristic.properties = []

                var data: Data?

                characteristic.readAndObserveValue()
                    .subscribe(onNext: { data = $0 })
                    .disposed(by: disposeBag)

                characteristic.value = Data([0x01])
                characteristic.didUpdateValueSubject.onNext(())

                expect(data) == Data([0x01])
            }
        }
    }
}
