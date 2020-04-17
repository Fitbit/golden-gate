// Copyright 2016-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/// Utility to deduplicate float casting logic in `SingleValueDecodingContainer`.
protocol FbSmoCompatibleFloatingPoint: FloatingPoint {
    init?(exactly source: Int64)
    init?(exactly source: Double)
}

extension FbSmoCompatibleFloatingPoint {
    /// Utility to keep +Infinity/-Infinity/NaN when using `init?(exactly:)`.
    init?(exactlyKeepingInfinite source: Double) {
        guard let value = Self.from(exactlyKeepingInfinite: source) else {
            return nil
        }

        // This `self` assignment is only possible in Swift 4.1
        // otherwise `'self' used before self.init call` is reported.
        self = value
    }

    /// Utility to keep +Infinity/-Infinity/NaN when using `init?(exactly:)`.
    static func from(exactlyKeepingInfinite source: Double) -> Self? {
        if source.isFinite {
            return Self.init(exactly: source)
        } else if source.isNaN {
            // swiftlint:disable:next colon
            return source.isSignalingNaN ? Self.signalingNaN : Self.nan
        } else if source.isInfinite {
            var value = Self.infinity

            if source.sign == .minus {
                value.negate()
            }

            return value
        } else {
            assertionFailure("Unexpected floating-point class")
            return nil
        }
    }
}

// MARK: - Conformances

extension Double: FbSmoCompatibleFloatingPoint {}
extension Float: FbSmoCompatibleFloatingPoint {}
