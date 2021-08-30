//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PairPeerViewController.swift
//  GoldenGateHost-iOS
//
//  Created by Marcel Jackwerth on 11/1/17.
//

import BluetoothConnection
import Foundation
import GoldenGate
import RxCocoa
import RxSwift
import UIKit

class PairPeerViewController: UITableViewController {
    var discoverer: PeerDiscoverer!
    var peerManager: PeerManager<ManagedNode>!

    private let disposeBag = DisposeBag()

    override func viewDidLoad() {
        super.viewDidLoad()

        title = "Pair Client"

        discoverer = Component.instance.discoverer
        peerManager = Component.instance.peerManager

        let activityIndicator = UIActivityIndicatorView(style: .gray)
        navigationItem.rightBarButtonItem = UIBarButtonItem(customView: activityIndicator)

        tableView.dataSource = nil
        tableView.register(Cell.self, forCellReuseIdentifier: "Cell")

        tableView.rx.modelSelected(DiscoveredPeer.self)
            // handle asynchronously to get a chance for scanner to stop established connections
            .observeOn(MainScheduler.asyncInstance)
            .subscribe(onNext: { [weak self, peerManager] peer in
                let descriptor = PeerDescriptor(identifier: peer.identifier)
                _ = peerManager!.getOrCreate(peerDescriptor: descriptor, name: peer.name)
                self?.navigationController?.popViewController(animated: true)
            })
            .disposed(by: disposeBag)

        let peers = discoverer
            // Look for multiple UUIDs, as the link service supports multiple configurations
            .peerList(
                withServices: BluetoothConfiguration.default.linkService.map { $0.serviceUUID },
                peerFilter: { Component.instance.peerManager.get(peerDescriptor: PeerDescriptor(identifier: $0)) == nil }
            )
            .share()

        let isDiscovering = peers.map { $0 != nil }
            .distinctUntilChanged()
            .asDriver(onErrorJustReturn: false)


        isDiscovering
            .drive(activityIndicator.rx.isAnimating)
            .disposed(by: disposeBag)

        isDiscovering
            .map { !$0 }
            .drive(activityIndicator.rx.isHidden)
            .disposed(by: disposeBag)

        peers.map { $0 ?? [] }
            .takeUntil(tableView.rx.modelSelected(DiscoveredPeer.self).asObservable())
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
    private let disposeBag = DisposeBag()
    
    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: .subtitle, reuseIdentifier: reuseIdentifier)
    }

    required init?(coder aDecoder: NSCoder) {
        fatalError("not implemented")
    }

    func bind(_ peer: DiscoveredPeerType) {
        textLabel?.text = peer.name

        peer.rssi
            .asDriver(onErrorJustReturn: 0)
            .map { "Signal Strength: \($0)" }
            .drive(detailTextLabel!.rx.text)
            .disposed(by: disposeBag)
    }
}
