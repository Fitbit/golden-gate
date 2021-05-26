//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  BlastingPlaygroundViewController.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 27/07/2020.
//

#if os(iOS)

import BluetoothConnection
import RxCocoa
import RxSwift
import UIKit

public protocol BlastingPlaygroundViewControllerViewModel {
    var blasterConfiguration: Observable<BlasterService.Configuration> { get }
    func setBlasterPacketSize(_ packetSize: Int)

    var blasterService: BlasterService { get }
}

public final class BlastingPlaygroundViewController: UITableViewController {
    public var viewModel: BlastingPlaygroundViewControllerViewModel!

    private let stats = BehaviorRelay<TrafficPerformanceStats>(value: TrafficPerformanceStats())
    private let disposeBag = DisposeBag()

    override public func viewDidLoad() {
        super.viewDidLoad()

        viewModel.blasterService.stats
            .subscribe(onNext: stats.accept)
            .disposed(by: disposeBag)
    }

    override public func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section)! {
        case .outgoingBlast: return "Blast (Outgoing)"
        case .incomingBlast: return "Blast (Incoming)"
        }
    }

    override public func numberOfSections(in tableView: UITableView) -> Int {
        return Section.allCases.count
    }

    override public func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section)! {
        case .outgoingBlast: return OutgoingBlastRow.allCases.count
        case .incomingBlast: return IncomingBlastRow.allCases.count
        }
    }

    override public func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        switch Section(rawValue: indexPath.section)! {
        case .outgoingBlast: return cell(forRow: OutgoingBlastRow(rawValue: indexPath.row)!)
        case .incomingBlast: return cell(forRow: IncomingBlastRow(rawValue: indexPath.row)!)
        }
    }

    override public func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        switch Section(rawValue: indexPath.section)! {
        case .outgoingBlast:
            switch OutgoingBlastRow(rawValue: indexPath.row)! {
            case .blast:
                break
            case .packetSize:
                presentBlasterPacketSizeViewController()
            }
        case .incomingBlast:
            break
        }
    }
}

private extension BlastingPlaygroundViewController {
    func cell(forRow row: OutgoingBlastRow) -> UITableViewCell {
        let cell: UITableViewCell

        switch row {
        case .blast:
            cell = UITableViewCell(style: .default, reuseIdentifier: nil)
            cell.selectionStyle = .none
            cell.textLabel?.text = "Blast"

            let aSwitch = UISwitch()
            let blasterService = viewModel.blasterService

            cell.accessoryView = aSwitch

            // Switch is ON if blaster service is blasting
            blasterService.isBlasting
                .asDriver(onErrorJustReturn: false)
                .drive(aSwitch.rx.isOn)
                .disposed(by: disposeBag)

            // Blast if switch is ON
            aSwitch.rx.isOn.asDriver()
                .skip(1)
                .drive(onNext: { value in
                    blasterService.shouldBlast.accept(value)
                })
                .disposed(by: disposeBag)

        case .packetSize:
            cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
            cell.textLabel?.text = "Packet Size"

            viewModel.blasterConfiguration
                .map { $0.packetSize }
                .map(String.init(describing:))
                .asDriver(onErrorJustReturn: "")
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)
        }

        return cell
    }

    func cell(forRow row: IncomingBlastRow) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
        cell.selectionStyle = .none

        func bind(_ label: String, mapping: @escaping (TrafficPerformanceStats) -> Any) {
            cell.textLabel!.text = label

            stats.asDriver()
                .map(mapping)
                .map(String.init(describing:))
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)
        }

        switch row {
        case .packetsReceived:
            bind("Packets Received") { $0.packetsReceived }
        case .bytesReceived:
            bind("Bytes Received") { String(format: "%d kB", Int(Double($0.bytesReceived) / 1000)) }
        case .throughput:
            bind("Throughput") { String(format: "%.2f kBps", Double($0.throughput) / 1000) }
        case .lastReceivedCounter:
            bind("Last Received Counter") { $0.lastReceivedCounter }
        case .nextExpectedCounter:
            bind("Next Expected Counter") { $0.nextExpectedCounter }
        case .gapCount:
            bind("Gap Count") { $0.gapCount }
        }

        return cell
    }
}

private extension BlastingPlaygroundViewController {
    func presentBlasterPacketSizeViewController() {
        let viewController = PickerViewController<Int>()
        viewController.title = "Packet Size"
        viewController.viewModel = ComboBoxViewModel<Int>(
            elements: .just([
                30, 100, 150, 250, 500, 1000
            ]),
            selectedElement: viewModel.blasterConfiguration.map { $0.packetSize },
            setSelectedElement: viewModel.setBlasterPacketSize
        )
        navigationController?.pushViewController(viewController, animated: true)
    }
}

private enum Section: Int, CaseIterable {
    case outgoingBlast
    case incomingBlast
}

private enum OutgoingBlastRow: Int, CaseIterable {
    case blast
    case packetSize
}

private enum IncomingBlastRow: Int, CaseIterable {
    case packetsReceived
    case bytesReceived
    case throughput
    case lastReceivedCounter
    case nextExpectedCounter
    case gapCount
}

extension GGPeer: BlastingPlaygroundViewControllerViewModel {}

#endif
