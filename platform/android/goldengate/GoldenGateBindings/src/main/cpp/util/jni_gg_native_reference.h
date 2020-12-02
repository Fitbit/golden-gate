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

/**
 * Build a NativeReferenceWrapper struct for a native memory reference
 * @param env A JNIEnv that can be called from the current thread.
 * @param pointer a native memory reference
 * @param java_object the java object which stores the native reference
 * @return
 */
NativeReferenceWrapper*
createNativeReferenceWrapper(
        JNIEnv* env,
        void* pointer,
        jobject java_object
);

/**
 * Call onFree callback inside java class object which implements NativeReferenceWithCallback interface
 * ino order to notify that the native memory is going to be freed
 *
 * @param env A JNIEnv that can be called from the current thread.
 * @param class_name the java class name which implements NativeReferenceWithCallback interface
 * @param java_object the java class instance associated with class_name
 */
void
callJavaObjectOnFreeMethod(
        JNIEnv* env,
        const char* class_name,
        jobject java_object
);

/**
 * Free the NativeReferenceWrapper struct
 * @param env A JNIEnv that can be called from the current thread.
 * @param wrapper the memory pointer to NativeReferenceWrapper struct
 */
void
freeNativeReferenceWrapper(
        JNIEnv* env,
        NativeReferenceWrapper* wrapper
 );

#endif //GOLDENGATELIB_JNI_GG_NATIVE_REFERENCE_H
