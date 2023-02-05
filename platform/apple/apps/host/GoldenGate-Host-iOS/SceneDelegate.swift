//
//  SceneDelegate.swift
//  GoldenGateHost
//
//  Created by Axel Molchanov on 1/10/23.
//  Copyright © 2023 Fitbit. All rights reserved.
//

import UIKit

final class SceneDelegate: UIResponder, UIWindowSceneDelegate {

    var window: UIWindow?

    func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
        guard let windowScene = (scene as? UIWindowScene) else { return }
        window = UIWindow(windowScene: windowScene)
        let navigationController = UINavigationController(rootViewController: RootViewController())
        window?.rootViewController = navigationController
        window?.makeKeyAndVisible()
    }
}
