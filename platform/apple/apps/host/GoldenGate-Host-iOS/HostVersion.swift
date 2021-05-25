//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  HostVersion.swift
//  GoldenGateHost-iOS
//
//  Created by Tudor Marinescu on 21/05/2018.
//

import GoldenGate

#if os(iOS)
    import UIKit
#endif

struct HardwareInfo: Encodable {
    // Identifier for the host device.
    let id: String

    init(id: String) {
        self.id = id
    }
}

struct PlatformInfo: Encodable {
    // Platform name
    let name: String

    // Operating system name
    let osName: String

    // Operating system version
    let osVersion: String

    init(name: String, osName: String, osVersion: String) {
        self.name = name
        self.osName = osName
        self.osVersion = osVersion
    }
}

struct BuildVersionInfo: Encodable {
    let maj: String
    let min: String
    let patch: String
    let buildDate: String?
    let buildTime: String?
    let commitHash: String?
    let branch: String?
    let commitCount: UInt32

    init(
        maj: String,
        min: String,
        patch: String,
        buildDate: String?,
        buildTime: String?,
        commitHash: String?,
        branch: String?,
        commitCount: UInt32
    ) {
        self.maj = maj
        self.min = min
        self.patch = patch
        self.buildDate = buildDate
        self.buildTime = buildTime
        self.commitHash = commitHash
        self.branch = branch
        self.commitCount = commitCount
    }

    init(_ version: GoldenGateVersion) {
        self.maj = String(version.major)
        self.min = String(version.minor)
        self.patch = String(version.patch)
        self.buildDate = version.buildDateComponent
        self.buildTime = version.buildTimeComponent
        self.commitHash = version.commitHash
        self.branch = version.branchName
        self.commitCount = version.commitCount
    }
}

protocol HostVersionProvider {
    var hostVersion: BuildVersionInfo { get }
    var hardwareInfo: HardwareInfo { get }
    var platformInfo: PlatformInfo { get }
}

struct HostVersion: HostVersionProvider {
    public static let shared = HostVersion()
    public let major: UInt16
    public let minor: UInt16
    public let patch: UInt16
    public let commitCount: UInt32
    public let commitHash: String?
    public let branchName: String?
    public let buildDateComponent: String?
    public let buildTimeComponent: String?
    public var hardwareInfo: HardwareInfo {
        #if os(iOS)
            return HardwareInfo(id: UIDevice.current.identifierForVendor?.uuidString ?? "Unknown iOS Device Identifier")
        #else
            return HardwareInfo(id: ProcessInfo.processInfo.hostName)
        #endif
    }
    public var hostVersion: BuildVersionInfo {
        return BuildVersionInfo(maj: String(major),
                                min: String(minor),
                                patch: String(patch),
                                buildDate: buildDateComponent,
                                buildTime: buildTimeComponent,
                                commitHash: commitHash,
                                branch: branchName,
                                commitCount: commitCount)
    }
    public var platformInfo: PlatformInfo {
        #if os(iOS)
            return PlatformInfo(name: "iOS", osName: UIDevice.current.systemName, osVersion: UIDevice.current.systemVersion)
        #else
            return PlatformInfo(name: "macOS", osName: "macOS", osVersion: ProcessInfo.processInfo.operatingSystemVersionString)
        #endif
    }

    private init() {
        if let appVersionString = Bundle.main.object(forInfoDictionaryKey: "CFBundleShortVersionString") as? String {
            let components = appVersionString.components(separatedBy: ".")
            self.major = UInt16(components[0])!
            self.minor = UInt16(components[1])!
            if components.indices.contains(2) {
                self.patch = UInt16(components[2])!
            } else {
                self.patch = 0
            }
        } else {
            self.major = 0
            self.minor = 0
            self.patch = 0
        }
        self.commitCount = 0
        self.commitHash = nil
        self.branchName = nil
        self.buildDateComponent = nil
        self.buildTimeComponent = nil
    }
}
