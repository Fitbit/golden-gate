package com.fitbit.goldengate.bindings.logging

import androidx.annotation.VisibleForTesting
import android.util.Log
import androidx.annotation.Keep
import com.fitbit.goldengate.bindings.GoldenGate
import timber.log.Timber
import java.io.Closeable

/**
 * Created by jpreston on 12/7/17.
 *
 * This is an abstract Logger class that can be implemented by the host to have custom logging.
 * The jni code will call back into the java from both Golden Gate and JNI code to deliver it to
 * the end user
 *
 * In order to use this logger, you will need to set your android property on your phone. To do this,
 * use adb set prop and the property debug.gg.log.config. e.g., debug.gg.log.config plist:.level=ALL
 */

@Keep
internal abstract class Logger : Closeable {

    val ggMethodSignature = "(Ljava/lang/String;IJLjava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V";
    val jniMethodSignature = "(Ljava/lang/String;Ljava/lang/String;)V";

    val ptr: Long

    init {
        GoldenGate.check()

        ptr = createLoggerJNI(Logger::class.java, "log", ggMethodSignature, jniMethodSignature)
    }

    /*  Logs the given message from JNI
        - Parameters:
        - message: Message to the logger
        - tag: for the end user to use as a tag into specific JNI classes and functions
     */
    @Keep
    abstract fun log(tag: String, message: String);

    /*

    *   Logs the given message from GoldenGate
    *   - Parameters:
    *   - message: The message to be logger.
    *              Provided as a closure to avoid performing interpolation if the message is not going to be logged.
    *   - level: The severity of the message.
    *            A logger can use this flag to decide if to log or ignore this specific message.
    *   - timestamp: the time the logger logged the event
    *   - loggerName: the name of the logger
    *   - file: The file name of the file where the message was created.
    *   - function: The function name where the message was created.
    *   - line: The line number in the file where the message was created.
    */
    abstract fun log(message: String, level: LogLevel, timeStamp: Long,
            loggerName: String, fileName: String, function: String, line: Int)

    /* The logger should be closed in a cleanup to prevent memory leaks */
    override fun close() {
        destroyLoggerJNI(ptr)
    }

    /* callback method from JNI into Java. DO NOT CALL THIS METHOD */
    @VisibleForTesting
    fun log(message: String, level: Int, timeStamp: Long,
            loggerName: String, fileName: String, function: String, line: Int) {
        log(message, LogLevel.getLevel(level), timeStamp, loggerName, fileName, function, line)
    }

    external fun createLoggerJNI(clazz: Class<Logger>, methodName: String,
                                 ggMethodSignature: String, jniMethodSignature: String) : Long
    external fun destroyLoggerJNI(loggerPtr: Long)

}

/* Simple logger implementation for now, but otherwise a logger impl should be implemented via the host */
internal class SimpleLogger : Logger() {
    override fun log(tag: String, message: String) {
        Timber.tag(tag).d(message)
    }

    override fun log(message: String, level: LogLevel, timeStamp: Long,
                     loggerName: String, fileName: String, function: String, line: Int) {
        Timber.tag("GoldenGate").log(level.priority, "%s: %s", level.name, message)
    }
}

internal enum class LogLevel(val priority : Int) {
    FATAL(Log.ERROR), // Not Log.ASSERT because that should only be used when the app is crashing.
    SEVERE(Log.ERROR),
    WARNING(Log.WARN),
    INFO(Log.INFO),
    FINE(Log.DEBUG),
    FINER(Log.VERBOSE),
    FINEST(Log.VERBOSE);

    companion object {
        fun getLevel(level: Int) : LogLevel {
            return when(level) {
                in 700..Int.MAX_VALUE -> FATAL
                in 600..699 -> SEVERE
                in 500..599 -> WARNING
                in 400..499 -> INFO
                in 300..399 -> FINE
                in 200..299 -> FINER
                else -> FINEST
            }
        }
    }
}

