//
//  AppDelegate.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

import Cocoa
import Foundation

@NSApplicationMain
class AppDelegate: NSObject, NSApplicationDelegate {
    func applicationWillFinishLaunching(_ notification: Notification) {
        Component.instance.nodeSimulator.start()
    }
}
