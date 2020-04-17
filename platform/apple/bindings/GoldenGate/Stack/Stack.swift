//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  Stack.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/22/18.
//

import GoldenGateXP
import Rxbit
import RxCocoa
import RxSwift

public protocol StackBuilderType {
    associatedtype Stack

    /// Build (and destroys) stacks from a changing link.
    ///
    /// - Parameter link: The link to be used as the transport under the stack.
    /// - Returns: The current stack, or nil (if no link is present).
    func build(link: Observable<NetworkLink?>) -> Observable<Stack?>
}

/// Maintains one stack and destroys the old one before creating
/// a new stack when the link is changed.
public class StackBuilder: StackBuilderType {
    public typealias Stack = GoldenGate.Stack

    private let runLoop: RunLoop
    private let descriptor: Observable<StackDescriptor>
    private let role: Role
    private let stackProvider: Stack.Provider
    private let disposeBag = DisposeBag()

    /// Creates a StackBuilder.
    ///
    /// - Parameters:
    ///   - runLoop: The run loop of for the stack builder and its stacks.
    ///   - descriptor: The descriptor for the stack.
    ///   - role: The role of this device.
    ///   - stackProvider: A `Stack` provider.
    public init(
        runLoop: RunLoop,
        descriptor: Observable<StackDescriptor>,
        role: Role,
        stackProvider: @escaping Stack.Provider
    ) {
        self.runLoop = runLoop
        self.descriptor = descriptor.distinctUntilChanged(==)
        self.role = role
        self.stackProvider = stackProvider
    }

    public func build(link: Observable<NetworkLink?>) -> Observable<Stack?> {
        return Observable
            // Depends on descriptor and link
            .combineLatest(
                descriptor,
                link
            )
            .observeOn(runLoop)
            .do(onNext: { [weak self] tuple in
                LogBindingsVerbose("\(self ??? "StackBuilder").build: \(tuple)")
            })
            // Use scan so that we can destroy the previous stack
            // before allocating a new one
            .scan(nil as Stack?, accumulator: { [stackProvider, role] previous, element -> Stack? in
                previous?.destroy()
                guard case let (descriptor, link?) = element else { return nil }
                return try stackProvider(descriptor, role, link)
            })
            .do(onNext: { [weak self] stack in
                LogBindingsInfo("\(self ??? "StackBuilder").build: \(stack ??? "nil")")
            })
            .do(onNext: { $0?.start() })
    }
}

/// Wrapper for `GG_ACTIVITY_MONITOR_DIRECTION_*` values.
public struct ActivityMonitorDirection: Equatable, RawRepresentable {
    public let rawValue: UInt32

    public init(rawValue: UInt32) {
        self.rawValue = rawValue
    }

    /// Refers to the bottom to top direction of an activity monitor.
    ///
    /// - See Also: GG_ACTIVITY_MONITOR_DIRECTION_BOTTOM_TO_TOP
    static let bottomToTop = ActivityMonitorDirection(rawValue: UInt32(bitPattern: GG_ACTIVITY_MONITOR_DIRECTION_BOTTOM_TO_TOP))

    /// Refers to the top to bottom direction of an activity monitor.
    ///
    /// - See Also: GG_ACTIVITY_MONITOR_DIRECTION_TOP_TO_BOTTOM
    static let topToBottom = ActivityMonitorDirection(rawValue: UInt32(bitPattern: GG_ACTIVITY_MONITOR_DIRECTION_TOP_TO_BOTTOM))
}

extension ActivityMonitorDirection: CustomStringConvertible {
    public var description: String {
        switch rawValue {
        case ActivityMonitorDirection.bottomToTop.rawValue:
            return "incoming"
        case ActivityMonitorDirection.topToBottom.rawValue:
            return "outgoing"
        default:
            return "Unknown"
        }
    }
}

public enum StackEvent: Equatable {
    case tlsStateChange
    case linkMtuChange
    case gattlinkSessionReady
    case gattlinkSessionReset
    case gattlinkSessionStalled(time: TimeInterval)
    case gattlinkBufferOverThreshold
    case gattlinkBufferUnderThreshold
    case activityMonitorChanged(direction: ActivityMonitorDirection, active: Bool)
}

extension StackEvent {
    init?(_ ggEventPointer: UnsafePointer<GG_Event>) {
        let ggEventType = ggEventPointer.pointee.type

        switch ggEventType {
        case GGEventType.tlsStateChange.rawValue.rawValue:
            self = .tlsStateChange
        case GGEventType.linkMtuChange.rawValue.rawValue:
            self = .linkMtuChange
        case GGEventType.gattlinkSessionReady.rawValue.rawValue:
            self = .gattlinkSessionReady
        case GGEventType.gattlinkSessionReset.rawValue.rawValue:
            self = .gattlinkSessionReset
        case GGEventType.gattlinkSessionStalled.rawValue.rawValue:
            let stalledTime = ggEventPointer.withMemoryRebound(to: GG_GattlinkStalledEvent.self, capacity: 1) { stallEventPointer in
                TimeInterval(Double(stallEventPointer.pointee.stalled_time) / 1_000)
            }
            self = .gattlinkSessionStalled(time: stalledTime)
        case GGEventType.gattlinkBufferOverThreshold.rawValue.rawValue:
            self = .gattlinkBufferOverThreshold
        case GGEventType.gattlinkBufferUnderThreshold.rawValue.rawValue:
            self = .gattlinkBufferUnderThreshold
        case GGEventType.activityMonitorChange.rawValue.rawValue:
            let (direction, active) = ggEventPointer.withMemoryRebound(to: GG_ActivityMonitorChangeEvent.self, capacity: 1) { eventPointer in
                (ActivityMonitorDirection(rawValue: eventPointer.pointee.direction), eventPointer.pointee.active)
            }
            self = .activityMonitorChanged(direction: direction, active: active)
        default:
            LogBindingsError("Event with type \(ggEventType) is not handled")
            return nil
        }
    }
}

/// A stack that is likely to be a network stack.
public protocol StackType { }

/// A protocol for emitting stack events.
public protocol StackEventEmitter {
    /// Observable that will emit all the states of the stack.
    var event: Observable<StackEvent> { get }
}

/// A protocol for reporting a stack DTLS element state.
public protocol StackDtlsStateReporter {
    /// Get the status of the DTLS element if there is one.
    var dtlsState: DtlsProtocolState? { get }
}

/// A configurable default stack.
public class Stack: StackType, StackEventEmitter, StackDtlsStateReporter {
    public typealias Provider = (StackDescriptor, Role, NetworkLink) throws -> Stack

    /// Observable that will emit all the states of the stack.
    public let event: Observable<StackEvent>

    /// Get the status for the DTLS element of the stack, if there is one.
    public var dtlsState: DtlsProtocolState? {
        return gg.value.dtlsStatus.map { DtlsProtocolState($0) }
    }

    private let gg: RunLoopGuardedValue<GGStack>
    private let disposeBag = DisposeBag()

    /// Creates a new stack.
    ///
    /// - Parameters:
    ///   - runLoop: The run loop of the stack.
    ///   - role: The role of this device.
    ///   - descriptor: The descriptor for the stack.
    ///   - link: The link to communicate over.
    ///   - elementConfigurationProvider: An object that
    ///     provides configuration for each element of the stack on-demand.
    public init(
        runLoop: RunLoop,
        descriptor: StackDescriptor,
        role: Role,
        link: NetworkLink,
        elementConfigurationProvider: StackElementConfigurationProvider
    ) throws {
        var configuration = StackConfiguration(
            role: role,
            descriptor: descriptor,
            networkLinkMtu: link.mtu.value,
            configurationProvider: elementConfigurationProvider
        )

        self.gg = RunLoopGuardedValue(
            try GGStack(
                runLoop: runLoop,
                link: link,
                configuration: &configuration
            ),
            runLoop: runLoop
        )

        // Ensure MTU is kept up to date
        link.mtu
            .asObservable()
            .skip(1)
            .distinctUntilChanged()
            .observeOn(runLoop)
            .subscribe(onNext: gg.value.onMtuChange)
            .disposed(by: disposeBag)

        /// All the stack events are replayed in order to not miss any event.
        /// Note: This can be dangerous if the stack emits multiple elements.
        /// Right now, the stack is teared down if the connection is terminated
        /// so we should have a limited amount of events.
        // swiftlint:disable:next identifier_name
        let _event = ReplaySubject<StackEvent>.createUnbounded()
        self.event = _event.asObservable()

        let eventListener = GGEventListener { eventPointer in
            let unwrappedEventPointer = Stack.unwrapEvent(from: eventPointer)

            if let stackEvent = StackEvent(unwrappedEventPointer) {
                LogBindingsDebug("Event emitted \(stackEvent)")
                _event.on(.next(stackEvent))
            }
        }
        self.gg.value.register(eventListener: eventListener)
    }

    /// Provides access to the top most socket of the stack
    /// depending on the stack configuration.
    public private(set) lazy var topPort: Port? = {
        guard
            let port = try? gg.value.port(.top, element: .top)
        else { return nil }

        return port
    }()

    /// - See also: `GG_Stack_Start`
    fileprivate func start() {
        gg.value.start()
    }

    /// - See also: `GG_Stack_Destroy`
    fileprivate func destroy() {
        gg.value.destroy()
    }

    /// Retrieve the event emited by the stack. The stack emits two types of events:
    /// events generated by itself and the events forwarded by the modules used by the stack (like dtls).
    /// The method will unwrap if the event is a forwarded one.
    ///
    /// - Parameter eventPointer: Pointer to the event.
    /// - Returns: The UnsafePointer<GG_Event> emited by the stack.
    private static func unwrapEvent(from eventPointer: UnsafePointer<GG_Event>) -> UnsafePointer<GG_Event> {
        if eventPointer.pointee.type == GGEventType.stackEventForward.rawValue.rawValue {
            return eventPointer.withMemoryRebound(to: GG_StackForwardEvent.self, capacity: 1) { forwardedEvent in
                return forwardedEvent.pointee.forwarded
            }
        } else {
            return eventPointer
        }
    }
}

/// Wrapper around GG_Stack
private class GGStack {
    typealias Ref = OpaquePointer

    private let runLoop: RunLoop
    private let ref: Ref
    private var configuration: StackConfiguration
    private let link: NetworkLink
    private let linkDataSource: UnsafeDataSource
    private let linkDataSink: UnsafeDataSink
    private var eventListener: GGEventListener?

    /// Identifies if the stack was destroyed.
    ///
    /// Needed to allow destruction before deallocation.
    /// Some parts might still hold a reference to the old
    /// stack when we create a new stack, so we need to ensure
    private var destroyed = false

    /// Get the status for the DTLS element of the stack, if there is one.
    var dtlsStatus: GG_DtlsProtocolStatus? {
        var status = GG_DtlsProtocolStatus()

        return withUnsafeMutablePointer(to: &status) { pointer in
            let result = GG_Stack_GetDtlsProtocolStatus(ref, pointer)
            if result == GG_SUCCESS {
                return pointer.pointee
            } else {
                return nil
            }
        }
    }

    fileprivate init(runLoop: RunLoop, link: NetworkLink, configuration: inout StackConfiguration) throws {
        runLoopPrecondition(condition: .onRunLoop)

        // Ensure dependencies stay alive
        self.configuration = configuration
        self.link = link
        self.runLoop = runLoop

        let linkDataSource = link.dataSource.gg
        self.linkDataSource = linkDataSource

        let linkDataSink = link.dataSink.gg
        self.linkDataSink = linkDataSink

        self.ref = try self.configuration.withParameters { parameters in
            var ref: Ref?
            try GG_StackBuilder_BuildStack(
                configuration.descriptor.rawValue,
                parameters,
                parameters.count,
                configuration.role.stackRole,
                nil,
                runLoop.ref,
                linkDataSource.ref,
                linkDataSink.ref,
                &ref
            ).rethrow()
            return ref!
        }
    }

    deinit {
        /// Destroy the stack if we haven't done it before.
        if !destroyed {
            destroy()
        }
    }

    /// Emit MTU Change events onto the stack.
    ///
    /// - See also: `GG_StackLinkMtuChangeEvent`
    fileprivate func onMtuChange(_ mtu: UInt) {
        runLoopPrecondition(condition: .onRunLoop)
        guard !destroyed else { return }

        let event = UnsafeHeapAllocatedValue(
            GG_StackLinkMtuChangeEvent(
                base: GG_Event(type: .linkMtuChange, source: nil),
                link_mtu: UInt32(clamping: mtu)
            )
        )

        let listenerRef = GG_Stack_AsEventListener(ref)
        event.pointer.withMemoryRebound(to: GG_Event.self, capacity: 1) {
            GG_EventListener_OnEvent(listenerRef, $0)
        }
    }

    func register(eventListener: GGEventListener?) {
        self.eventListener = eventListener
        let emitter = GG_Stack_AsEventEmitter(ref)
        GG_EventEmitter_SetListener(emitter, eventListener?.ref)
    }

    /// Access to a port of an element in the Stack.
    ///
    /// - See also: `GG_Stack_GetPortById`
    func port(_ id: StackPortId, element: StackElementId) throws -> Port? {
        runLoopPrecondition(condition: .onRunLoop)
        precondition(!destroyed)

        var portInfo = GG_StackElementPortInfo()

        do {
            try GG_Stack_GetPortById(ref, element.rawValue, id.rawValue, &portInfo).rethrow()
        } catch let error as GGRawError where error.ggResult == GG_ERROR_NO_SUCH_ITEM {
            return nil
        }

        guard portInfo.sink != nil && portInfo.source != nil else {
            LogBindingsWarning("Element \(element) did not have a sink/source at port \(port ??? "nil")")
            return nil
        }

        return GGStackPort(
            dataSink: UnmanagedDataSink(portInfo.sink),
            dataSource: UnmanagedDataSource(portInfo.source),
            verify: { [weak self] in
                runLoopPrecondition(condition: .onRunLoop)
                // Prevent any operations if stack was deallocated
                // or marked for destruction.
                guard let self = self, !self.destroyed else {
                    throw GGRawError.invalidState
                }
            }
        )
    }

    /// Starts the stack.
    ///
    /// - See also: `GG_Stack_Start`
    func start() {
        runLoopPrecondition(condition: .onRunLoop)
        precondition(!destroyed)

        GG_Stack_Start(ref)
    }

    /// Destroys the stack.
    ///
    /// - See also: `GG_Stack_Destroy`
    func destroy() {
        runLoop.sync {
            guard !destroyed else {
                assertLogBindingsError("Stack was already destroyed.")
                return
            }

            // Clear any registered listener
            register(eventListener: nil)

            destroyed = true

            // Make sure the link does not try to communicate with the Stack anymore
            _ = try? link.dataSource.setDataSink(nil)
            _ = try? link.dataSink.setDataSinkListener(nil)

            GG_Stack_Destroy(ref)
        }
    }
}

private extension Role {
    /// Utility to cast Role to GG_StackRole
    var stackRole: GG_StackRole {
        switch self {
        case .hub:
            return GG_STACK_ROLE_HUB
        case .node:
            return GG_STACK_ROLE_NODE
        }
    }
}

private extension GGEventType {
    /// Event indicating an MTU Change (see GG_EVENT_TYPE_LINK_MTU_CHANGE)
    static let linkMtuChange = GGEventType(rawValue: "mtuc")
    /// Event indicating a tls state change (see GG_EVENT_TYPE_TLS_STATE_CHANGE)
    static let tlsStateChange = GGEventType(rawValue: "tlss")
    /// Event indicating a forwarded wrapped event (see GG_EVENT_TYPE_STACK_EVENT_FORWARD)
    static let stackEventForward = GGEventType(rawValue: "stkf")
    /// Event indicating a gattlink session is up (see GG_EVENT_TYPE_GATTLINK_SESSION_READY)
    static let gattlinkSessionReady = GGEventType(rawValue: "gls+")
    /// Event indicating a gattlink session reset (see GG_EVENT_TYPE_GATTLINK_SESSION_RESET)
    static let gattlinkSessionReset = GGEventType(rawValue: "gls-")
    /// Event indicating a gattlink session is stalled (see GG_EVENT_TYPE_GATTLINK_SESSION_STALLED)
    static let gattlinkSessionStalled = GGEventType(rawValue: "gls#")
    /// Event indicating a gattlink buffer size over threshold
    /// (see GATTLINK_CLIENT_OUTPUT_BUFFER_OVER_THRESHOLD)
    static let gattlinkBufferOverThreshold = GGEventType(rawValue: "glb+")
    /// Event indicating a gattlink buffer size under threshold
    /// (see GATTLINK_CLIENT_OUTPUT_BUFFER_UNDER_THRESHOLD)
    static let gattlinkBufferUnderThreshold = GGEventType(rawValue: "glb-")
    /// Event indicating a change of activity in the activity monitor element
    static let activityMonitorChange = GGEventType(rawValue: "amoc")
}

/// Throws an error if object is not permitted
private typealias Verifier = () throws -> Void

/// Utility to wrap a "port" that we got from the Stack into a Socket
private struct GGStackPort: Port {
    let dataSink: DataSink
    let dataSource: DataSource

    init(dataSink: DataSink, dataSource: DataSource, verify: @escaping Verifier) {
        self.dataSink = GGDataSinkWithVerifier(dataSink, verify: verify)
        self.dataSource = GGDataSourceWithVerifier(dataSource, verify: verify)
    }
}

/// Calls `verify` before performing any DataSource operations.
///
/// Can be used to suppress any calls into unsafe C APIs
/// when the wrapped C API has been deallocated.
private class GGDataSourceWithVerifier: DataSource {
    private let wrapped: DataSource
    private let verify: Verifier
    private var dataSink: DataSink?

    init(_ wrapped: DataSource, verify: @escaping Verifier) {
        self.wrapped = wrapped
        self.verify = verify
    }

    func setDataSink(_ dataSink: DataSink?) throws {
        self.dataSink = dataSink
        try verify()
        try wrapped.setDataSink(dataSink)
    }
}

/// Calls verifier before performing any DataSource operations.
private class GGDataSinkWithVerifier: DataSink {
    private let wrapped: DataSink
    private let verify: Verifier

    init(_ wrapped: DataSink, verify: @escaping Verifier) {
        self.wrapped = wrapped
        self.verify = verify
    }

    func put(_ buffer: Buffer, metadata: DataSink.Metadata?) throws {
        try verify()
        try wrapped.put(buffer, metadata: metadata)
    }

    func setDataSinkListener(_ dataSinkListener: DataSinkListener?) throws {
        try verify()
        try wrapped.setDataSinkListener(dataSinkListener)
    }
}
