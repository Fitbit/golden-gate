// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

package com.fitbit.goldengatehost

import android.annotation.SuppressLint
import android.app.Application
import android.os.Build
import android.util.Log
import io.reactivex.exceptions.UndeliverableException
import io.reactivex.plugins.RxJavaPlugins
import timber.log.Timber

/**
 * Created by muyttersprot on 11/13/17.
 */

class GoldenGateHostApplication : Application() {

    override fun onCreate() {
        super.onCreate()

        initRxJava()
        Timber.plant(ThreadReportingTimberTree())
    }

    class ThreadReportingTimberTree : Timber.DebugTree() {
        override fun log(priority: Int, tag: String?, message: String, t: Throwable?) {
            val logMessage = StringBuffer().append("[").append(Thread.currentThread().name).append("] ").append(message).toString()

            super.log(priority, tag, logMessage, t)
        }
    }

    @SuppressLint("LogNotTimber")
    private fun initRxJava() {
        // RxJava2 makes an extra effort to not swallow any errors.  If an error is generated in an observable,
        // but the observer has already un-subscribed, then the observable has no way to route the error.
        // In these cases RxJava2 sends the error to this global error handler.
        RxJavaPlugins.setErrorHandler { t ->
            val current = Thread.currentThread()
            if (t is UndeliverableException) {
                val wrapped = t.cause
                if (wrapped is InterruptedException) {
                    // fine, some blocking code was interrupted by a dispose call
                    Timber.d(wrapped)
                    return@setErrorHandler
                }
                if (wrapped !is NullPointerException &&
                    wrapped !is IllegalArgumentException &&
                    wrapped !is IllegalStateException
                ) {
                    // Unless the undeliverable exception is an obvious programming error, just log it and continue.
                    Timber.e(wrapped)
                    return@setErrorHandler
                }
            }
            // crashy time
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) {
                // Calling uncaughtException directly skips logcat until P
                // https://android.googlesource.com/platform/frameworks/base.git/+/b1f5312c61926bc5e5343d1f5886e5ba1d01a1aa
                Log.e("Fitbit", String.format("FATAL EXCEPTION %s", t.javaClass.simpleName), t)
            }
            current.uncaughtExceptionHandler.uncaughtException(current, t)
        }
    }

}
