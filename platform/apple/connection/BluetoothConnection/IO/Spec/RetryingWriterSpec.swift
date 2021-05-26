//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RetryingWriterSpec.swift
//  GoldenGateTests
//
//  Created by Emanuel Jarnea on 27/02/2019.
//

@testable import BluetoothConnection
import Nimble
import Quick
import RxSwift

// swiftlint:disable function_body_length identifier_name

final class RetryingWriterSpec: QuickSpec {
    fileprivate typealias Destination = Int

    override func spec() {
        let data_1 = "1".data(using: .utf8)!
        let data_2 = "2".data(using: .utf8)!
        let data_3 = "3".data(using: .utf8)!
        let data_4 = "4".data(using: .utf8)!

        let firstTarget: Destination = 1
        let secondTarget: Destination = 2

        var mockWriter: MockWriter!
        var retrySubject: PublishSubject<Void>!
        var retryingWriter: RetryingWriter<Destination>!

        beforeEach {
            mockWriter = MockWriter()
            retrySubject = PublishSubject()

            retryingWriter = RetryingWriter(
                write: {
                    mockWriter.write(data: $0, target: $1)
                },
                retryTrigger: retrySubject.asObservable(),
                scheduler: MainScheduler.instance
            )
        }

        it("writes one update to first target") {
            mockWriter.stagedResult = .success(())
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))

            expect(mockWriter.writes.count) == 1
            expect(mockWriter.writes[0].target) == firstTarget
        }

        it("writes one update to second target") {
            mockWriter.stagedResult = .success(())
            retryingWriter.writeRelay.accept((data: data_2, target: secondTarget))

            expect(mockWriter.writes.count) == 1
            expect(mockWriter.writes[0].target) == secondTarget
        }

        it("writes one update to each target") {
            mockWriter.stagedResult = .success(())
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))
            retryingWriter.writeRelay.accept((data: data_2, target: secondTarget))

            expect(mockWriter.writes.count) == 2
            expect(mockWriter.writes[0].data) == data_1
            expect(mockWriter.writes[0].target) == firstTarget
            expect(mockWriter.writes[1].data) == data_2
            expect(mockWriter.writes[1].target) == secondTarget
        }

        it("writes two interleaved updates to each target") {
            mockWriter.stagedResult = .success(())
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))
            retryingWriter.writeRelay.accept((data: data_1, target: secondTarget))
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))
            retryingWriter.writeRelay.accept((data: data_1, target: secondTarget))

            expect(mockWriter.writes.count) == 4
            expect(mockWriter.writes[0].target) == firstTarget
            expect(mockWriter.writes[1].target) == secondTarget
            expect(mockWriter.writes[2].target) == firstTarget
            expect(mockWriter.writes[3].target) == secondTarget
        }

        it("waits for `retry` then writes one update") {
            mockWriter.stagedResult = .failure(.retriable)
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))

            mockWriter.stagedResult = .success(())
            retrySubject.onNext(())

            expect(mockWriter.writes.count) == 1
            expect(mockWriter.writes[0].target) == firstTarget
        }

        it("waits for `retry`, writes one update, then writes another update") {
            mockWriter.stagedResult = .failure(.retriable)
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))

            mockWriter.stagedResult = .success(())
            retrySubject.onNext(())

            retryingWriter.writeRelay.accept((data: data_2, target: firstTarget))

            expect(mockWriter.writes.count) == 2
            expect(mockWriter.writes[0].data) == data_1
            expect(mockWriter.writes[0].target) == firstTarget
            expect(mockWriter.writes[1].data) == data_2
            expect(mockWriter.writes[1].target) == firstTarget
        }

        it("waits for `retry` then writes one update for each target") {
            mockWriter.stagedResult = .failure(.retriable)
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))
            retryingWriter.writeRelay.accept((data: data_2, target: secondTarget))

            mockWriter.stagedResult = .success(())
            retrySubject.onNext(())

            expect(mockWriter.writes.count) == 2
            expect(mockWriter.writes[0].data) == data_1
            expect(mockWriter.writes[0].target) == firstTarget
            expect(mockWriter.writes[1].data) == data_2
            expect(mockWriter.writes[1].target) == secondTarget
        }

        it("doesn't retry unretriable updates") {
            mockWriter.stagedResult = .failure(.unretriable)
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))

            mockWriter.stagedResult = .success(())
            retrySubject.onNext(())

            expect(mockWriter.writes.count) == 0
        }

        it("doesn't write same update twice") {
            mockWriter.stagedResult = .failure(.retriable)
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))

            mockWriter.stagedResult = .success(())
            retrySubject.onNext(())
            retrySubject.onNext(())

            expect(mockWriter.writes.count) == 1
            expect(mockWriter.writes[0].target) == firstTarget
        }

        it("cancels a pending update for one target") {
            mockWriter.stagedResult = .failure(.retriable)
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))
            retryingWriter.writeRelay.accept((data: data_2, target: secondTarget))

            retryingWriter.writeRelay.accept((data: data_3, target: firstTarget))

            mockWriter.stagedResult = .success(())
            retrySubject.onNext(())

            expect(mockWriter.writes.count) == 2
            expect(mockWriter.writes[0].data) == data_3
            expect(mockWriter.writes[0].target) == firstTarget
            expect(mockWriter.writes[1].data) == data_2
            expect(mockWriter.writes[1].target) == secondTarget
        }

        it("cancels a pending update for each target") {
            mockWriter.stagedResult = .failure(.retriable)
            retryingWriter.writeRelay.accept((data: data_1, target: firstTarget))
            retryingWriter.writeRelay.accept((data: data_2, target: secondTarget))

            retryingWriter.writeRelay.accept((data: data_3, target: firstTarget))
            retryingWriter.writeRelay.accept((data: data_4, target: secondTarget))

            mockWriter.stagedResult = .success(())
            retrySubject.onNext(())

            expect(mockWriter.writes.count) == 2
            expect(mockWriter.writes[0].data) == data_3
            expect(mockWriter.writes[0].target) == firstTarget
            expect(mockWriter.writes[1].data) == data_4
            expect(mockWriter.writes[1].target) == secondTarget
        }
    }
}

private extension RetryingWriterSpec {
    class MockWriter {
        var stagedResult: Result<Void, RetryingWriterError>!

        private(set) var writes: [(data: Data, target: Destination)] = []

        func write(data: Data, target: Destination) -> Result<Void, RetryingWriterError> {
            if stagedResult.isSuccess {
                writes.append((data, target))
            }

            return stagedResult
        }
    }
}

private extension Result {
    var isSuccess: Bool {
        switch self {
        case .success:
            return true
        case .failure:
            return false
        }
    }
}
