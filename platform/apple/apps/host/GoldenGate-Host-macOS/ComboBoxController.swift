//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ComboBoxController.swift
//  GoldenGateHost-macOS
//
//  Created by Marcel Jackwerth on 4/6/18.
//

import BluetoothConnection
import Cocoa
import RxSwift

/// Utility to bind a NSComboBox to a `ComboBoxViewModel`
final class ComboBoxController<Element: Equatable>: NSObject, NSComboBoxDelegate {
    private let viewModel: ComboBoxViewModel<Element>
    private let didChangeSelection = PublishSubject<Int>()
    private let disposeBag = DisposeBag()

    init(viewModel: ComboBoxViewModel<Element>) {
        self.viewModel = viewModel

        Observable.combineLatest(didChangeSelection, viewModel.elements)
            .subscribe(onNext: { index, elements in
                guard index >= 0 && index < elements.count else { return }

                viewModel.setSelectedElement(elements[index])
            })
            .disposed(by: disposeBag)
    }

    @objc func comboBoxSelectionDidChange(_ notification: Notification) {
        guard let comboBox = notification.object as? NSComboBox else { return }

        didChangeSelection.onNext(comboBox.indexOfSelectedItem)
    }

    func bind(to comboBox: NSComboBox) {
        viewModel.elements
            .subscribe(onNext: {
                comboBox.removeAllItems()
                comboBox.addItems(withObjectValues: $0)
            })
            .disposed(by: disposeBag)

        comboBox.usesDataSource = false
        comboBox.delegate = self

        Observable.combineLatest(viewModel.selectedElement, viewModel.elements)
            .compactMap { selection, elements in elements.firstIndex { $0 == selection } }
            .distinctUntilChanged()
            .asDriver(onErrorJustReturn: -1)
            .drive(onNext: comboBox.selectItem)
            .disposed(by: disposeBag)
    }
}
