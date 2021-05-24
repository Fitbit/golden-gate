//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  MainViewController.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/4/18.
//

import BluetoothConnection
import Cocoa
import GoldenGate
import RxCocoa
import RxSwift

class MainViewController: NSViewController {
    @IBOutlet private var tableView: NSTableView!
    @IBOutlet private var serviceComboBox: NSComboBox!
    @IBOutlet private var stackComboBox: NSComboBox!

    private var simulator: NodeSimulator!
    private var peerManager: PeerManager<ManagedHub>!
    private var stackComboBoxController: ComboBoxController<StackDescriptor>!
    private var serviceComboBoxController: ComboBoxController<ServiceDescriptor>!

    fileprivate var menuActionRow = -1

    private let disposeBag = DisposeBag()

    override func viewDidLoad() {
        simulator = Component.instance.nodeSimulator
        peerManager = Component.instance.peerManager

        // Setup stackComboBoxController
        let globalStackDescriptor = Component.instance.globalStackDescriptor
        stackComboBoxController = ComboBoxController(viewModel: ComboBoxViewModel<StackDescriptor>(
            elements: .just([
                .dtlsSocketNetifGattlinkActivity,
                .dtlsSocketNetifGattlink,
                .socketNetifGattlink,
                .netifGattlink,
                .gattlink
            ]),
            selectedElement: globalStackDescriptor.asObservable().distinctUntilChanged(==),
            setSelectedElement: globalStackDescriptor.accept
        ))
        stackComboBoxController.bind(to: stackComboBox)

        // Setup globalServiceDescriptor
        let globalServiceDescriptor = Component.instance.globalServiceDescriptor
        serviceComboBoxController = ComboBoxController(viewModel: ComboBoxViewModel<ServiceDescriptor>(
            elements: .just([
                .none,
                .coap,
                .blasting
            ]),
            selectedElement: globalServiceDescriptor.asObservable().distinctUntilChanged(==),
            setSelectedElement: globalServiceDescriptor.accept
        ))
        serviceComboBoxController.bind(to: serviceComboBox)

        // Setup tableView
        tableView.delegate = self
        tableView.dataSource = self

        // Reload tableView when peers change
        peerManager.peers
            .asDriver()
            .drive(onNext: { [unowned self] _ in
                self.tableView.reloadData()
            })
            .disposed(by: disposeBag)
    }

    fileprivate func peer(at index: Int) -> ManagedHub? {
        let peers = peerManager.peers.value
        guard index >= 0 && index < peers.count else { return nil }
        return peers[index]
    }

    @IBAction func disconnect(_ sender: Any?) {
        guard let peer = peer(at: menuActionRow) else { return }
        peer.connectionController.disconnect(trigger: "admin")
    }

    @IBAction func forget(_ sender: Any?) {
        guard let peer = peer(at: menuActionRow) else { return }
        peerManager.remove(peer)
    }

    @IBAction func blast(_ sender: Any?) {
        guard
            let peer = peer(at: menuActionRow)
        else { return }

        let shouldBlast = peer.blasterService.shouldBlast.value
        peer.blasterService.shouldBlast.accept(!shouldBlast)
    }
}

extension MainViewController: NSTableViewDelegate, NSTableViewDataSource, NSMenuItemValidation {
    func numberOfRows(in tableView: NSTableView) -> Int {
        return peerManager.peers.value.count
    }

    // swiftlint:disable:next function_body_length
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        guard
            let peer = peer(at: row),
            let tableColumn = tableColumn,
            let cell = tableView.makeView(withIdentifier: tableColumn.identifier, owner: nil) as? NSTableCellView
        else { return nil }
        
        let connection = peer.connectionController.connectionStatus
            .catchErrorJustReturn(.disconnected)
            .map { $0.connection }

        switch tableColumn.identifier {
        case .nameColumn:
            _ = peer.name
                .takeUntil(cell.rx.detached)
                .asDriver(onErrorJustReturn: "")
                .drive(cell.textField!.rx.text)
        case .uuidColumn:
            _ = peer.connectionController.descriptor
                .map { $0?.identifier.uuidString }
                .takeUntil(cell.rx.detached)
                .asDriver(onErrorJustReturn: "")
                .drive(cell.textField!.rx.text)
        case .stateColumn:
            _ = peer.connectionController.connectionStatus
                .map { $0.cellDescription }
                .takeUntil(cell.rx.detached)
                .asDriver(onErrorJustReturn: "")
                .drive(cell.textField!.rx.text)
        case .blasterStatsColumn:
            _ = peer.blasterService.stats
                .map { $0.description }
                .startWith("n/a")
                .takeUntil(cell.rx.detached)
                .asDriver(onErrorJustReturn: "")
                .drive(cell.textField!.rx.text)
        case .preferredConnectionConfigColumn:
            _ = connection
                .flatMapLatest { $0?.remotePreferredConnectionConfiguration.asObservable() ?? .just(nil) }
                .map { $0?.description ?? "n/a" }
                .startWith("n/a")
                .takeUntil(cell.rx.detached)
                .asDriver(onErrorJustReturn: "")
                .drive(cell.textField!.rx.text)
        case .preferredConnectionModeColumn:
            _ = connection
                .flatMapLatest { $0?.remotePreferredConnectionMode.asObservable() ?? .just(nil) }
                .map {
                    $0?.description ?? "n/a"
                }
                .startWith("n/a")
                .takeUntil(cell.rx.detached)
                .asDriver(onErrorJustReturn: "")
                .drive(cell.textField!.rx.text)
        default:
            return nil
        }

        return cell
    }

    func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
        if menuItem.action == #selector(blast(_:)), let peer = peer(at: tableView.clickedRow) {
            menuItem.title = "Blast"

            // disable blast menu item if blaster service is not set
            if peer.blasterService.shouldBlast.value {
                menuItem.title = "Stop Blast"
            }

            return true
        }

        return responds(to: menuItem.action)
    }
}

extension MainViewController: NSMenuDelegate {
    func menuWillOpen(_ menu: NSMenu) {
        menuActionRow = tableView.clickedRow
    }
}

private extension NSUserInterfaceItemIdentifier {
    static let nameColumn = NSUserInterfaceItemIdentifier(rawValue: "name")
    static let uuidColumn = NSUserInterfaceItemIdentifier(rawValue: "uuid")
    static let stateColumn = NSUserInterfaceItemIdentifier(rawValue: "state")
    static let blasterStatsColumn = NSUserInterfaceItemIdentifier(rawValue: "blasterStats")
    static let preferredConnectionConfigColumn = NSUserInterfaceItemIdentifier(rawValue: "preferredConnectionConfig")
    static let preferredConnectionModeColumn = NSUserInterfaceItemIdentifier(rawValue: "preferredConnectionMode")
}
