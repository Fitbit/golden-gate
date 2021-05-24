//
//  CBUUID+ArbitraryDataSpec.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 13/11/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import BluetoothConnectionObjC
import BluetoothConnection
import CoreBluetooth
import Nimble
import Quick

final class CBUUIDArbitraryDataSpec: QuickSpec {
    override func spec() {
        context("create CBUUID from invalid data") {
            it("fails with empty data") {
                expect { try CBUUID(arbitraryData: Data()) }.to(throwError())
            }

            it("fails with data of illegal length") {
                expect { try CBUUID(arbitraryData: Data([1, 2, 3])) }.to(throwError())
            }

            it("fails with data of legal length, but illegal content") {
                expect { try CBUUID(arbitraryData: Data.init(repeating: 0, count: 32)) }.to(throwError())
            }
        }

        context("create CBUUID from valid data") {
            it("succeeds with data of legal length and legal content") {
                let uuid = UUID(uuidString: "e4b50e10-2585-11eb-adc1-0242ac120002")!.uuid
                let bytes = Mirror(reflecting: uuid).children.compactMap { $0.value as? UInt8 }

                expect { try CBUUID(arbitraryData: Data(bytes)) }.notTo(throwError())
            }
        }
    }
}
