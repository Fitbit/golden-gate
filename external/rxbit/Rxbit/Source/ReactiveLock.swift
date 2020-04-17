//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ReactiveLock.swift
//  Devicebit
//
//  Created by Bogdan Vlad on 16/01/2019.
//

import Foundation
import RxCocoa
import RxSwift

// swiftlint:disable trailing_closure

/// Inspired from TicketLock implementation (https://en.wikipedia.org/wiki/Ticket_lock).
///
/// Class that assures mutual exclusion and fairness for accessing a critical section.
public class ReactiveLock {
	/// Each `acquire()` call will get a "turn" number.
	/// The `acquire` call will wait until the "turn" number is equal to the first ticket in the queue.
	/// This assures a FIFO style fairness.

	private let ticketQueueRelay = BehaviorRelay<[Int]>(value: [])
	private let scheduler: SchedulerType

	public init(scheduler: SerialDispatchQueueScheduler) {
		self.scheduler = scheduler
	}

	// Should be only used directly for testing.
	init(scheduler: SchedulerType) {
		self.scheduler = scheduler
	}

	/// Acquires the lock. The lock will be released when the returned observable is disposed.
	///
	/// - Returns: Observable that emits when the lock was acquired.
	func acquire() -> Observable<Void> {
		return Observable<Void>.deferred {
			let turn = (self.ticketQueueRelay.value.max() ?? 0) + 1
			self.ticketQueueRelay.accept(self.ticketQueueRelay.value + [turn])

			return self.ticketQueueRelay
				.filter { $0.first == turn }
				.do(onDispose: {
					self.release(ticket: turn)
				})
				.map { _ in () }
		}
		.subscribeOn(scheduler)
	}

	private func release(ticket: Int) {
		_ = scheduler.schedule(()) { _ in
			let newTickets = self.ticketQueueRelay.value.filter { $0 != ticket }
			self.ticketQueueRelay.accept(newTickets)

			return Disposables.create()
		}
	}
}

public extension ObservableType {
	/// The method guarantees that the observable won't be subscribed if the lock is acquired by someone else.
	/// If the lock is locked, the observable will wait until it is released. Once the lock is released, the observable will be subscribed.
	///
	/// - Parameter lock: Lock that guarantees mutual exclusion.
	/// - Returns: The observable that will be subscribed once it enters the critical section.
	func synchronized(using lock: ReactiveLock) -> Observable<Self.Element> {
		return lock.acquire()
			.flatMapFirst { _ in
				self.materialize()
			}
			.takeWhile { state in
				switch state {
				case .completed: return false
				default: return true
				}
			}
			.dematerialize()
	}
}
