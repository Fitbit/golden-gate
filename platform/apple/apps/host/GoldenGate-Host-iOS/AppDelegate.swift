//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  AppDelegate.swift
//  GoldenGate-Host-iOS
//
//  Created by Bogdan Vlad on 9/24/17.
//

import BluetoothConnection
import GoldenGate
import RxSwift
import UIKit
import UserNotifications

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Initialize GoldenGate logger before anything else
        // Don't use NSLogger if STDERR is a TTY (i.e. a debugger is attached)
        if isatty(STDERR_FILENO) == 0 {
            GoldenGateLogger.defaultLogger = NSLoggerLogger(UIDevice.current.name)
        }

        requestNotificationAuthorization()

        return true
    }

    private func requestNotificationAuthorization() {
        let center = UNUserNotificationCenter.current()

        center.getNotificationSettings { (notificationSettings) in
            guard notificationSettings.authorizationStatus != .authorized else { return }

            center.requestAuthorization(options: [.alert]) { (granted, error) in
                if !granted {
                    Log("User notification center authorization request failed with error: \(error ??? "nil").", .error)
                }
            }
        }
    }

    private func notifyIfRelaunchedByBluetooth(launchOptions: [UIApplication.LaunchOptionsKey: Any]?) {
        guard launchOptions?[.bluetoothCentrals] != nil || launchOptions?[.bluetoothPeripherals] != nil else { return }

        let notificationContent = UNMutableNotificationContent()
        notificationContent.title = "App relaunched in background by Bluetooth."

        let notificationRequest = UNNotificationRequest(identifier: "gg.host.relaunched.bluetooth", content: notificationContent, trigger: nil)
        UNUserNotificationCenter.current().add(notificationRequest, withCompletionHandler: nil)
    }

    func applicationWillResignActive(_ application: UIApplication) {
        // Sent when the application is about to move from active to inactive state. This can occur for certain types of temporary interruptions (such as an incoming phone call or SMS message) or when the user quits the application and it begins the transition to the background state.
        // Use this method to pause ongoing tasks, disable timers, and invalidate graphics rendering callbacks. Games should use this method to pause the game.
    }

    func applicationDidEnterBackground(_ application: UIApplication) {
        // Use this method to release shared resources, save user data, invalidate timers, and store enough application state information to restore your application to its current state in case it is terminated later.
        // If your application supports background execution, this method is called instead of applicationWillTerminate: when the user quits.
    }

    func applicationWillEnterForeground(_ application: UIApplication) {
        // Called as part of the transition from the background to the active state; here you can undo many of the changes made on entering the background.
    }

    func applicationDidBecomeActive(_ application: UIApplication) {
        // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
        Component.instance.appDidBecomeActiveSubject.onNext(())
    }

    func applicationWillTerminate(_ application: UIApplication) {
        // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
    }
    
    func application(_ application: UIApplication, configurationForConnecting connectingSceneSession: UISceneSession, options: UIScene.ConnectionOptions) -> UISceneConfiguration {
        guard connectingSceneSession.role == UISceneSession.Role.windowApplication else {
            fatalError("Unknown scene role \(connectingSceneSession.role)")
         }
        let config = UISceneConfiguration(name: nil, sessionRole: connectingSceneSession.role)
        config.delegateClass = SceneDelegate.self
        return config
    }
}
