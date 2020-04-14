//
//  GGEventListener.swift
//  GoldenGate
//
//  Created by Bogdan Vlad on 03/09/2018.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGateXP

/// Wrapper over GG_EventListener from XP.
/// Listens for events and call the closure wherever one is triggered.
final class GGEventListener: GGAdaptable {
    typealias GGObject = GG_EventListener
    typealias GGInterface = GG_EventListenerInterface

    let adapter: Adapter
    private let eventClosure: (UnsafePointer<GG_Event>) -> Void

    private static var interface = GG_EventListenerInterface { ref, event in
        let `self` = Adapter.takeUnretained(ref)
        guard let eventPointer = event else {
            assertionFailure("Event should not be nil.")
            return
        }

        self.eventClosure(eventPointer)
    }

    /// Constructor for GGEventListener object
    ///
    /// - Parameter eventClosure: Closure that will get called when a new event is triggered.
    init(eventClosure: @escaping (UnsafePointer<GG_Event>) -> Void) {
        self.eventClosure = eventClosure
        self.adapter = Adapter(iface: &type(of: self).interface)
        adapter.bind(to: self)
    }
}
