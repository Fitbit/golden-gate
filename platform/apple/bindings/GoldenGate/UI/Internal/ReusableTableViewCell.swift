//
//  ReusableTableViewCell.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/5/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

#if os(iOS)

import UIKit

/// Utility to avoid dealing with `cellReuseIdentifier` all the time.
internal protocol ReusableTableViewCell: class {
    static var cellReuseIdentifier: String { get }
}

/// Default implementation just uses the type
internal extension ReusableTableViewCell {
    static var cellReuseIdentifier: String {
        return "\(type(of: self))"
    }
}

internal extension UITableView {
    /// Registers a ReusableTableViewCell for later dequeueing.
    func register<T: ReusableTableViewCell & UITableViewCell>(_ type: T.Type) {
        register(type, forCellReuseIdentifier: type.cellReuseIdentifier)
    }

    /// Dequeues a ReusableTableViewCell.
    ///
    /// - See Also: `dequeueReusableCell(withIdentifier:)`
    func dequeue<T: ReusableTableViewCell & UITableViewCell>() -> T? {
        return dequeueReusableCell(withIdentifier: T.self.cellReuseIdentifier) as? T
    }

    /// Dequeues a ReusableTableViewCell.
    ///
    /// Variant with an explicit type parameter.
    ///
    /// - See Also: `dequeueReusableCell(withIdentifier:)`
    func dequeue<T: ReusableTableViewCell & UITableViewCell>(_ type: T.Type) -> T? {
        return dequeueReusableCell(withIdentifier: type.cellReuseIdentifier) as? T
    }

    /// Dequeues a ReusableTableViewCell.
    ///
    /// - See Also: `dequeueReusableCell(withIdentifier:for:)`
    func dequeue<T: ReusableTableViewCell & UITableViewCell>(for indexPath: IndexPath) -> T {
        // swiftlint:disable:next force_cast
        return dequeueReusableCell(withIdentifier: T.self.cellReuseIdentifier, for: indexPath) as! T
    }

    /// Dequeues a ReusableTableViewCell.
    ///
    /// Variant with an explicit type parameter.
    ///
    /// - See Also: `dequeueReusableCell(withIdentifier:for:)`
    func dequeue<T: ReusableTableViewCell & UITableViewCell>(_ type: T.Type, for indexPath: IndexPath) -> T {
        // swiftlint:disable:next force_cast
        return dequeueReusableCell(withIdentifier: T.self.cellReuseIdentifier, for: indexPath) as! T
    }
}

#endif
