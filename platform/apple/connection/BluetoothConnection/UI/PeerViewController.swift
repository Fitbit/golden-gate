//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  PeerViewController.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 11/3/17.
//

#if os(iOS)

import RxCocoa
import RxSwift
import UIKit

// swiftlint:disable file_length

public protocol PeerViewControllerViewModel {
    associatedtype ConnectionType: LinkConnection

    var peer: Observable<Peer> { get }

    var connectionStatus: Observable<ConnectionStatus<ConnectionType>> { get }
    func connect(trigger: ConnectionTrigger)
    func disconnect(trigger: ConnectionTrigger)

    var stackDescription: Driver<String> { get }
    var stackDescriptor: Observable<StackDescriptor> { get }
    func setCustomStackDescriptor(_ descriptor: StackDescriptor?)
    var supportedStackDescriptors: Observable<[StackDescriptor]> { get }

    var serviceDescriptor: Observable<ServiceDescriptor> { get }
    func setCustomServiceDescriptor(_ descriptor: ServiceDescriptor?)
    var supportedServiceDescriptors: Observable<[ServiceDescriptor]> { get }

    var configureStackViewControllerViewModel: ConfigureStackViewControllerViewModel { get }

    var connectionDetails: [(label: String, value: Driver<String?>, isChecked: Driver<Bool>)] { get }
    var networkLinkStatusDetails: [(label: String, value: Driver<String?>)] { get }
    var connectionStatusDetails: [(label: String, value: Driver<String?>)] { get }
    var connectionConfigurationDetails: [(label: String, value: Driver<String?>)] { get }
    var descriptorDetails: [(label: String, value: Driver<String?>)] { get }

    var linkConfigurationViewControllerViewModel: LinkConfigurationViewControllerViewModel { get }
}

public class PeerViewController<ViewModel: PeerViewControllerViewModel>: UITableViewController {
    public var viewModel: ViewModel!
    public var servicePlaygroundProvider: ((_ descriptor: ServiceDescriptor, _ peer: Peer) -> UITableViewController)!
    public var onForget: (() -> Void)?

    private let toggleConnection = PublishSubject<Void>()
    private let disposeBag = DisposeBag()

    override public func viewDidLoad() {
        super.viewDidLoad()

        // Warn if stack changed
        viewModel.stackDescriptor
            .distinctUntilChanged()
            .skip(1)
            .delay(.milliseconds(500), scheduler: MainScheduler())
            .asDriver(onErrorJustReturn: .empty)
            .drive(onNext: { [unowned self] _ in self.presentStackChangeWarning() })
            .disposed(by: disposeBag)

        toggleConnection.withLatestFrom(viewModel.connectionStatus)
            .subscribe(onNext: { [viewModel] status in
                let trigger: ConnectionTrigger = "admin"
                if status == .disconnected {
                    viewModel?.connect(trigger: trigger)
                } else {
                    viewModel?.disconnect(trigger: trigger)
                }
            })
            .disposed(by: disposeBag)
    }

    override public func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section)! {
        case .connection: return "Connection"
        case .settings: return "Settings"
        case .servicePlayground: return "Service Playground"
        case .networkLinkStatus: return "Network Link Status"
        case .connectionStatus: return "Connection Status"
        case .connectionConfiguration: return "Connection Configuration"
        case .descriptor: return "Descriptor"
        case .actions: return nil
        }
    }

    override public func numberOfSections(in tableView: UITableView) -> Int {
        return Section.allCases.count
    }

    override public func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section)! {
        case .connection: return ConnectionRow.allCases.count
        case .settings: return SettingRow.allCases.count
        case .servicePlayground: return 1
        case .networkLinkStatus: return viewModel.networkLinkStatusDetails.count
        case .connectionStatus: return viewModel.connectionStatusDetails.count
        case .connectionConfiguration: return viewModel.connectionConfigurationDetails.count
        case .descriptor: return viewModel.descriptorDetails.count
        case .actions:
            return (onForget == nil ? 0 : 1)
        }
    }

    override public func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        switch Section(rawValue: indexPath.section)! {
        case .connection:
            return connectionCell(forRow: indexPath.row)
        case .settings:
            return settingCell(forRow: indexPath.row)
        case .servicePlayground:
            return servicePlaygroundCell(forRow: indexPath.row)
        case .networkLinkStatus:
            return networkLinkStatusCell(forRow: indexPath.row)
        case .connectionStatus:
            return connectionStatusCell(forRow: indexPath.row)
        case .connectionConfiguration:
            return connectionConfigurationCell(forRow: indexPath.row)
        case .descriptor:
            return descriptorCell(forRow: indexPath.row)
        case .actions:
            return actionCell(forRow: indexPath.row)
        }
    }

    private func connectionCell(forRow row: Int) -> UITableViewCell {
        let cell: UITableViewCell

        switch ConnectionRow(rawValue: row)! {
        case .connectionStatus:
            let rowViewModel = viewModel.connectionDetails[row]

            cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
            cell.selectionStyle = .none
            cell.textLabel?.text = rowViewModel.label

            rowViewModel.value
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)

            rowViewModel.isChecked
                .drive(cell.rx.checked)
                .disposed(by: disposeBag)
        case .toggleConnection:
            cell = UITableViewCell(style: .default, reuseIdentifier: nil)
            cell.textLabel?.textColor = view.tintColor

            viewModel.connectionStatus.asDriver(onErrorJustReturn: .disconnected)
                .map { $0 == .disconnected ? "Connect" : "Disconnect" }
                .drive(cell.textLabel!.rx.text)
                .disposed(by: disposeBag)
        }

        return cell
    }

    private func settingCell(forRow row: Int) -> UITableViewCell {
        let cell: UITableViewCell

        switch SettingRow(rawValue: row)! {
        case .stackDescriptor:
            cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
            cell.textLabel?.text = "Stack"

            viewModel.stackDescription
                .drive(cell.detailTextLabel!.rx.text)
                .disposed(by: disposeBag)

            cell.accessoryType = .disclosureIndicator

        case .serviceDescriptor:
            cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
            cell.textLabel?.text = "Service"

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

    private func servicePlaygroundCell(forRow row: Int) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: nil)

        viewModel.serviceDescriptor
            .map { descriptor in
                switch descriptor {
                case .none: return "No service available"
                default: return descriptor.rawValue
                }
            }
            .asDriver(onErrorJustReturn: "")
            .drive(cell.textLabel!.rx.text)
            .disposed(by: disposeBag)

        let userInteractionEnabled = viewModel.serviceDescriptor
            .map { $0 != .none }
            .asDriver(onErrorJustReturn: false)

        userInteractionEnabled.drive(cell.rx.isUserInteractionEnabled)
            .disposed(by: disposeBag)

        userInteractionEnabled
            .map { $0 ? 1 : 0.5 }
            .drive(cell.textLabel!.rx.alpha)
            .disposed(by: disposeBag)

        cell.accessoryType = .disclosureIndicator

        return cell
    }

    private func networkLinkStatusCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.networkLinkStatusDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    private func connectionStatusCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.connectionStatusDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    private func connectionConfigurationCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.connectionConfigurationDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    private func descriptorCell(forRow row: Int) -> UITableViewCell {
        let rowViewModel = viewModel.descriptorDetails[row]
        return detailCell(label: rowViewModel.label, value: rowViewModel.value)
    }

    private func detailCell(label: String, value: Driver<String?>) -> UITableViewCell {
        let cell = UITableViewCell(style: .subtitle, reuseIdentifier: nil)
        cell.selectionStyle = .none
        cell.textLabel?.text = label

        value
            .drive(cell.detailTextLabel!.rx.text)
            .disposed(by: disposeBag)

        return cell
    }

    private func actionCell(forRow row: Int) -> UITableViewCell {
        let tintColor = self.view.tintColor

        let cell = UITableViewCell(style: .default, reuseIdentifier: nil)
        cell.textLabel?.textColor = tintColor

        switch ActionRow(rawValue: row)! {
        case .forget:
            cell.textLabel?.text = "Forget This Device"
        }

        return cell
    }

    override public func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        switch Section(rawValue: indexPath.section)! {
        case .connection:
            switch ConnectionRow(rawValue: indexPath.row)! {
            case .connectionStatus:
                break
            case .toggleConnection:
                toggleConnection.onNext(())
            }
        case .settings:
            switch SettingRow(rawValue: indexPath.row)! {
            case .stackDescriptor:
                presentStackDescriptorViewController()
            case .serviceDescriptor:
                presentServiceDescriptorViewController()
            case .linkConfiguration:
                presentLinkConfigurationViewController()
            }
        case .servicePlayground:
            presentServicePlaygroundViewController()
        case .networkLinkStatus: break
        case .connectionStatus: break
        case .connectionConfiguration: break
        case .descriptor: break
        case .actions:
            switch ActionRow(rawValue: indexPath.row)! {
            case .forget:
                forget()
            }
        }

        tableView.deselectRow(at: indexPath, animated: true)
    }
}

private extension PeerViewController {
    func presentStackDescriptorViewController() {
        let viewController = ConfigureStackViewController(style: .grouped)
        viewController.childViewModel = viewModel.configureStackViewControllerViewModel
        viewController.title = "Stack"
        viewController.viewModel = ComboBoxViewModel<StackDescriptor>(
            elements: viewModel.supportedStackDescriptors,
            selectedElement: viewModel.stackDescriptor,
            setSelectedElement: viewModel.setCustomStackDescriptor
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    func presentServiceDescriptorViewController() {
        let viewController = ComboBoxViewController<ServiceDescriptor>(style: .grouped)
        viewController.title = "Service"
        viewController.viewModel = ComboBoxViewModel<ServiceDescriptor>(
            elements: viewModel.supportedServiceDescriptors,
            selectedElement: viewModel.serviceDescriptor,
            setSelectedElement: viewModel.setCustomServiceDescriptor
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    func presentLinkConfigurationViewController() {
        let viewController = LinkConfigurationViewController(style: .grouped)
        viewController.title = "Link Configuration"
        viewController.viewModel = self.viewModel.linkConfigurationViewControllerViewModel
        navigationController?.pushViewController(viewController, animated: true)
    }

    func presentServicePlaygroundViewController() {
        _ = Observable.combineLatest(viewModel.serviceDescriptor, viewModel.peer)
            .take(1)
            .map { [unowned self] in self.servicePlaygroundProvider($0.0, $0.1) }
            .subscribe(onNext: { [navigationController] playground in
                navigationController?.pushViewController(playground, animated: true)
            })
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

private enum Section: Int, CaseIterable {
    case connection
    case settings
    case servicePlayground
    case networkLinkStatus
    case connectionStatus
    case connectionConfiguration
    case descriptor
    case actions
}

private enum ConnectionRow: Int, CaseIterable {
    case connectionStatus
    case toggleConnection
}

private enum SettingRow: Int, CaseIterable {
    case stackDescriptor
    case serviceDescriptor
    case linkConfiguration
}

private enum ActionRow: Int {
    case forget
}

extension ServiceDescriptor: Hashable {}

#endif
