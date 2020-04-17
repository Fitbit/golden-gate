//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CaseIterable+AllUniqueRawValues.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 25/02/2020.
//

import Foundation

extension CaseIterable where AllCases.Element: RawRepresentable, AllCases.Element.RawValue: Hashable {
    public static var allUniqueRawValues: Set<AllCases.Element.RawValue> {
        Set(allCases.map { $0.rawValue })
    }
}
