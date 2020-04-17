//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ViewController.swift
//  GoldenGate-Host-iOS
//
//  Created by Bogdan Vlad on 9/24/17.
//

import GoldenGate
import RxBluetoothKit
import RxCocoa
import RxSwift
import UIKit

class RootViewController: UITableViewController {
    enum Section: Int {
        case connectivity
        case hostVersionInfo
        case ggVersionInfo

        static let count = 3
    }

    enum GoldenGateVersionInfoRow: Int {
        case majorMinorPatch
        case commitHash
        case branchName
        case date
        case commitCount

        static let count = 5
    }

    var peerManager: PeerManager<ManagedNode>!

    override func viewDidLoad() {
        super.viewDidLoad()

        // Eagerly load the component
        _ = Component.instance

        peerManager = Component.instance.peerManager
    }

    override func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section)! {
        case .connectivity:
            return "Connectivity"
        case .hostVersionInfo:
            return "Host Version Info"
        case .ggVersionInfo:
            return "GoldenGate Version Info"
        }
    }

    override func numberOfSections(in tableView: UITableView) -> Int {
        return Section.count
    }

    override func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section)! {
        case .connectivity:
            return 1
        case .hostVersionInfo:
            return 1
        case .ggVersionInfo:
            return GoldenGateVersionInfoRow.count
        }
    }

    override func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        switch Section(rawValue: indexPath.section)! {
        case .connectivity:
            return pairedPeersCell()
        case .hostVersionInfo:
            return appVersionInfoCell()
        case .ggVersionInfo:
            return xpVersionInfoCellForRow(GoldenGateVersionInfoRow(rawValue: indexPath.row)!)
        }
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        let viewController = ManagedPeerListViewController(style: .grouped)
        navigationController?.pushViewController(viewController, animated: true)
    }

    private func pairedPeersCell() -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: nil)

        cell.textLabel?.text = "Paired Peers"
        cell.accessoryType = .disclosureIndicator

        // Generate: "N / M" = N peers connected, M peers known
        peerManager.peers
            .flatMapLatest { known -> Observable<String> in
                let states = known.map { $0.connectionController.state.asObservable() }

                return Observable.combineLatest(states)
                    .map { $0.filter { $0 == .connected } }
                    .startWith([])
                    .map { connected in "\(connected.count) / \(known.count)" }
            }
            .asDriver(onErrorJustReturn: "")
            .drive(cell.detailTextLabel!.rx.text)
            .disposed(by: disposeBag)

        return cell
    }

    private func appVersionInfoCell() -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: nil)

        cell.textLabel?.text = "Version"

        let dictionary = Bundle.main.infoDictionary!
        let version = dictionary["CFBundleShortVersionString"] as? String ?? "??"
        let build = dictionary["CFBundleVersion"] as? String ?? "??"
        cell.detailTextLabel?.text = "\(version) (\(build))"

        return cell
    }

    private func xpVersionInfoCellForRow(_ row: GoldenGateVersionInfoRow) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
        let version = GoldenGateVersion.shared

        switch row {
        case .majorMinorPatch:
            cell.textLabel?.text = "Version"
            cell.detailTextLabel?.text = version.majorMinorPatch
        case .commitHash:
            cell.textLabel?.text = "Commit Hash"
            cell.detailTextLabel?.text = version.commitHash ?? "None"
        case .branchName:
            cell.textLabel?.text = "Branch Name"
            cell.detailTextLabel?.text = version.branchName ?? "None"
        case .date:
            cell.textLabel?.text = "Built"

            if let buildDate = version.buildDate {
                let formatter = DateFormatter()
                formatter.dateStyle = .medium
                formatter.timeStyle = .short
                cell.detailTextLabel?.text = formatter.string(from: buildDate)
            } else {
                cell.detailTextLabel?.text = "None"
            }
        case .commitCount:
            cell.textLabel?.text = "Commit Count"
            cell.detailTextLabel?.text = "\(version.commitCount)"
        }

        return cell
    }
}
