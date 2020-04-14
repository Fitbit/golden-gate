//
//  GattlinkParameters.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/28/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Foundation
import GoldenGateXP

public struct GattlinkProbeConfig {
    /// Size of window in ms. 0 to disable windowing.
    public let windowSize: UInt32

    /// Size of buffer to allocate for probe.
    public let bufferSampleCount: UInt32

    /// Threshold to use for when deciding to send event.
    public let bufferThreshold: UInt32

    /// Default config.
    public static let `default` = GattlinkProbeConfig(
        windowSize: 1000,
        bufferSampleCount: 50,
        bufferThreshold: 400
    )
}

/// Parameters for the Gattlink stack element.
///
/// - See Also: `GG_StackElementGattlinkParameters`
public struct GattlinkParameters {
    /// The incoming packet window size (or nil when using the default).
    public let rxWindow: UInt8?

    /// The outgoing packet window size (or nil when using the default).
    public let txWindow: UInt8?

    /// The buffer size (or nil when using the default).
    public let bufferSize: Int?

    /// Initial maximum fragment size (use 0 for default).
    public let initialMaxFragmentSize: UInt16

    /// Configuration for the data probe.
    public let probeConfig: GattlinkProbeConfig

    /// Create a parameters object.
    ///
    /// - Parameters:
    ///   - rxWindow: The incoming packet window size (or nil when using the default).
    ///   - txWindow: The outgoing packet window size (or nil when using the default).
    ///   - initialMaxFragmentSize: The initial maximum fragment size (use 0 for default).
    ///   - bufferSize: The buffer size (or nil when using the default).
    ///   - probeConfig: The configuration for the data probe.
    public init(
        rxWindow: UInt8? = nil,
        txWindow: UInt8? = nil,
        bufferSize: Int? = nil,
        initialMaxFragmentSize: UInt16 = 0,
        probeConfig: GattlinkProbeConfig = .default
    ) {
        assert(rxWindow != 0)
        self.rxWindow = rxWindow

        assert(txWindow != 0)
        self.txWindow = txWindow

        assert(bufferSize != 0)
        self.bufferSize = bufferSize

        self.initialMaxFragmentSize = initialMaxFragmentSize
        self.probeConfig = probeConfig
    }
}

// Utility to hold reference to the Swift objects
internal class GattlinkParametersContainer {
    private var gattlinkParameters: GattlinkParameters

    internal var gg: UnsafeHeapAllocatedValue<GG_StackElementGattlinkParameters>
    internal var probeConfig: UnsafeHeapAllocatedValue<GG_GattlinkProbeConfig>?

    internal init(gattlinkParameters: GattlinkParameters) {
        self.gattlinkParameters = gattlinkParameters

        self.probeConfig = UnsafeHeapAllocatedValue(
            GG_GattlinkProbeConfig(
                window_size_ms: gattlinkParameters.probeConfig.windowSize,
                buffer_sample_count: gattlinkParameters.probeConfig.bufferSampleCount,
                buffer_threshold: gattlinkParameters.probeConfig.bufferThreshold)
            )

        self.gg = UnsafeHeapAllocatedValue(
            GG_StackElementGattlinkParameters(
                rx_window: self.gattlinkParameters.rxWindow ?? 0,
                tx_window: self.gattlinkParameters.txWindow ?? 0,
                buffer_size: self.gattlinkParameters.bufferSize ?? 0,
                initial_max_fragment_size: self.gattlinkParameters.initialMaxFragmentSize,
                probe_config: self.probeConfig?.pointer
            )
        )
    }

    internal var ref: UnsafeMutablePointer<GG_StackElementGattlinkParameters> {
        return gg.pointer
    }
}
