//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CommonPeerViewController.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 11/3/17.
//

#if os(iOS)

import RxCocoa
import RxSwift
import UIKit

public protocol CommonPeerViewControllerViewModel {
    var connectionController: DefaultConnectionController { get }

    var stackDescription: Driver<String> { get }
    var stackDescriptor: Observable<StackDescriptor> { get }
    func setCustomStackDescriptor(_ descriptor: StackDescriptor?)

    var serviceDescriptor: Observable<ServiceDescriptor> { get }
    func setCustomServiceDescriptor(_ descriptor: ServiceDescriptor?)

    var connectionIsUserControllable: Driver<Bool> { get }
    func setUserWantsToConnect(_ connect: Bool)
    var userWantsToConnect: Driver<Bool> { get }

    var configureStackViewControllerViewModel: ConfigureStackViewControllerViewModel { get }
    var serviceViewControllerViewModel: ServiceViewControllerViewModel { get }

    var connectionDetails: [(label: String, value: Driver<String?>, isChecked: Driver<Bool>)] { get }
    var networkLinkStatusDetails: [(label: String, value: Driver<String?>)] { get }
    var connectionStatusDetails: [(label: String, value: Driver<String?>)] { get }
    var connectionConfigurationDetails: [(label: String, value: Driver<String?>)] { get }
    var batteryLevelDetails: [(label: String, value: Driver<String?>)] { get }
    var descriptorDetails: [(label: String, value: Driver<String?>)] { get }

    var linkConfigurationViewControllerViewModel: LinkConfigurationViewControllerViewModel { get }
}

public class CommonPeerViewController: UITableViewController {
    public typealias ViewModel = CommonPeerViewControllerViewModel

    public var viewModel: ViewModel!
    public var onForget: (() -> Void)?

    private let blasterService = BehaviorRelay<BlasterService?>(value: nil)
    private let coapEndpoint = BehaviorRelay<CoapEndpoint?>(value: nil)

    override public func viewDidLoad() {
        super.viewDidLoad()

        // Warn if stack changed
        viewModel.stackDescriptor
            .distinctUntilChanged()
            .skip(1)
            .delay(.milliseconds(500), scheduler: MainScheduler())
            .asDriver(onErrorJustReturn: .dtlsSocketNetifGattlink)
            .drive(onNext: { [unowned self] _ in self.presentStackChangeWarning() })
            .disposed(by: disposeBag)
    }

    override public func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section)! {
        case .settings: return "Settings"
        case .connection: return "Connection"
        case .networkLinkStatus: return "Network Link Status"
        case .connectionStatus: return "Connection Status"
        case .connectionConfiguration: return "Connection Configuration"
        case .batteryLevel: return "Battery"
        case .descriptor: return "Descriptor"
        case .actions: return nil
        }
    }

    override public func numberOfSections(in tableView: UITableView) -> Int {
        return Section.count
    }

    override public func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section)! {
        case .settings: return SettingRow.count
        case .connection: return viewModel.connectionDetails.count
        case .networkLinkStatus: return viewModel.networkLinkStatusDetails.count
        case .connectionStatus: return viewModel.connectionStatusDetails.count
        case .connectionConfiguration: return viewModel.connectionConfigurationDetails.count
        case .batteryLevel: return viewModel.batteryLevelDetails.count
        case .descriptor: return viewModel.descriptorDetails.count
        case .actions:
            return (onForget == nil ? 1 : 2)
        }
    }

    override public func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        switch Section(rawValue: indexPath.section)! {
        case .settings:
            return settingCell(forRow: indexPath.row)
        case .connection:
            return connectionCell(forRow: indexPath.row)
        case .networkLinkStatus:
            return networkLinkStatusCell(forRow: indexPath.row)
        case .connectionStatus:
            return connectionStatusCell(forRow: indexPath.row)
        case .connectionConfiguration:
            return connectionConfigurationCell(forRow: indexPath.row)
        case .batteryLevel:
            return batteryCell(forRow: indexPath.row)
        case .descriptor:
            return descriptorCell(forRow: indexPath.row)
        case .actions:
            return actionCell(forRow: indexPath.row)
        }
    }

    func settingCell(forRow row: Int) -> UITableViewCell {
        let cell: UITableViewCell

        switch SettingRow(rawValue: row)! {
        case .autoConnect:
            cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
            cell.selectionStyle = .none
            cell.textLabel?.text = "Connect"

            viewModel.connectionController.state.asDriver(onErrorJustReturn: .disconnected)
                .map { "\($0)" }
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)

            let aSwitch = UISwitch()
            cell.accessoryView = aSwitch

            aSwitch.rx.value
                .asDriver()
                .skip(1)
                .drive(onNext: viewModel.setUserWantsToConnect)
                .disposed(by: disposeBag)

            viewModel.connectionIsUserControllable
                .drive(aSwitch.rx.isEnabled)
                .disposed(by: disposeBag)

            viewModel
                .userWantsToConnect
                .drive(aSwitch.rx.isOn)
                .disposed(by: disposeBag)

        case .stackDescriptor:
            cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
            cell.textLabel?.text = "Stack"

            viewModel.stackDescription
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)

            cell.accessoryType = .disclosureIndicator

        case .services:
            cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
            cell.textLabel?.text = "Services"

            viewModel.serviceDescriptor
                .map { $0.rawValue }
                .asDriver(onErrorJustReturn: "")
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)

            cell.accessoryType = .disclosureIndicator

        case .linkConfiguration:
            cell = UITableViewCell(style: .default, reuseIdentifier: nil)
            cell.textLabel?.text = "Link Preferred Configuration"
            cell.accessoryType = .disclosureIndicator
        }

        return cell
    }

    func connectionCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.connectionDetails[row]

        let cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
        cell.selectionStyle = .none
        cell.textLabel?.text = rowViewModel.label

        rowViewModel.value
            .drive(cell.detailTextLabel!.rx.text)
            .disposed(by: disposeBag)

        rowViewModel.isChecked
            .drive(cell.rx.checked)
            .disposed(by: disposeBag)

        return cell
    }

    func networkLinkStatusCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.networkLinkStatusDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    func connectionStatusCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.connectionStatusDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    func connectionConfigurationCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.connectionConfigurationDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    func batteryCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.batteryLevelDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    func descriptorCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.descriptorDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    func detailCell(label: String, value: Driver<String?>) -> UITableViewCell {
        let cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        cell.selectionStyle = .none
        cell.textLabel?.text = label

        value
            .drive(cell.detailTextLabel!.rx.text)
            .disposed(by: disposeBag)

        return cell
    }

    func actionCell(forRow row: Int) -> UITableViewCell {
        let tintColor = self.view.tintColor

        let cell = UITableViewCell(style: .default, reuseIdentifier: nil)
        cell.textLabel?.textColor = tintColor

        switch ActionRow(rawValue: row)! {
        case .disconnect:
            cell.textLabel?.text = "Force Disconnect"
            viewModel.connectionController.state.asDriver(onErrorJustReturn: .disconnected)
                .map { $0 == .disconnected ? UIColor.gray : tintColor }
                .drive(cell.textLabel!.rx.textColor)
                .disposed(by: disposeBag)
        case .forget:
            cell.textLabel?.text = "Forget This Device"
        }

        return cell
    }

    override public func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        switch Section(rawValue: indexPath.section)! {
        case .settings:
            switch SettingRow(rawValue: indexPath.row)! {
            case .autoConnect: break
            case .stackDescriptor:
                presentStackDescriptorViewController()
            case .services:
                presentServiceViewController()
            case .linkConfiguration:
                presentLinkConfigurationViewController()
            }
        case .connection: break
        case .networkLinkStatus: break
        case .connectionStatus: break
        case .connectionConfiguration: break
        case .batteryLevel: break
        case .descriptor: break
        case .actions:
            switch ActionRow(rawValue: indexPath.row)! {
            case .disconnect:
                viewModel.connectionController.forceDisconnect()
            case .forget:
                forget()
            }
        }

        tableView.deselectRow(at: indexPath, animated: true)
    }
}

private extension CommonPeerViewController {
    func presentStackDescriptorViewController() {
        let viewController = ConfigureStackViewController(style: .grouped)
        viewController.childViewModel = viewModel.configureStackViewControllerViewModel
        viewController.title = "Stack"
        viewController.viewModel = ComboBoxViewModel<StackDescriptor>(
            elements: [
                .dtlsSocketNetifGattlinkActivity,
                .dtlsSocketNetifGattlink,
                .socketNetifGattlink,
                .netifGattlink,
                .gattlink
            ],
            selectedElement: viewModel.stackDescriptor,
            setSelectedElement: viewModel.setCustomStackDescriptor
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    func presentServiceViewController() {
        let viewController = ServiceViewController(style: .grouped)
        viewController.title = "Services"
        viewController.viewModel = self.viewModel.serviceViewControllerViewModel
        navigationController?.pushViewController(viewController, animated: true)
    }

    func presentLinkConfigurationViewController() {
        let viewController = LinkConfigurationViewController(style: .grouped)
        viewController.title = "Link Configuration"
        viewController.viewModel = self.viewModel.linkConfigurationViewControllerViewModel
        navigationController?.pushViewController(viewController, animated: true)
    }

    func forget() {
        let controller = UIAlertController(title: nil, message: nil, preferredStyle: .actionSheet)

        controller.addAction(UIAlertAction(title: "Forget", style: .destructive) { [weak self] _ in
            guard let `self` = self else { return }
            self.onForget?()
            self.navigationController?.popViewController(animated: true)
        })

        controller.addAction(UIAlertAction(title: "Cancel", style: .cancel))

        present(controller, animated: true)
    }

    func presentStackChangeWarning() {
        let controller = UIAlertController(
            title: "Recommendation",
            message: "After changing the stack the Gattlink/DTLS session might be stuck.\n\nTry disconnecting from the device.",
            preferredStyle: .alert
        )

        controller.addAction(UIAlertAction(title: "Okay", style: .default))
        navigationController?.present(controller, animated: true)
    }
}

private enum Section: Int {
    case settings
    case connection
    case networkLinkStatus
    case connectionStatus
    case connectionConfiguration
    case batteryLevel
    case descriptor
    case actions

    static var count = 8
}

private enum SettingRow: Int {
    case autoConnect
    case stackDescriptor
    case services
    case linkConfiguration

    static var count = 4
}

private enum ActionRow: Int {
    case disconnect
    case forget
}

#endif
