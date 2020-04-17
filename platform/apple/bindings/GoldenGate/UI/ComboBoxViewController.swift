//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  ComboBoxViewController.swift
//  GoldenGate
//
//  Created by Marcel Jackwerth on 4/3/18.
//

#if os(iOS)

import RxCocoa
import RxSwift
import UIKit

/// A view controller that allows selection of one item.
class ComboBoxViewController<Element: Equatable>: UITableViewController {
    public var viewModel: ComboBoxViewModel<Element>!

    override func viewDidLoad() {
        super.viewDidLoad()

        tableView.dataSource = nil
        tableView.register(Cell.self)

        tableView.rx.modelSelected(Element.self)
            .take(1)
            .do(onNext: viewModel.setSelectedElement)
            .do(onNext: { [unowned self] _ in self.pop() })
            .subscribe()
            .disposed(by: disposeBag)

        Observable.just(viewModel.elements)
            .observeOn(MainScheduler.instance)
            .bind(to: tableView.rx.items) { [viewModel = viewModel!] tableView, _, element in
                let cell = tableView.dequeue(Cell.self)!
                let checked = viewModel.selectedElement.map { $0 == element }
                return cell.bind(Cell.ViewModel(
                    label: .just("\(element)"),
                    checked: checked
                ))
            }
            .disposed(by: disposeBag)
    }

    override func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        tableView.deselectRow(at: indexPath, animated: true)
    }

    private func pop() {
        navigationController?.popViewController(animated: true)
    }
}

private class Cell: UITableViewCell, ReusableTableViewCell {
    struct ViewModel {
        let label: Observable<String>
        let checked: Observable<Bool>
    }

    override init(style: UITableViewCell.CellStyle, reuseIdentifier: String?) {
        super.init(style: .subtitle, reuseIdentifier: reuseIdentifier)
    }

    required init?(coder aDecoder: NSCoder) {
        fatalError("not implemented")
    }

    func bind(_ viewModel: ViewModel) -> Cell {
        viewModel.label
            .takeUntil(rx.methodInvoked(#selector(prepareForReuse)))
            .asDriver(onErrorJustReturn: "")
            .drive(textLabel!.rx.text)
            .disposed(by: disposeBag)

        viewModel.checked
            .takeUntil(rx.methodInvoked(#selector(prepareForReuse)))
            .asDriver(onErrorJustReturn: false)
            .drive(rx.checked)
            .disposed(by: disposeBag)

        return self
    }
}

#endif
