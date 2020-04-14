//
//  HostVersion.swift
//  GoldenGateHost-iOS
//
//  Created by Tudor Marinescu on 21/05/2018.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGate

#if os(iOS)
    import UIKit
#endif

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
