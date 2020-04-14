//
//  RetryingWriter.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 27/02/2019.
//  Copyright © 2019 Fitbit. All rights reserved.
//

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
    ///         status (success or failure).
    ///   - retryTrigger: Observable that emits when writes are allowed again. Any pending writes
    ///         will be retried.
    ///   - scheduler: Scheduler used to perform writes on.
    init(write: @escaping (Data, Destination) -> Bool, retryTrigger: Observable<Void>, scheduler: SchedulerType) {
        self.writeRelay.asObservable()
            .observeOn(scheduler)
            .groupBy { $0.target }
            .flatMap { group in
                // Attempt to write new values as they're received, cancelling pending retries
                // for previously failed attempts.
                group.flatMapLatest { item -> Completable in
                    // Turn the `write` closure into a Completable.
                    let written = Completable.create { completable in
                        completable(write(item.data, item.target) ? .completed : .error(GGRawError.wouldBlock))
                        return Disposables.create()
                    }

                    return written.asObservable()
                        .retryWhen { _ in retryTrigger.observeOn(scheduler) }
                        .ignoreElements()
                }
            }
            .subscribe()
            .disposed(by: disposeBag)
    }
}
