// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <util/jni_gg_native_reference.h>
#include <xp/common/gg_port.h>

void callJavaObjectOnFreeMethod(
        JNIEnv* env,
        const char* class_name,
        jobject java_object
) {
    jclass java_class = env->FindClass(class_name);
    GG_ASSERT(java_class);

    jmethodID on_free_method_id = env->GetMethodID(
            java_class,
            JAVA_OBJECT_ON_FREE_METHOD_NAME,
            JAVA_OBJECT_ON_FREE_METHOD_SIG);
    GG_ASSERT(on_free_method_id);

    env->CallVoidMethod(java_object, on_free_method_id);
}
