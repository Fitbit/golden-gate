//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ManagedPeerListViewController.swift
//  GoldenGateHost-iOS
//
//  Created by Marcel Jackwerth on 11/2/17.
//

import BluetoothConnection
import Foundation
import GoldenGate
import RxCocoa
import RxSwift
import UIKit

class ManagedPeerListViewController: UITableViewController {
    var peerManager: PeerManager<ManagedNode>!

    private let disposeBag = DisposeBag()

    override func viewDidLoad() {
        super.viewDidLoad()

        title = "Paired Peers"
        navigationItem.backBarButtonItem = UIBarButtonItem(
            title: "Peers",
            style: .plain,
            target: nil,
            action: nil
        )

        peerManager = Component.instance.peerManager

        let activityIndicator = UIActivityIndicatorView(style: .gray)
        let addButton = UIBarButtonItem(barButtonSystemItem: .add, target: nil, action: nil)

        navigationItem.rightBarButtonItems = [
            addButton,
            UIBarButtonItem(customView: activityIndicator)
        ]

        tableView.dataSource = nil
        tableView.register(Cell.self)

        peerManager.peers
            .asObservable()
            .observeOn(MainScheduler.instance)
            .bind(to: tableView.rx.items) { tableView, _, peer in
                let cell = tableView.dequeue(Cell.self)!
                cell.bind(peer)
                return cell
            }
            .disposed(by: disposeBag)

        addButton.rx.tap
            .subscribe(onNext: { [weak self] in
                self?.pairing()
            })
            .disposed(by: disposeBag)
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
        peerManager.peers.value[indexPath.row].connectionController.establishConnection(trigger: "admin")
    }

    override func tableView(_ tableView: UITableView, accessoryButtonTappedForRowWith indexPath: IndexPath) {
        let peerManager = self.peerManager!
        let peer = peerManager.peers.value[indexPath.row]
        let viewController = PeerViewController<PeerViewModel<HubConnection>>(style: .grouped)
        let linkConfigurationViewControllerViewModel = LinkConfigurationViewControllerViewModel(
            preferredConnectionConfiguration: peer.preferredConnectionConfiguration,
            preferredConnectionMode: peer.preferredConnectionMode
        )

        viewController.title = peer.name.value

        let connection = peer.connectionController.connectionStatus.map { $0.connection }
        viewController.viewModel = PeerViewModel(
            peer: .just(peer),
            connectionController: peer.connectionController,
            remoteConnectionStatus: connection.flatMapLatestForwardingNil { $0.remoteConnectionStatus },
            remoteConnectionConfiguration: connection.flatMapLatestForwardingNil { $0.remoteConnectionConfiguration },
            linkConfigurationViewControllerViewModel: linkConfigurationViewControllerViewModel
        )

        viewController.servicePlaygroundProvider = { descriptor, _ in
            switch descriptor {
            case .coap:
                let playgroundViewController = CoapPlaygroundViewController(style: .grouped)
                playgroundViewController.title = "CoAP playground"
                playgroundViewController.viewModel = peer
                return playgroundViewController
            case .blasting:
                let playgroundViewController = BlastingPlaygroundViewController(style: .grouped)
                playgroundViewController.title = "Blasting playground"
                playgroundViewController.viewModel = peer
                return playgroundViewController
            default: fatalError("Could not create a playground without a service")
            }
        }

        viewController.onForget = { peerManager.remove(peer) }
        navigationController!.pushViewController(viewController, animated: true)
    }
}

private extension ManagedPeerListViewController {
    func pairing() {
        let viewController = PairPeerViewController(style: .grouped)
        navigationController!.pushViewController(viewController, animated: true)
    }
}

private class Cell: UITableViewCell, ReusableTableViewCell {
    private let disposeBag = DisposeBag()
    
    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: .value1, reuseIdentifier: reuseIdentifier)
        accessoryType = .detailButton
    }

    required init?(coder aDecoder: NSCoder) {
        fatalError("not implemented")
    }

    func bind(_ peer: ManagedNode) {
        peer.name
            .asDriver()
            .drive(textLabel!.rx.text)
            .disposed(by: disposeBag)

        peer.connectionController.connectionStatus
            .asDriver(onErrorJustReturn: .disconnected)
            .map { $0.cellDescription }
            .drive(detailTextLabel!.rx.text)
            .disposed(by: disposeBag)

        peer.connectionController.connectionStatus
            .asDriver(onErrorJustReturn: .disconnected)
            .map { $0.connected ? UIColor.darkText : UIColor.lightGray }
            .drive(onNext: { [detailTextLabel] in
                detailTextLabel?.textColor = $0
            })
            .disposed(by: disposeBag)
    }
}
