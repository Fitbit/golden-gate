//
//  TestScheduler+AdvanceBy.swift
//  BluetoothConnectionTests
//
//  Created by Emanuel Jarnea on 2/25/21.
//  Copyright © 2021 Fitbit. All rights reserved.
//

import RxTest

extension TestScheduler {
    /// Advances the scheduler's clock by the specified virtual time, running all work till that point.
    /// - Parameter virtualTime: Absolute time to advance the scheduler's clock by. The real time is
    /// mapped to virtual time using the resolution passed to the scheduler's init. Considering the default
    /// resolution (1), this default virtual time corresponds to 10 seconds of real time.
    func advance(by virtualTime: VirtualTime = 10) {
        advanceTo(clock + virtualTime)
    }

    /// Execute the specified action and advance the scheduler's clock by a predefined time step.
    @discardableResult
    static func ~~><T>(action: @autoclosure () throws -> T, scheduler: TestScheduler) rethrows -> T {
        let result = try action()
        scheduler.advance()
        return result
    }
}

infix operator ~~>
