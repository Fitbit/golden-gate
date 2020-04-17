//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ServiceViewController.swift
//  GoldenGate-iOS
//
//  Created by Marcel Jackwerth on 4/26/18.
//

#if os(iOS)

import RxCocoa
import RxSwift
import UIKit

public protocol ServiceViewControllerViewModel {
    var serviceDescriptor: Observable<ServiceDescriptor> { get }
    func setCustomServiceDescriptor(_ descriptor: ServiceDescriptor?)

    var blasterConfiguration: Observable<BlasterService.Configuration> { get }
    func setBlasterPacketSize(_ packetSize: Int)

    var coapEndpoint: CoapEndpointType { get }
    var blasterService: BlasterService { get }
}

class ServiceViewController: UITableViewController {
    public var viewModel: ServiceViewControllerViewModel!

    private let stats = BehaviorRelay<TrafficPerformanceStats>(value: TrafficPerformanceStats())

    override public func viewDidLoad() {
        super.viewDidLoad()

        // Only monitor stats when service is blasting
        viewModel.serviceDescriptor
            .flatMapLatest { [blasterService = viewModel.blasterService] descriptor -> Observable<TrafficPerformanceStats> in
                guard descriptor == .blasting else { return .empty() }

                return blasterService.stats
            }
            .subscribe(onNext: stats.accept)
            .disposed(by: disposeBag)
    }

    override public func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section)! {
        case .settings: return "Settings"
        case .coap: return "CoAP"
        case .outgoingBlast: return "Blast (Outgoing)"
        case .incomingBlast: return "Blast (Incoming)"
        }
    }

    override public func numberOfSections(in tableView: UITableView) -> Int {
        return Section.count
    }

    override public func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section)! {
        case .settings: return SettingRow.count
        case .coap: return CoapRow.count
        case .outgoingBlast: return OutgoingBlastRow.count
        case .incomingBlast: return IncomingBlastRow.count
        }
    }

    override public func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        switch Section(rawValue: indexPath.section)! {
        case .settings: return cell(forRow: SettingRow(rawValue: indexPath.row)!)
        case .coap: return cell(forRow: CoapRow(rawValue: indexPath.row)!)
        case .outgoingBlast: return cell(forRow: OutgoingBlastRow(rawValue: indexPath.row)!)
        case .incomingBlast: return cell(forRow: IncomingBlastRow(rawValue: indexPath.row)!)
        }
    }

    override public func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        switch Section(rawValue: indexPath.section)! {
        case .settings:
            presentServiceDescriptorViewController()
        case .coap:
            tableView.deselectRow(at: indexPath, animated: true)
            switch CoapRow(rawValue: indexPath.item)! {
            case .performGetRequest:
                sendCoapRequest(method: .get)
            case .performPutRequest:
                sendCoapRequest(method: .put)
            }

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

private extension ServiceViewController {
    func cell(forRow row: SettingRow) -> UITableViewCell {
        let cell: UITableViewCell

        switch row {
        case .serviceDescriptor:
            cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
            cell.textLabel?.text = "Service"

            viewModel.serviceDescriptor
                .map { $0.rawValue }
                .asDriver(onErrorJustReturn: "")
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)

            cell.accessoryType = .disclosureIndicator
        }

        return cell
    }

    func cell(forRow row: CoapRow) -> UITableViewCell {
        let cell: UITableViewCell

        switch row {
        case .performGetRequest:
            cell = UITableViewCell(style: .default, reuseIdentifier: nil)
            cell.textLabel?.text = "CoAP Request: GET /helloworld"
            cell.textLabel?.textColor = self.view.tintColor
        case .performPutRequest:
            cell = UITableViewCell(style: .default, reuseIdentifier: nil)
            cell.textLabel?.text = "CoAP Request: PUT /helloworld"
            cell.textLabel?.textColor = self.view.tintColor
        }

        return cell
    }

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

private extension ServiceViewController {
    func presentServiceDescriptorViewController() {
        let viewController = ComboBoxViewController<ServiceDescriptor>(style: .grouped)
        viewController.title = "Service"
        viewController.viewModel = ComboBoxViewModel<ServiceDescriptor>(
            elements: [
                ServiceDescriptor.none,
                .coap,
                .blasting
            ],
            selectedElement: viewModel.serviceDescriptor,
            setSelectedElement: viewModel.setCustomServiceDescriptor
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    func presentBlasterPacketSizeViewController() {
        let viewController = PickerViewController<Int>()
        viewController.title = "Packet Size"
        viewController.viewModel = ComboBoxViewModel<Int>(
            elements: [
                30, 100, 150, 250, 500, 1000
            ],
            selectedElement: viewModel.blasterConfiguration.map { $0.packetSize },
            setSelectedElement: viewModel.setBlasterPacketSize
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    func sendCoapRequest(method: CoapCode.Request) {
        let time = Date()
        let requestBuilder = CoapRequestBuilder()
            .method(method)
            .path(["helloworld"])

        switch method {
        case .put, .post:
            requestBuilder.body(data: UIDevice.current.name.data(using: .utf8)!)
        case .get, .delete:
            break
        }

        viewModel
            .coapEndpoint
            .response(request: requestBuilder.build())
            .flatMap { $0.body.asData() }
            .timeout(.seconds(5), scheduler: MainScheduler.instance)
            .observeOn(MainScheduler.instance)
            .subscribe(
                onSuccess: { [weak self] response in
                    let message = String(data: response, encoding: .utf8) ?? "ENCODING ERROR"
                    let responseTime = String(format: "%.2f s.", Date().timeIntervalSince(time))

                    let alert = UIAlertController(
                        title: "CoAP Response received after \(responseTime)",
                        message: message,
                        preferredStyle: .alert
                    )

                    alert.addAction(UIAlertAction(title: "OK", style: .default, handler: nil))
                    self?.present(alert, animated: true, completion: nil)
                }, onError: { [weak self] error in
                    let alert = UIAlertController(
                        title: "CoAP Response Error",
                        message: "\(error)",
                        preferredStyle: .alert
                    )

                    alert.addAction(UIAlertAction(title: "OK", style: .default, handler: nil))
                    self?.present(alert, animated: true, completion: nil)
                }
            )
            .disposed(by: disposeBag)
    }
}

private enum Section: Int {
    case settings
    case coap
    case outgoingBlast
    case incomingBlast

    static var count = 4
}

private enum SettingRow: Int {
    case serviceDescriptor

    static var count = 1
}

private enum CoapRow: Int {
    case performGetRequest
    case performPutRequest

    static var count = 2
}

private enum OutgoingBlastRow: Int {
    case blast
    case packetSize

    static var count = 2
}

private enum IncomingBlastRow: Int {
    case packetsReceived
    case bytesReceived
    case throughput
    case lastReceivedCounter
    case nextExpectedCounter
    case gapCount

    static var count = 6
}

#endif
