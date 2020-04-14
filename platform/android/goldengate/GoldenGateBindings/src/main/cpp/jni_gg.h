#include <jni.h>

#ifndef GOLDENGATEHOST_JNI_GG_H
#define GOLDENGATEHOST_JNI_GG_H

extern "C"
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_initModulesJNI(
        JNIEnv *env,
        jobject thiz/* this */);

extern "C"
JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_getVersionJNI(
        JNIEnv *env,
        jobject /* this */,
        jclass clazz);

#endif // GOLDENGATEHOST_JNI_GG_H
