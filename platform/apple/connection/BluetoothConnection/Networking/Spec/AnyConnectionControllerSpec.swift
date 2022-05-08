//
//  AnyConnectionControllerSpec.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 10.02.2022.
//  Copyright © 2022 Fitbit. All rights reserved.
//

import BluetoothConnection
import Foundation
import Nimble
import Quick
import RxSwift

final class AnyConnectionControllerSpec: QuickSpec {
    override func spec() {
        let descriptor = PeerDescriptor(identifier: UUID())
        let connection = MockConnection()

        var disposeBag: DisposeBag!
        var baseController: MockConnectionController!

        var controller: AnyConnectionController<MockConnection>!

        beforeEach {
            disposeBag = DisposeBag()
            baseController = MockConnectionController()
            controller = AnyConnectionController(connectionController: baseController)
        }

        it("forwards the descriptor") {
            var descriptors: [PeerDescriptor?] = []
            controller.descriptor
                .subscribe(onNext: { descriptors.append($0) })
                .disposed(by: disposeBag)

            baseController.descriptorRelay.accept(descriptor)
            baseController.descriptorRelay.accept(nil)

            expect(descriptors) == [nil, descriptor, nil] // Starts with nil
        }

        it("forwards the connection status") {
            var statuses: [ConnectionStatus<MockConnection>] = []
            controller.connectionStatus
                .subscribe(onNext: { statuses.append($0) })
                .disposed(by: disposeBag)

            baseController.connectionStatusSubject.onNext(.disconnected)
            baseController.connectionStatusSubject.onNext(.connecting)
            baseController.connectionStatusSubject.onNext(.connected(connection))
            baseController.connectionStatusSubject.onNext(.disconnected)

            expect(statuses) == [.disconnected, .connecting, .connected(connection), .disconnected]
        }

        it("forwards the connection error") {
            var errors: [Error?] = []
            controller.connectionError
                .subscribe(onNext: { errors.append($0) })
                .disposed(by: disposeBag)

            baseController.connectionErrorSubject.onNext(nil)
            baseController.connectionErrorSubject.onNext(TestError.someError)
            baseController.connectionErrorSubject.onNext(nil)

            var iterator = errors.makeIterator()
            expect(errors.count) == 3
            expect(iterator.next() as? Error).to(beNil())
            expect(iterator.next() as? Error).to(matchError(TestError.someError))
            expect(iterator.next() as? Error).to(beNil())
        }

        it("forwards the metrics") {
            var metrics: [ConnectionControllerMetricsEvent] = []
            controller.metrics
                .subscribe(onNext: { metrics.append($0) })
                .disposed(by: disposeBag)

            baseController.metricsSubject.onNext(.stateChangedToDisconnected)
            baseController.metricsSubject.onNext(.stateChangedToConnecting)
            baseController.metricsSubject.onNext(.stateChangedToConnected)

            expect(stringify(metrics)) == stringify(
                [
                    .stateChangedToDisconnected,
                    .stateChangedToConnecting,
                    .stateChangedToConnected
                ] as [ConnectionControllerMetricsEvent]
            )
        }

        it("forwards clearing and setting the descriptor") {
            controller.update(descriptor: descriptor)
            expect(baseController.lastDescriptor) == descriptor

            controller.clearDescriptor()
            expect(baseController.lastDescriptor).to(beNil())
        }

        it("forwards connect and disconnect requests") {
            controller.establishConnection(trigger: "ConnectTrigger")
            expect(baseController.lastConnectTrigger) == "ConnectTrigger"

            controller.disconnect(trigger: "DisconnectTrigger")
            expect(baseController.lastDisconnectTrigger) == "DisconnectTrigger"
        }

        it("forwards the description") {
            expect(String(describing: controller)) == String(describing: baseController)
        }
    }
}
