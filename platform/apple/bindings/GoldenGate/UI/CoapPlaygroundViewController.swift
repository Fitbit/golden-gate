//  Copyright 2017-2020 Fitbit, Inc
//  SPDX-License-Identifier: Apache-2.0
//
//  CoapPlaygroundViewController.swift
//  GoldenGate
//
//  Created by Emanuel Jarnea on 27/07/2020.
//

#if os(iOS)

import RxCocoa
import RxSwift
import UIKit

public protocol CoapPlaygroundViewControllerViewModel {
    var coapEndpoint: CoapEndpointType { get }
}

public final class CoapPlaygroundViewController: UITableViewController {
    public var viewModel: CoapPlaygroundViewControllerViewModel!

    private let disposeBag = DisposeBag()

    override public func numberOfSections(in tableView: UITableView) -> Int {
        return Section.allCases.count
    }

    override public func tableView(_ tableView: UITableView, numberOfRowsInSection section: Int) -> Int {
        switch Section(rawValue: section)! {
        case .coap: return CoapRow.allCases.count
        }
    }

    override public func tableView(_ tableView: UITableView, cellForRowAt indexPath: IndexPath) -> UITableViewCell {
        switch Section(rawValue: indexPath.section)! {
        case .coap: return cell(forRow: CoapRow(rawValue: indexPath.row)!)
        }
    }

    override public func tableView(_ tableView: UITableView, didSelectRowAt indexPath: IndexPath) {
        switch Section(rawValue: indexPath.section)! {
        case .coap:
            tableView.deselectRow(at: indexPath, animated: true)
            switch CoapRow(rawValue: indexPath.item)! {
            case .performGetRequest:
                sendCoapRequest(method: .get)
            case .performPutRequest:
                sendCoapRequest(method: .put)
            }
        }
    }
}

private extension CoapPlaygroundViewController {
    func cell(forRow row: CoapRow) -> UITableViewCell {
        let cell: UITableViewCell

        switch row {
        case .performGetRequest:
            cell = UITableViewCell(style: .default, reuseIdentifier: nil)
            cell.textLabel?.text = "CoAP Request: GET /helloworld"
            cell.textLabel?.textColor = self.view.tintColor
        case .performPutRequest:
            cell = UITableViewCell(style: .default, reuseIdentifier: nil)
            cell.textLabel?.text = "CoAP Request: PUT /helloworld"
            cell.textLabel?.textColor = self.view.tintColor
        }

        return cell
    }
}

private extension CoapPlaygroundViewController {
    func sendCoapRequest(method: CoapCode.Request) {
        let time = Date()
        let requestBuilder = CoapRequestBuilder()
            .method(method)
            .path(["helloworld"])

        switch method {
        case .put, .post:
            requestBuilder.body(data: UIDevice.current.name.data(using: .utf8)!)
        case .get, .delete:
            break
        }

        viewModel
            .coapEndpoint
            .response(request: requestBuilder.build())
            .flatMap { $0.body.asData() }
            .timeout(.seconds(5), scheduler: MainScheduler.instance)
            .observeOn(MainScheduler.instance)
            .subscribe(
                onSuccess: { [weak self] response in
                    let message = String(data: response, encoding: .utf8) ?? "ENCODING ERROR"
                    let responseTime = String(format: "%.2f s.", Date().timeIntervalSince(time))

                    let alert = UIAlertController(
                        title: "CoAP Response received after \(responseTime)",
                        message: message,
                        preferredStyle: .alert
                    )

                    alert.addAction(UIAlertAction(title: "OK", style: .default, handler: nil))
                    self?.present(alert, animated: true, completion: nil)
                }, onError: { [weak self] error in
                    let alert = UIAlertController(
                        title: "CoAP Response Error",
                        message: "\(error)",
                        preferredStyle: .alert
                    )

                    alert.addAction(UIAlertAction(title: "OK", style: .default, handler: nil))
                    self?.present(alert, animated: true, completion: nil)
                }
            )
            .disposed(by: disposeBag)
    }
}

private enum Section: Int, CaseIterable {
    case coap
}

private enum CoapRow: Int, CaseIterable {
    case performGetRequest
    case performPutRequest
}

extension GGPeer: CoapPlaygroundViewControllerViewModel {}

#endif
