//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  RunLoop+Async.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/22/18.
//

import Foundation
import GoldenGateXP

extension RunLoop {
    var timerSchedulerRef: OpaquePointer {
        return GG_Loop_GetTimerScheduler(ref)
    }

    /// Schedule some work after a certain deadline.
    ///
    /// - See Also: `DispatchQueue.asyncAfter`
    public func asyncAfter(deadline: DispatchTime, execute work: @escaping () -> Void) {
        async {
            if !self.running {
                LogBindingsWarning("\(self) is not running yet")
            }

            var timerRef: OpaquePointer?

            do {
                try GG_TimerScheduler_CreateTimer(self.timerSchedulerRef, &timerRef).rethrow()
            } catch {
                LogBindingsWarning("Timer pool exhausted. Switching to unbounded GCD...")

                self.internalQueue.asyncAfter(deadline: deadline) {
                    self.async(execute: work)
                }
            }

            let now = DispatchTime.now()

            let millisecondsFromNow: UInt32

            if deadline.uptimeNanoseconds <= now.uptimeNanoseconds {
                millisecondsFromNow = 0
            } else {
                let nanoseconds = deadline.uptimeNanoseconds &- now.uptimeNanoseconds
                let milliseconds = nanoseconds / 1_000_000

                if milliseconds > UInt32.max {
                    LogBindingsWarning("Timer deadline too far in the future: \(milliseconds)ms")
                    millisecondsFromNow = UInt32.max
                } else {
                    millisecondsFromNow = UInt32(milliseconds)
                }
            }

            let timerListener = TimerListener(closure: work)

            GG_Timer_Schedule(timerRef, timerListener.retain().ref, millisecondsFromNow)
        }
    }
}

private final class TimerListener: GGAdaptable {
    private let closure: () -> Void
    private var release: (() -> Void)?

    typealias GGObject = GG_TimerListener
    typealias GGInterface = GG_TimerListenerInterface

    let adapter: Adapter

    private static var interface = GG_TimerListenerInterface(
        OnTimerFired: { timerListenerRef, timerRef, _ in
            let timerListener = Adapter.takeUnretained(timerListenerRef)
            timerListener.closure()
            GG_Timer_Destroy(timerRef)
            timerListener.release?()
        }
    )

    init(closure: @escaping () -> Void) {
        self.closure = closure

        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }

    fileprivate func retain() -> TimerListener {
        // Note: This intentionally creates a retain-cycle which is
        // resolved when `release` is called by the loop. 🎩
        release = { self.release = nil }
        return self
    }
}
