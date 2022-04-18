//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RetryingWriter.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 27/02/2019.
//

import Foundation
import RxCocoa
import RxSwift

/// Writer that writes using a provided `write` closure and retries failed attempts when an input
/// `retryTrigger` observable emits. A write will cancel pending retries for previously failed
/// write attempts on the same target.
final class RetryingWriter<Destination: Hashable> {
    let writeRelay = PublishRelay<(data: Data, target: Destination)>()
    private let disposeBag = DisposeBag()

    /// - Parameters:
    ///   - write: Closure invoked to do the actual data writing. It must return the write
    ///         result (success or failure). Failures can be either retriable or unretriable.
    ///   - retryTrigger: Observable that emits when writes are allowed again. Any pending writes
    ///         will be retried.
    ///   - scheduler: Scheduler used to perform writes on.
    init(write: @escaping (Data, Destination) -> Result<Void, RetryingWriterError>, retryTrigger: Observable<Void>, scheduler: SchedulerType) {
        self.writeRelay.asObservable()
            .observe(on: scheduler)
            .groupBy { $0.target }
            .flatMap { group in
                // Attempt to write new values as they're received, cancelling pending retries
                // for previously failed attempts.
                group.flatMapLatest { item -> Completable in
                    // Turn the `write` closure into a Completable.
                    let written = Completable.create { observer in
                        do {
                            try write(item.data, item.target).get()
                            observer(.completed)
                        } catch {
                            observer(.error(error))
                        }
                        return Disposables.create()
                    }

                    return written.asObservable()
                        .catch { error in
                            // Simply ignore unretriable failures
                            if case RetryingWriterError.unretriable = error {
                                return .empty()
                            }
                            throw error
                        }
                        .retry { _ in retryTrigger.observe(on: scheduler) }
                        .asCompletable()
                }
            }
            .subscribe()
            .disposed(by: disposeBag)
    }
}

enum RetryingWriterError: Error {
    case retriable
    case unretriable
}
