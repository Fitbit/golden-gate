//
//  LinkConfigurationViewController.swift
//  GoldenGate-iOS
//
//  Created by Sylvain Rebaud on 10/21/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

#if os(iOS)

import RxCocoa
import RxSwift
import UIKit

// swiftlint:disable file_length

public class LinkConfigurationViewControllerViewModel {
    // Inputs
    // A BehaviorRelaphy which emits the "Edit" state events.
    public let edit = BehaviorRelay<Bool>(value: false)
    // A PublishSubject which emits the "Done" button events.
    public let done = PublishSubject<Void>()
    // A PublishSubject which emits the "Cancel" button events.
    public let cancel = PublishSubject<Void>()
    
    // Outputs
    // A BehaviorRelay which emits current connection configuration.
    public let currentConnectionConfiguration: BehaviorRelay<LinkConfigurationService.PreferredConnectionConfiguration>
    // A BehaviorRelay which emits current connection mode.
    public let currentConnectionMode: BehaviorRelay<LinkConfigurationService.PreferredConnectionMode>
    
    // Private
    private let disposeBag = DisposeBag()

    public init(
        preferredConnectionConfiguration: BehaviorRelay<LinkConfigurationService.PreferredConnectionConfiguration>,
        preferredConnectionMode: BehaviorRelay<LinkConfigurationService.PreferredConnectionMode>
    ) {
        currentConnectionConfiguration = BehaviorRelay(value: preferredConnectionConfiguration.value)
        currentConnectionMode = BehaviorRelay(value: preferredConnectionMode.value)

        // Restore connection configuration when cancel fires
        cancel
            .subscribe(onNext: { [currentConnectionConfiguration, currentConnectionMode] in
                currentConnectionConfiguration.accept(preferredConnectionConfiguration.value)
                currentConnectionMode.accept(preferredConnectionMode.value)
            })
            .disposed(by: disposeBag)

        // Save current configuration when done fires
        done
            .subscribe(onNext: { [currentConnectionConfiguration, currentConnectionMode] in
                preferredConnectionConfiguration.accept(currentConnectionConfiguration.value)
                preferredConnectionMode.accept(currentConnectionMode.value)
            })
            .disposed(by: disposeBag)

        // Exit edit mode
        Observable.merge(cancel, done)
            .subscribe(onNext: { [edit] in
                edit.accept(false)
            })
            .disposed(by: disposeBag)
    }
    
}

private enum ViewModelEntry {
    case int(label: String, value: Driver<String?>, step: Int, min: Int, max: Int, update: (Int) -> Void)
    case double(label: String, value: Driver<String?>, step: Double, min: Double, max: Double, update: (Double) -> Void)
    case string(label: String, value: Driver<String?>, possbileValues: [String], update: (String) -> Void)
}

extension LinkConfigurationViewControllerViewModel {
    typealias PreferredConnectionConfiguration = LinkConfigurationService.PreferredConnectionConfiguration
    typealias ModeConfiguration = PreferredConnectionConfiguration.ModeConfiguration

    // swiftlint:disable:next function_body_length
    fileprivate func viewModelEntries(
        for configuration: Driver<ModeConfiguration?>,
        with keyPath: WritableKeyPath<PreferredConnectionConfiguration, ModeConfiguration?>,
        defaultValue: ModeConfiguration
    ) -> [ViewModelEntry] {
        let connectionIntervalValidRange = ModeConfiguration.connectionIntervalValidRange
        let supervisionTimeoutValidRange = ModeConfiguration.supervisionTimeoutValidRange
        let slaveLatencyValidRange = ModeConfiguration.slaveLatencyValidRange

        return [
            ViewModelEntry.double(
                label: "Min Connection Interval",
                value: configuration
                    .map { $0?.min }
                    .map { $0.map { "\(Double($0) * connectionIntervalValidRange.unit) ms" } },
                step: connectionIntervalValidRange.unit,
                min: connectionIntervalValidRange.min * connectionIntervalValidRange.unit,
                max: connectionIntervalValidRange.max * connectionIntervalValidRange.unit,
                update: { [currentConnectionConfiguration] in
                    var currentConfiguration = currentConnectionConfiguration.value
                    currentConfiguration[keyPath: keyPath] = currentConfiguration[keyPath: keyPath] ?? defaultValue
                    currentConfiguration[keyPath: keyPath]?.min = UInt16($0 / connectionIntervalValidRange.unit)

                    currentConnectionConfiguration.accept(currentConfiguration)
                }
            ),
            ViewModelEntry.double(
                label: "Max Connection Interval",
                value: configuration
                    .map { $0?.max }
                    .map { $0.map { "\(Double($0) * connectionIntervalValidRange.unit) ms" } },
                step: connectionIntervalValidRange.unit,
                min: connectionIntervalValidRange.min * connectionIntervalValidRange.unit,
                max: connectionIntervalValidRange.max * connectionIntervalValidRange.unit,
                update: { [currentConnectionConfiguration] in
                    var currentConfiguration = currentConnectionConfiguration.value
                    currentConfiguration[keyPath: keyPath] = currentConfiguration[keyPath: keyPath] ?? defaultValue
                    currentConfiguration[keyPath: keyPath]?.max = UInt16($0 / connectionIntervalValidRange.unit)

                    currentConnectionConfiguration.accept(currentConfiguration)
                }
            ),
            ViewModelEntry.int(
                label: "Slave Latency",
                value: configuration
                    .map { $0?.slaveLatency }
                    .map { $0.map { "\($0) intervals" } },
                step: Int(slaveLatencyValidRange.unit),
                min: Int(slaveLatencyValidRange.min),
                max: Int(slaveLatencyValidRange.max),
                update: { [currentConnectionConfiguration] in
                    var currentConfiguration = currentConnectionConfiguration.value
                    currentConfiguration[keyPath: keyPath] = currentConfiguration[keyPath: keyPath] ?? defaultValue
                    currentConfiguration[keyPath: keyPath]?.slaveLatency = UInt8($0)

                    currentConnectionConfiguration.accept(currentConfiguration)
                }
            ),
            ViewModelEntry.int(
                label: "Supervision Timeout",
                value: configuration
                    .map { $0?.supervisionTimeout }
                    .map { $0.map { "\(UInt32($0) * supervisionTimeoutValidRange.unit) ms" } },
                step: Int(supervisionTimeoutValidRange.unit),
                min: Int(supervisionTimeoutValidRange.min * supervisionTimeoutValidRange.unit),
                max: Int(supervisionTimeoutValidRange.max * supervisionTimeoutValidRange.unit),
                update: { [currentConnectionConfiguration] in
                    var currentConfiguration = currentConnectionConfiguration.value
                    currentConfiguration[keyPath: keyPath] = currentConfiguration[keyPath: keyPath] ?? defaultValue
                    currentConfiguration[keyPath: keyPath]?.supervisionTimeout = UInt8($0 / Int(supervisionTimeoutValidRange.unit))

                    currentConnectionConfiguration.accept(currentConfiguration)
                }
            )
        ]
    }

    fileprivate var fastModeConfigurationDetails: [ViewModelEntry] {
        let configuration = currentConnectionConfiguration
            .asDriver()
            .map { $0.fastModeConfiguration }

        return viewModelEntries(
            for: configuration,
            with: \PreferredConnectionConfiguration.fastModeConfiguration,
            defaultValue: .fast
        )
    }

    fileprivate var slowModeConfigurationDetails: [ViewModelEntry] {
        let configuration = currentConnectionConfiguration
            .asDriver()
            .map { $0.slowModeConfiguration }

        return viewModelEntries(
            for: configuration,
            with: \PreferredConnectionConfiguration.slowModeConfiguration,
            defaultValue: .slow
        )
    }

    fileprivate var dleConfigurationDetails: [ViewModelEntry] {
        let configuration = currentConnectionConfiguration
            .asDriver()
            .map { $0.dleConfiguration }

        return [
            ViewModelEntry.int(
                label: "Max Tx PDU",
                value: configuration
                    .map { $0?.maxTxPDU }
                    .map { $0.map { "\($0) bytes" } },
                step: 1,
                min: 0x001B,
                max: 0x00FB,
                update: { [currentConnectionConfiguration] in
                    var currentConfiguration = currentConnectionConfiguration.value
                    currentConfiguration.dleConfiguration =
                        currentConfiguration.dleConfiguration ?? .default
                    currentConfiguration.dleConfiguration?.maxTxPDU = UInt8($0)

                    currentConnectionConfiguration.accept(currentConfiguration)
                }
            ),
            ViewModelEntry.int(
                label: "Max Tx Time",
                value: configuration
                    .map { $0?.maxTxTime }
                    .map { $0.map { "\($0) ms" } },
                step: 1,
                min: 0x0148,
                max: 0x0848,
                update: { [currentConnectionConfiguration] in
                    var currentConfiguration = currentConnectionConfiguration.value
                    currentConfiguration.dleConfiguration =
                        currentConfiguration.dleConfiguration ?? .default
                    currentConfiguration.dleConfiguration?.maxTxTime = UInt16($0)

                    currentConnectionConfiguration.accept(currentConfiguration)
                }
            )
        ]
    }

    fileprivate var mtuDetails: [ViewModelEntry] {
        let configuration = currentConnectionConfiguration
            .asDriver()

        return [
            ViewModelEntry.int(
                label: "MTU",
                value: configuration
                    .map { $0.MTU }
                    .map { $0.map { "\($0) bytes" } },
                step: 1,
                min: 23,
                max: 506,
                update: { [currentConnectionConfiguration] in
                    var currentConfiguration = currentConnectionConfiguration.value
                    currentConfiguration.MTU = UInt16($0)
                    
                    currentConnectionConfiguration.accept(currentConfiguration)
                }
            )
        ]
    }

    fileprivate var modeDetails: [ViewModelEntry] {
        let configuration = currentConnectionMode
            .asDriver()

        return [
            ViewModelEntry.string(
                label: "Mode",
                value: configuration
                    .map { $0.mode }
                    .map { "\($0)" },
                possbileValues: ["fast", "slow"],
                update: { [currentConnectionMode] in
                    var preferredMode = currentConnectionMode.value
                    switch $0 {
                    case "fast":
                        preferredMode.mode = .fast
                    case "slow":
                        preferredMode.mode = .slow
                    default:
                    break
                    }
                    
                    currentConnectionMode.accept(preferredMode)
                }
            )
        ]
    }
}

class LinkConfigurationViewController: UITableViewController {
    public typealias ViewModel = LinkConfigurationViewControllerViewModel

    public var viewModel: ViewModel!
    
    private let editButton = UIBarButtonItem(title: "Edit", style: .plain, target: self, action: nil)
    private let doneButton = UIBarButtonItem(title: "Done", style: .plain, target: self, action: nil)
    private let cancelButton = UIBarButtonItem(title: "Cancel", style: .plain, target: self, action: nil)

    override func viewDidLoad() {
        self.navigationItem.rightBarButtonItem = editButton
        self.tableView.allowsSelectionDuringEditing = true

        viewModel.edit
            .asDriver()
            .drive(onNext: { [unowned self] in
                self.navigationItem.rightBarButtonItem = $0 ? self.doneButton : self.editButton
                self.navigationItem.leftBarButtonItem = $0 ? self.cancelButton : self.navigationItem.backBarButtonItem
                self.tableView.setEditing($0, animated: true)
            })
            .disposed(by: disposeBag)

        editButton.rx.tap
            .subscribe(onNext: { [viewModel] in viewModel?.edit.accept(true) })
            .disposed(by: disposeBag)

        // Trigger cancel updates
        cancelButton.rx.tap
            .bind(to: viewModel.cancel)
            .disposed(by: disposeBag)

        // Trigger save new updates
        doneButton.rx.tap
            .bind(to: viewModel.done)
            .disposed(by: disposeBag)
    }

    private func editIntRow(for entry: ViewModelEntry) {
        guard case let .int(label, value, step, min, max, update) = entry else { return }
        let viewController = PickerViewController<Int>()

        let elements: [Int] = stride(from: min, through: max, by: step).map { Int($0) }

        let selectedElement = value
            .map { (currentValue) -> Int in
                guard let currentValue = currentValue else { return min }
                return Int(currentValue.split(separator: " ")[0]) ?? min
            }

        viewController.title = label
        viewController.viewModel = ComboBoxViewModel<Int>(
            elements: elements,
            selectedElement: selectedElement.asObservable(),
            setSelectedElement: update
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    private func editDoubleRow(for entry: ViewModelEntry) {
        guard case let .double(label, value, step, min, max, update) = entry else { return }
        let viewController = PickerViewController<Double>()

        let elements: [Double] = stride(from: Double(min), through: Double(max), by: step).map { $0 }
        
        let selectedElement = value
            .map { (currentValue) -> Double in
                guard let currentValue = currentValue else { return Double(min) }
                return Double(currentValue.split(separator: " ")[0]) ?? Double(min)
            }

        viewController.title = label
        viewController.viewModel = ComboBoxViewModel<Double>(
            elements: elements,
            selectedElement: selectedElement.asObservable(),
            setSelectedElement: update
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    private func editNumericRow(for entry: ViewModelEntry) {
        switch entry {
        case .double:
            editDoubleRow(for: entry)
        case .int:
            editIntRow(for: entry)
        default:
            return
        }
    }

    private func editStringRow(for entry: ViewModelEntry) {
        guard case let .string(label, value, possibleValues, update) = entry else { return }
        let viewController = PickerViewController<String>()

        viewController.title = label
        viewController.viewModel = ComboBoxViewModel<String>(
            elements: possibleValues,
            selectedElement: value.asObservable().filterNil(),
            setSelectedElement: update
        )
        navigationController?.pushViewController(viewController, animated: true)
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        guard viewModel.edit.value == true else { return }

        switch Section(rawValue: indexPath.section)! {
        case .fastMode:
            editNumericRow(for: viewModel.fastModeConfigurationDetails[indexPath.row])
        case .slowMode:
            editNumericRow(for: viewModel.slowModeConfigurationDetails[indexPath.row])
        case .dle:
            editNumericRow(for: viewModel.dleConfigurationDetails[indexPath.row])
        case .mtu:
            editNumericRow(for: viewModel.mtuDetails[indexPath.row])
        case .preferredMode:
            editStringRow(for: viewModel.modeDetails[indexPath.row])
        }
    }

    override func tableView(_ tableView: UITableView, editingStyleForRowAt indexPath: IndexPath) -> UITableViewCell.EditingStyle {
        return .none
    }

    override func tableView(_ tableView: UITableView, shouldIndentWhileEditingRowAt indexPath: IndexPath) -> Bool {
        return false
    }

    override public func tableView(_ tableView: UITableView, titleForHeaderInSection section: Int) -> String? {
        switch Section(rawValue: section)! {
        case .fastMode: return "Fast mode"
        case .slowMode: return "Slow Mode"
        case .dle: return "DLE"
        case .mtu: return "MTU"
        case .preferredMode: return "Preferred Mode"
        }
    }

    override public func numberOfSections(in tableView: UITableView) -> Int {
        return Section.count
    }

    override public func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section)! {
        case .fastMode: return FastModeRow.count
        case .slowMode: return SlowModeRow.count
        case .dle: return DleRow.count
        case .mtu: return MtuRow.count
        case .preferredMode: return ModeRow.count
        }
    }

    override public func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        switch Section(rawValue: indexPath.section)! {
        case .fastMode:
            return cell(for: viewModel.fastModeConfigurationDetails[indexPath.row])
        case .slowMode:
            return cell(for: viewModel.slowModeConfigurationDetails[indexPath.row])
        case .dle:
            return cell(for: viewModel.dleConfigurationDetails[indexPath.row])
        case .mtu:
            return cell(for: viewModel.mtuDetails[indexPath.row])
        case .preferredMode:
            return cell(for: viewModel.modeDetails[indexPath.row])
        }
    }

    private func cell(for entry: ViewModelEntry) -> UITableViewCell {
        let cell = UITableViewCell(style: .value1, reuseIdentifier: nil)
        cell.selectionStyle = .none
        cell.editingAccessoryType = .disclosureIndicator

        switch entry {
        case .int(let label, let value, _, _, _, _),
             .double(let label, let value, _, _, _, _),
             .string(let label, let value, _, _):
            cell.textLabel?.text = label

            value
                .drive(onNext: { cell.detailTextLabel?.text = $0 })
                .disposed(by: disposeBag)
        }

        return cell
    }
}

private enum Section: Int {
    case fastMode
    case slowMode
    case dle
    case mtu
    case preferredMode

    static var count = 5
}

private enum FastModeRow: Int {
    case min
    case max
    case slaveLatency
    case supervisionTimeout

    static var count = 4
}

private enum SlowModeRow: Int {
    case min
    case max
    case slaveLatency
    case supervisionTimeout

    static var count = 4
}

private enum DleRow: Int {
    case maxTxPDU
    case maxTxTime

    static var count = 2
}

private enum MtuRow: Int {
    case mtu

    static var count = 1
}

private enum ModeRow: Int {
    case mode

    static var count = 1
}

#endif
