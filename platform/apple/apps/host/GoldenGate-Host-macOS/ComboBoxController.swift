//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ComboBoxController.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/6/18.
//

import Cocoa
import GoldenGate
import RxSwift

/// Utility to bind a NSComboBox to a `ComboBoxViewModel`
class ComboBoxController<Element: Equatable>: NSObject, NSComboBoxDataSource, NSComboBoxDelegate {
    private let viewModel: ComboBoxViewModel<Element>

    init(viewModel: ComboBoxViewModel<Element>) {
        self.viewModel = viewModel
    }

    private subscript(index: Int) -> Element? {
        guard index >= 0 && index < viewModel.elements.count else { return nil }
        return viewModel.elements[index]
    }

    private var selectedIndex: Observable<Int?> {
        return viewModel.selectedElement.map { [viewModel] element in
            viewModel.elements.firstIndex(where: { $0 == element })
        }
    }

    func numberOfItems(in comboBox: NSComboBox) -> Int {
        return viewModel.elements.count
    }

    func comboBox(_ comboBox: NSComboBox, objectValueForItemAt index: Int) -> Any? {
        guard let item = self[index] else { return nil }
        return "\(item)"
    }

    @objc func comboBoxSelectionDidChange(_ notification: Notification) {
        guard
            let comboBox = notification.object as? NSComboBox,
            let element = self[comboBox.indexOfSelectedItem]
            else { return }

        viewModel.setSelectedElement(element)
    }

    func bind(to comboBox: NSComboBox) {
        comboBox.addItems(withObjectValues: viewModel.elements)
        comboBox.usesDataSource = true
        comboBox.delegate = self
        comboBox.dataSource = self

        selectedIndex
            .filterNil()
            .distinctUntilChanged(==)
            .asDriver(onErrorJustReturn: -1)
            .drive(onNext: comboBox.selectItem)
            .disposed(by: disposeBag)
    }
}
