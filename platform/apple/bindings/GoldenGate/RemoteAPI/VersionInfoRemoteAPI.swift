//
//  VersionInfoRemoteAPI.swift
//  GoldenGate
//
//  Created by Tudor Marinescu on 16/05/2018.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import GoldenGateXP
import RxSwift

public struct HardwareInfo: Encodable {
    // Identifier for the host device.
    let id: String

    public init(id: String) {
        self.id = id
    }
}

public struct PlatformInfo: Encodable {
    // Platform name
    let name: String

    // Operating system name
    let osName: String

    // Operating system version
    let osVersion: String

    public init(name: String, osName: String, osVersion: String) {
        self.name = name
        self.osName = osName
        self.osVersion = osVersion
    }
}

public struct BuildVersionInfo: Encodable {
    let maj: String
    let min: String
    let patch: String
    let buildDate: String?
    let buildTime: String?
    let commitHash: String?
    let branch: String?
    let commitCount: UInt32

    public init(
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

private struct VersionInfo: Encodable {
    let ggLibVersion: BuildVersionInfo
    let hostVersion: BuildVersionInfo
    let ggServicesVersion: String?
}

public protocol HostVersionProvider {
    var hostVersion: BuildVersionInfo { get }
    var hardwareInfo: HardwareInfo { get }
    var platformInfo: PlatformInfo { get }
}

public class VersionInfoRemoteAPI: RemoteApiModule {
    private let provider: HostVersionProvider

    public let methods: Set<String> = Method.allUniqueRawValues

    public init(provider: HostVersionProvider) {
        self.provider = provider
    }

    public func publishHandlers(on remoteShell: RemoteShell) {
        remoteShell.register(Method.getHost.rawValue, handler: getHost)
        remoteShell.register(Method.getPlatform.rawValue, handler: getPlatform)
        remoteShell.register(Method.getVersion.rawValue, handler: getVersion)
    }

    private func getHost() -> Single<HardwareInfo> {
        return .just(provider.hardwareInfo)
    }

    private func getPlatform() -> Single<PlatformInfo> {
        return .just(provider.platformInfo)
    }

    private func getVersion() -> Single<VersionInfo> {
        let versionInfo = VersionInfo(
            ggLibVersion: BuildVersionInfo(GoldenGateVersion.shared),
            hostVersion: provider.hostVersion,
            ggServicesVersion: nil
        )

        return Single.just(versionInfo)
    }
}

private extension VersionInfoRemoteAPI {
    enum Method: String, CaseIterable {
        case getHost        = "gg/get_host"
        case getPlatform    = "gg/get_platform"
        case getVersion     = "gg/get_version"
    }
}
