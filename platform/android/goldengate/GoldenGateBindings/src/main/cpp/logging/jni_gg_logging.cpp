// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <cassert>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include "third_party/goldengate/xp/common/gg_logging.h"
#include "third_party/goldengate/xp/common/gg_memory.h"


extern "C" {

typedef struct {
    GG_IMPLEMENTS(GG_LogHandler);
} GG_LogAndroidHandler;

struct Logger {
    JavaVM *jvm;
    jobject receiver;
    jmethodID ggLogCallback;
    jmethodID jniLogCallback;
};

Logger* globalLogger;

//TODO: Figure out how to combine the differences between normal JNI and Android JNI.h files
#define static JNIEnv* getEnv(bool *shouldDetach);

#ifdef __ANDROID__
static JNIEnv* getEnv(bool *shouldDetach) {
    JNIEnv *env;
    *shouldDetach = false;
    jint envProp = globalLogger->jvm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (envProp == JNI_EDETACHED) {
        jint rs = globalLogger->jvm->AttachCurrentThread(&env, NULL);
        *shouldDetach = true;
        assert(rs == JNI_OK);
    }
    return env;
}

#else
static JNIEnv* getEnv(bool *shouldDetach) {
    void *env;
    *shouldDetach = false;
    jint envProp = globalLogger->jvm->GetEnv(&env, JNI_VERSION_1_6);
    if (envProp == JNI_EDETACHED) {
        jint rs = globalLogger->jvm->AttachCurrentThread(&env, NULL);
        *shouldDetach = true;
        assert(rs == JNI_OK);
    }
    return (JNIEnv *) env;
}
#endif

static void GG_AndroidLogHandler_Log(GG_LogHandler *_self, const GG_LogRecord *record) {
    if (!globalLogger) {
        return; // if a logger isn't set up, don't try to log
    }

    // only log strings for now
    if (record->message_type != GG_LOG_MESSAGE_TYPE_STRING) {
        return;
    }

    bool shouldDetach;
    JNIEnv *env = getEnv(&shouldDetach);

    jstring filename = env->NewStringUTF(record->source_file);
    jstring function = env->NewStringUTF(record->source_function);
    jstring message = env->NewStringUTF((const char*)record->message);
    jint level = record->level;
    jint line = record->source_line;
    jlong timestamp = record->timestamp;
    jstring loggername = env->NewStringUTF(record->logger_name);
    env->CallVoidMethod(globalLogger->receiver, globalLogger->ggLogCallback, message, level, timestamp,
                        loggername, filename, function, line);
    /*
     * When calling back into a java method, you must free local references created with
     * NewStringUTF8 since it is not an explicit java thread and will not garbage collect.
     * If from a java thread initially, it can garbage collect it.
     */
    env->DeleteLocalRef(filename);
    env->DeleteLocalRef(function);
    env->DeleteLocalRef(message);
    env->DeleteLocalRef(loggername);

    if (shouldDetach) {
        /* TODO: Clean this up
         * this is actually a bit expensive. if we see performance issues,
         * we should look into only cleaning this up when we need to
         */
        globalLogger->jvm->DetachCurrentThread();
    }
}

void GG_Log_JNI(const char *tag, const char *fmt , ...) {
    if (!globalLogger) {
        return; // if a logger isn't set up, don't try to log
    }

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsprintf(buffer, fmt, args);
    va_end(args);

    bool shouldDetach;
    JNIEnv *env = getEnv(&shouldDetach);
    jstring tagString = env->NewStringUTF(tag);
    jstring messageString = env->NewStringUTF(buffer);
    env->CallVoidMethod(globalLogger->receiver, globalLogger->jniLogCallback, tagString, messageString);
    env->DeleteLocalRef(tagString);
    env->DeleteLocalRef(messageString);

    if (shouldDetach) {
        /* TODO: Clean this up
         * this is actually a bit expensive. if we see performance issues,
         * we should look into only cleaning this up when we need to
         */
        globalLogger->jvm->DetachCurrentThread();
    }
}


static void GG_AndroidLogHandler_Destroy(GG_LogHandler *_self) {
    GG_LogAndroidHandler *self = GG_SELF(GG_LogAndroidHandler, GG_LogHandler);
    if (self) {
        GG_FreeMemory(self);
    }
}

GG_IMPLEMENT_INTERFACE(GG_LogAndroidHandler, GG_LogHandler) {
        GG_AndroidLogHandler_Log,
        GG_AndroidLogHandler_Destroy
};

static GG_Result GG_LogAndroidHandler_Creator(const char*     handler_name,
                                              const char*     logger_name,
                                              GG_LogHandler** handler) {
    GG_Result result = GG_SUCCESS;

    /* check the handler name and only accept to create a platform handler */
    if (strcmp(handler_name, GG_LOG_PLATFORM_HANDLER_NAME)) {
        return GG_ERROR_NO_SUCH_ITEM;
    }

    /* allocate a new object */
    GG_LogAndroidHandler* self = (GG_LogAndroidHandler *) GG_AllocateZeroMemory(sizeof(GG_LogAndroidHandler));
    if (self == NULL) return GG_ERROR_OUT_OF_MEMORY;

    /* setup the interface */
    GG_SET_INTERFACE(self, GG_LogAndroidHandler, GG_LogHandler);

    /* return the new object */
    *handler = GG_CAST(self, GG_LogHandler);

    return result;
}

JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_registerLoggerJNI(
        JNIEnv *env,
        jobject /* this */) {
    GG_LogHandlerFactory factory = &GG_LogAndroidHandler_Creator;
    GG_LogManager_SetPlatformHandlerFactory(factory);
}

JNIEXPORT jlong
JNICALL
Java_com_fitbit_goldengate_bindings_logging_Logger_createLoggerJNI(
        JNIEnv *env,
        jobject thiz/* this */,
        jclass clazz,
        jstring ggMethodName,
        jstring ggMethodSignature,
        jstring jniMethodSignature) {
    Logger *logger = (Logger*) GG_AllocateZeroMemory(sizeof(Logger));
    // set up receiver and method ID
    const char* method = env->GetStringUTFChars(ggMethodName, NULL);
    const char* ggSignature =  env->GetStringUTFChars(ggMethodSignature, NULL);
    jmethodID ggCallback = env->GetMethodID(clazz, method, ggSignature);
    logger->ggLogCallback = ggCallback;
    const char* jniSignature =  env->GetStringUTFChars(jniMethodSignature, NULL);
    jmethodID jniCallback = env->GetMethodID(clazz, method, jniSignature);
    logger->jniLogCallback = jniCallback;
    JavaVM *jvm;
    jint rs = env->GetJavaVM(&jvm);
    assert(rs == JNI_OK);
    logger->jvm = jvm;
    jobject thizz = env->NewGlobalRef(thiz); // This is required to have a jobject we can share across JNI calls.
    logger->receiver = thizz;
    env->ReleaseStringUTFChars(ggMethodName, method);
    env->ReleaseStringUTFChars(ggMethodSignature, ggSignature);
    env->ReleaseStringUTFChars(jniMethodSignature, jniSignature);
    globalLogger = logger;
    return (jlong) (intptr_t) logger;
}

JNIEXPORT void
JNICALL
Java_com_fitbit_goldengate_bindings_logging_Logger_destroyLoggerJNI(JNIEnv *env,
                                                           jobject thiz/* this */,
                                                           jlong loggerPtr) {
    Logger* logger = (Logger*) (intptr_t) loggerPtr;
    if (!logger) {
        env->DeleteGlobalRef(logger->receiver);
        GG_FreeMemory(logger);
    }
}

}
