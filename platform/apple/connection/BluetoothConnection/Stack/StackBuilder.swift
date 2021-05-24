//
//  StackBuilder.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 04/06/2020.
//  Copyright © 2020 Fitbit. All rights reserved.
//

import RxSwift

public protocol StackBuilderType {
    typealias Provider = (StackDescriptor, Role, NetworkLink) throws -> StackType

    /// Build (and destroys) stacks from a changing link.
    ///
    /// - Parameter link: The link to be used as the transport under the stack.
    /// - Returns: The current stack, or nil (if no link is present).
    func build(link: Observable<NetworkLink?>) -> Observable<StackType?>
}

/// Maintains one stack and destroys the old one before creating
/// a new stack when the link is changed.
public class StackBuilder: StackBuilderType {
    private let protocolScheduler: ImmediateSchedulerType
    private let descriptor: Observable<StackDescriptor>
    private let role: Role
    private let stackProvider: Provider
    private let disposeBag = DisposeBag()

    /// Creates a StackBuilder.
    ///
    /// - Parameters:
    ///   - protocolScheduler: The scheduler for the stack builder and its stacks.
    ///   - descriptor: The descriptor for the stack.
    ///   - role: The role of this device.
    ///   - stackProvider: A `StackType` provider.
    public init(
        protocolScheduler: ImmediateSchedulerType,
        descriptor: Observable<StackDescriptor>,
        role: Role,
        stackProvider: @escaping Provider
    ) {
        self.protocolScheduler = protocolScheduler
        self.descriptor = descriptor.distinctUntilChanged(==)
        self.role = role
        self.stackProvider = stackProvider
    }

    public func build(link: Observable<NetworkLink?>) -> Observable<StackType?> {
        return Observable
            // Depends on descriptor and link
            .combineLatest(
                descriptor,
                link
            )
            .observeOn(protocolScheduler)
            .logInfo("StackBuilder.build (descriptor, link):", .bluetooth, .next)
            // Use scan so that we can destroy the previous stack
            // before allocating a new one
            .scan(nil as StackType?, accumulator: { [stackProvider, role] previous, element -> StackType? in
                previous?.destroy()
                guard case let (descriptor, link?) = element else { return nil }
                return try stackProvider(descriptor, role, link)
            })
            .logInfo("StackBuilder.build stack:", .bluetooth, .next)
            .do(onNext: { $0?.start() })
    }
}
