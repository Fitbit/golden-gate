//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ComboBoxViewModel.swift
//  BluetoothConnection
//
//  Created by Marcel Jackwerth on 4/5/18.
//

import RxSwift

/// A view model to select one item out of many.
public struct ComboBoxViewModel<Element: Equatable> {
    /// The options to pick form.
    public let elements: Observable<[Element]>

    /// The currently selected element.
    public let selectedElement: Observable<Element>

    /// Callback to change the selected element.
    public let setSelectedElement: (Element) -> Void

    /// Creates a new instance.
    public init(
        elements: Observable<[Element]>,
        selectedElement: Observable<Element>,
        setSelectedElement: @escaping (Element) -> Void
    ) {
        self.elements = elements
        self.selectedElement = selectedElement
        self.setSelectedElement = setSelectedElement
    }
}
