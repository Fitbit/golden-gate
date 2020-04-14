//
//  AppDelegate.swift
//  GoldenGate-Host-iOS
//
//  Created by Bogdan Vlad on 9/24/17.
//  Copyright © 2017 Fitbit. All rights reserved.
//

import GoldenGate
import RxSwift
import UIKit
import UserNotifications

@UIApplicationMain
class AppDelegate: UIResponder, UIApplicationDelegate {

    var window: UIWindow?

    func application(_ application: UIApplication, didFinishLaunchingWithOptions launchOptions: [UIApplication.LaunchOptionsKey: Any]?) -> Bool {
        // Initialize GoldenGate logger before anything else
        // Don't use NSLogger if STDERR is a TTY (i.e. a debugger is attached)
        if isatty(STDERR_FILENO) == 0 {
            GoldenGateLogger.defaultLogger = NSLoggerLogger(UIDevice.current.name)
        }

        requestNotificationAuthorization()
        notifyIfRelaunchedByBLE(launchOptions: launchOptions)

        let component = Component.instance

        didBecomeActiveSubject
            .subscribe(component.appDidBecomeActiveSubject)
            .disposed(by: disposeBag)

        let peerManager = component.peerManager

        if let remoteTestServer = component.remoteTestServer {
            let stackRemoteApi = StackRemoteAPI()
            stackRemoteApi.globalConfigurable = component
            remoteTestServer.register(module: stackRemoteApi)

            remoteTestServer.register(
                module: PairRemoteApi(
                    peerManager: peerManager,
                    scanner: Component.instance.scanner
                )
            )

            remoteTestServer.register(
                module: ConnectionRemoteApi(
                    peerManager: peerManager,
                    scanner: Component.instance.scanner
                )
            )

            remoteTestServer.register(
                module: BluetoothRemoteApi(
                    bluetoothManager: BluetoothManagerWrapper()
                )
            )

            remoteTestServer.register(
                module: SystemRemoteApi()
            )

            // Start the remote test server if necessary
            remoteTestServer.start()
        }

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

    private func notifyIfRelaunchedByBLE(launchOptions: [UIApplication.LaunchOptionsKey: Any]?) {
        guard launchOptions?[.bluetoothCentrals] != nil || launchOptions?[.bluetoothPeripherals] != nil else { return }

        let notificationContent = UNMutableNotificationContent()
        notificationContent.title = "App relaunched in background by BLE."

        let notificationRequest = UNNotificationRequest(identifier: "gg.host.relaunched.ble", content: notificationContent, trigger: nil)
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

    private let didBecomeActiveSubject = PublishSubject<Void>()

    func applicationDidBecomeActive(_ application: UIApplication) {
        // Restart any tasks that were paused (or not yet started) while the application was inactive. If the application was previously in the background, optionally refresh the user interface.
        didBecomeActiveSubject.on(.next(()))
    }

    func applicationWillTerminate(_ application: UIApplication) {
        // Called when the application is about to terminate. Save data if appropriate. See also applicationDidEnterBackground:.
    }
}

extension LoggerDomain {
    static let hockey = LoggerDomain("HockeySDK")
}
