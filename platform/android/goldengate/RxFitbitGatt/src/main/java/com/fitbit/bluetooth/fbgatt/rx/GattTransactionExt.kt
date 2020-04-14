package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.GattConnection
import com.fitbit.bluetooth.fbgatt.GattServerConnection
import com.fitbit.bluetooth.fbgatt.GattTransaction
import com.fitbit.bluetooth.fbgatt.TransactionResult
import io.reactivex.Single
import timber.log.Timber

/**
 * Extension function to create reactive method around [GattTransaction.runTx]
 *
 * @param gattConnection for peripheral this transaction is executed for
 */
fun GattTransaction.runTxReactive(gattConnection: GattConnection): Single<TransactionResult> {
    return Single.create<TransactionResult> { emitter ->
        Timber.d("Running GattTransaction ${this.name}")
        gattConnection.runTx(this) { result ->
            if (result.resultStatus == TransactionResult.TransactionResultStatus.SUCCESS) {
                Timber.d("Running GattTransaction ${this.name} succeeded")
                emitter.onSuccess(result)
            } else {
                Timber.w("Running GattTransaction ${this.name} failed, result: $result")
                emitter.onError(GattTransactionException(result))
            }
        }
    }
}

/**
 * Extension function to create reactive method around [GattTransaction.runTx]
 *
 * @param gattServerConnection for the service that transaction is executed for
 */
fun GattTransaction.runTxReactive(gattServerConnection: GattServerConnection): Single<TransactionResult> {
    return Single.create<TransactionResult> { emitter ->
        Timber.d("Running GattTransaction ${this.name}")
        gattServerConnection.runTx(this) { result ->
            if (result.resultStatus == TransactionResult.TransactionResultStatus.SUCCESS) {
                Timber.d("Running GattTransaction ${this.name} succeeded")
                emitter.onSuccess(result)
            } else {
                Timber.w("Running GattTransaction ${this.name} failed, result: $result")
                emitter.onError(GattTransactionException(result))
            }
        }
    }
}
