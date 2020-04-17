//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  GoldenGateVersion.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 3/26/18.
//

import GoldenGateXP

/// The version info of the library.
public class GoldenGateVersion {
    /// Singleton to access the version info of the library.
    public static let shared = GoldenGateVersion()

    /// The major component of the version number.
    public let major: UInt16

    /// The minor component of the version number.
    public let minor: UInt16

    /// The patch component of the version number.
    public let patch: UInt16

    /// The number of commits made to the branch from which the library was built.
    public let commitCount: UInt32

    /// The commit hash from which the library was built.
    public let commitHash: String?

    /// The branch from which the library was built.
    public let branchName: String?

    /// The date component of the date of when the library was built.
    public let buildDateComponent: String?

    /// The time component of the date of when the library was built.
    public let buildTimeComponent: String?

    internal typealias GetVersion = (_ maj: UnsafeMutablePointer<UInt16>, _ min: UnsafeMutablePointer<UInt16>, _ patch: UnsafeMutablePointer<UInt16>, _ commitCount: UnsafeMutablePointer<UInt32>, _ commitHash: UnsafeMutablePointer<UnsafePointer<Int8>?>, _ branchName: UnsafeMutablePointer<UnsafePointer<Int8>?>, _ buildDate: UnsafeMutablePointer<UnsafePointer<Int8>?>, _ buildTime: UnsafeMutablePointer<UnsafePointer<Int8>?>) -> Void

    convenience private init() {
        self.init(GG_Version)
    }

    internal init(_ getVersion: GetVersion) {
        var major: UInt16 = 0
        var minor: UInt16 = 0
        var patch: UInt16 = 0
        var commitCount: UInt32 = 0
        var commitHash: UnsafePointer<Int8>?
        var branchName: UnsafePointer<Int8>?
        var buildDateComponent: UnsafePointer<Int8>?
        var buildTimeComponent: UnsafePointer<Int8>?

        getVersion(
            &major,
            &minor,
            &patch,
            &commitCount,
            &commitHash,
            &branchName,
            &buildDateComponent,
            &buildTimeComponent
        )

        self.major = major
        self.minor = minor
        self.patch = patch
        self.commitCount = commitCount
        self.commitHash = commitHash.map { String(cString: $0) }
        self.branchName = branchName.map { String(cString: $0) }
        self.buildDateComponent = buildDateComponent.map { String(cString: $0) }
        self.buildTimeComponent = buildTimeComponent.map { String(cString: $0) }
    }

    /// The "classic" version string (1.2.3) of the library
    public var majorMinorPatch: String {
        return "\(major).\(minor).\(patch)"
    }

    /// The date of when the library was built
    public private(set) lazy var buildDate: Date? = {
        let components = [buildDateComponent, buildTimeComponent].compactMap { $0 }
        guard !components.isEmpty else { return nil }

        let string = components.joined(separator: " ")
        let formatter = DateFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.dateFormat = "M dd yyyy HH:mm:ss"

        return formatter.date(from: string)
    }()
}

extension GoldenGateVersion: CustomStringConvertible {
    public var description: String {
        return [majorMinorPatch, "\(commitCount)", commitHash, branchName]
            .compactMap { $0 }
            .joined(separator: "-")
    }
}
