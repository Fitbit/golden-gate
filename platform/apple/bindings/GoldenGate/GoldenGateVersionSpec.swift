//
//  GoldenGateVersionSpec.swift
//  GoldenGate
//
//  Created by Sylvain Rebaud on 10/12/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

@testable import GoldenGate
import Nimble
import Quick

// swiftlint:disable:next superfluous_disable_command
// swiftlint:disable function_body_length force_try

class GoldenGateVersionSpec: QuickSpec {
    override func spec() {
        context("when receiving non nil strings") {
            var version: GoldenGateVersion!

            beforeEach {
                let getVersion: GoldenGateVersion.GetVersion = { (maj: UnsafeMutablePointer<UInt16>, min: UnsafeMutablePointer<UInt16>, patch: UnsafeMutablePointer<UInt16>, commitCount: UnsafeMutablePointer<UInt32>, commitHash: UnsafeMutablePointer<UnsafePointer<Int8>?>, branchName: UnsafeMutablePointer<UnsafePointer<Int8>?>, buildDate: UnsafeMutablePointer<UnsafePointer<Int8>?>, buildTime: UnsafeMutablePointer<UnsafePointer<Int8>?>) in
                    maj.pointee = 1
                    min.pointee = 2
                    patch.pointee = 3
                    commitCount.pointee = 100
                    commitHash.pointee = UnsafePointer<Int8>(strdup("9b5178e3"))//_convertConstStringToUTF8PointerArgument(strdup("9b5178e3")).1
                    branchName.pointee = UnsafePointer<Int8>(strdup("master"))
                    buildDate.pointee = UnsafePointer<Int8>(strdup("7 20 2017"))
                    buildTime.pointee = UnsafePointer<Int8>(strdup("10:07:46"))
                }
                version = GoldenGateVersion(getVersion)
            }

            it("parses fields properly") {
                expect(version.major) == 1
                expect(version.minor) == 2
                expect(version.patch) == 3
                expect(version.commitCount) == 100
                expect(version.commitHash) == "9b5178e3"
                expect(version.branchName) == "master"
                expect(version.buildDateComponent) == "7 20 2017"
                expect(version.buildTimeComponent) == "10:07:46"
            }

            it("has proper semantic version formatting") {
                expect(version.majorMinorPatch) == "1.2.3"
            }

            it("has valid description") {
                expect(version.description) == "1.2.3-100-9b5178e3-master"
            }

            it("has valid buildData") {
                var dateComponents = DateComponents()
                dateComponents.year = 2017
                dateComponents.month = 7
                dateComponents.day = 20
                dateComponents.hour = 10
                dateComponents.minute = 07
                dateComponents.second = 46

                expect(version.buildDate) == Calendar.current.date(from: dateComponents)
            }
        }

        context("when receiving nil strings") {
            var version: GoldenGateVersion!

            beforeEach {
                version = GoldenGateVersion { (maj: UnsafeMutablePointer<UInt16>, min: UnsafeMutablePointer<UInt16>, patch: UnsafeMutablePointer<UInt16>, commitCount: UnsafeMutablePointer<UInt32>, commitHash: UnsafeMutablePointer<UnsafePointer<Int8>?>, branchName: UnsafeMutablePointer<UnsafePointer<Int8>?>, buildDate: UnsafeMutablePointer<UnsafePointer<Int8>?>, buildTime: UnsafeMutablePointer<UnsafePointer<Int8>?>) in
                    maj.pointee = 1
                    min.pointee = 2
                    patch.pointee = 3
                    commitCount.pointee = 100
                    commitHash.pointee = nil
                    branchName.pointee = nil
                    buildDate.pointee = nil
                    buildTime.pointee = nil
                }
            }

            it("has valid description") {
                expect(version.description) == "1.2.3-100"
            }

            it("has no date") {
                expect(version.buildDate).to(beNil())
            }
        }

        it("has a default instance") {
            let version = GoldenGateVersion.shared
            expect(version).notTo(beNil())
        }
    }
}
