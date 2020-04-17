// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.bluetooth.fbgatt.rx

import com.fitbit.bluetooth.fbgatt.TransactionResult

/**
 * Exception thrown when [com.fitbit.bluetooth.fbgatt.GattTransaction] fails with result other than
 * [TransactionResult.TransactionResultStatus.SUCCESS]
 */
class GattTransactionException(
        val result: TransactionResult,
        message: String? = null,
        cause: Throwable? = null
) : Exception(message, cause)
