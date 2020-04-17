//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PairPeerViewController.swift
//  GoldenGateHost-iOS
//
//  Created by Marcel Jackwerth on 11/1/17.
//

import Foundation
import GoldenGate
import RxCocoa
import RxSwift
import UIKit

class PairPeerViewController: UITableViewController {
    var scanner: Hub.Scanner!
    var peerManager: PeerManager<ManagedNode>!

    override func viewDidLoad() {
        super.viewDidLoad()

        title = "Pair Client"

        scanner = Component.instance.scanner
        peerManager = Component.instance.peerManager

        let activityIndicator = UIActivityIndicatorView(style: .gray)
        navigationItem.rightBarButtonItem = UIBarButtonItem(customView: activityIndicator)

        tableView.dataSource = nil
        tableView.register(Cell.self, forCellReuseIdentifier: "Cell")

        tableView.rx.modelSelected(DiscoveredBluetoothPeer.self)
            // handle asynchronously to get a chance for scanner to stop established connections
            .observeOn(MainScheduler.asyncInstance)
            .subscribe(onNext: { [weak self, peerManager] peer in
                let descriptor = BluetoothPeerDescriptor(identifier: peer.identifier)
                _ = peerManager!.getOrCreate(bluetoothPeerDescriptor: descriptor, name: peer.name.value)
                self?.navigationController?.popViewController(animated: true)
            })
            .disposed(by: disposeBag)

        scanner.isScanning
            .asDriver()
            .drive(activityIndicator.rx.isAnimating)
            .disposed(by: disposeBag)

        scanner.isScanning
            .asDriver()
            .map { !$0 }
            .drive(activityIndicator.rx.isHidden)
            .disposed(by: disposeBag)

        scanner.peers
            .takeUntil(tableView.rx.modelSelected(DiscoveredBluetoothPeer.self).asObservable())
            .bind(to: tableView.rx.items) { tableView, _, peer in
                // swiftlint:disable:next force_cast
                let cell = tableView.dequeueReusableCell(withIdentifier: "Cell")! as! Cell
                cell.bind(peer)
                return cell
            }
            .disposed(by: disposeBag)
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
    }
}

private class Cell: UITableViewCell {
    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: .subtitle, reuseIdentifier: reuseIdentifier)
    }

    required init?(coder aDecoder: NSCoder) {
        fatalError("not implemented")
    }

    func bind(_ peer: DiscoveredBluetoothPeer) {
        peer.name
            .asDriver()
            .drive(textLabel!.rx.text)
            .disposed(by: disposeBag)

        peer.rssi
            .asDriver()
            .map { "Signal Strength: \($0)" }
            .drive(detailTextLabel!.rx.text)
            .disposed(by: disposeBag)
    }
}
