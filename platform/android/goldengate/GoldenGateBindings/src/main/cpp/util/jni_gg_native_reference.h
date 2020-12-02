// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni.h>

#ifndef GOLDENGATELIB_JNI_GG_NATIVE_REFERENCE_H
#define GOLDENGATELIB_JNI_GG_NATIVE_REFERENCE_H

// onFree callback method names
#define JAVA_OBJECT_ON_FREE_METHOD_NAME "onFree"

// onFree callback method signature
#define JAVA_OBJECT_ON_FREE_METHOD_SIG "()V"

/**
 * Struct that stores the native memory reference and the java object that caches this native
 * reference.
 *
 * java_object will be used from native code to notify java object when native memory reference is
 * going to be freed.
 */
typedef struct {
    void *pointer;
    jobject java_object;
} NativeReferenceWrapper;


void callJavaObjectOnFreeMethod(
        JNIEnv* env,
        const char* class_name,
        jobject java_object);

#endif //GOLDENGATELIB_JNI_GG_NATIVE_REFERENCE_H
