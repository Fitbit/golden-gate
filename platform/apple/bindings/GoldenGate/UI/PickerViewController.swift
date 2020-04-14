//
//  PickerViewController.swift
//  GoldenGate-iOS
//
//  Created by Sylvain Rebaud on 5/1/18.
//  Copyright © 2018 Fitbit. All rights reserved.
//

#if os(iOS)

import RxCocoa
import RxSwift
import UIKit

/// A view controller that shows a UIPickerView
class PickerViewController<Element: Equatable>: UIViewController {
    public var viewModel: ComboBoxViewModel<Element>!

    // iOS10 auto layout backward compatibility
    private var topLayoutConstraint: NSLayoutConstraint?
    private var bottomLayoutConstraint: NSLayoutConstraint?
    private var leadingLayoutConstraint: NSLayoutConstraint?
    private var trailingLayoutConstraint: NSLayoutConstraint?

    private let picker = UIPickerView(frame: CGRect.zero)

    override func loadView() {
        // Create white background view
        view = UIView(frame: CGRect.zero)
        view.backgroundColor = .white

        // Add a picker view to background view
        view.addSubview(picker)

        // Map view model elements to picker item titles
        Observable.just(viewModel.elements)
            .observeOn(MainScheduler.instance)
            .bind(to: picker.rx.itemTitles) { (_, element) in
                return "\(element)"
            }
            .disposed(by: disposeBag)

        // Update view model upon user selection and hide picker automatically
        picker.rx.modelSelected(Element.self)
            .take(1)
            .map { $0[0] }
            .do(onNext: viewModel.setSelectedElement)
            .do(onNext: { [unowned self] _ in self.pop() })
            .subscribe()
            .disposed(by: disposeBag)
        
        // Set initial element
        viewModel.selectedElement
            .do(onNext: { [unowned self] in
                self.picker.selectRow(
                    self.viewModel.elements.firstIndex(of: $0) ?? 0,
                    inComponent: 0,
                    animated: false
                )
            })
            .subscribe()
            .disposed(by: disposeBag)

        // Layout picker view to fill up background view
        picker.translatesAutoresizingMaskIntoConstraints = false
        if #available(iOS 11.0, *) {
            picker.leadingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.leadingAnchor).isActive = true
            picker.trailingAnchor.constraint(equalTo: view.safeAreaLayoutGuide.trailingAnchor).isActive = true
            picker.topAnchor.constraint(equalTo: view.safeAreaLayoutGuide.topAnchor).isActive = true
            picker.bottomAnchor.constraint(equalTo: view.safeAreaLayoutGuide.bottomAnchor).isActive = true
        } else {
            // Fallback on earlier versions
            view.setNeedsLayout()
        }
    }

    override func viewDidLayoutSubviews() {
        super.viewDidLayoutSubviews()

        if #available(iOS 11.0, *) {
            // safe area constraints already set
        } else {
            if topLayoutConstraint == nil {
                topLayoutConstraint = picker.topAnchor.constraint(equalTo: topLayoutGuide.bottomAnchor)
                topLayoutConstraint?.isActive = true
            }
            if bottomLayoutConstraint == nil {
                bottomLayoutConstraint = picker.bottomAnchor.constraint(equalTo: bottomLayoutGuide.topAnchor)
                bottomLayoutConstraint?.isActive = true
            }
            if leadingLayoutConstraint == nil {
                leadingLayoutConstraint = picker.leadingAnchor.constraint(equalTo: view.layoutMarginsGuide.leadingAnchor)
                leadingLayoutConstraint?.isActive = true
            }
            if trailingLayoutConstraint == nil {
                trailingLayoutConstraint = picker.trailingAnchor.constraint(equalTo: view.layoutMarginsGuide.trailingAnchor)
                trailingLayoutConstraint?.isActive = true
            }
        }
    }

    private func pop() {
        navigationController?.popViewController(animated: true)
    }
}

#endif
