#include "jni_gg.h"
#include <xp/module/gg_module.h>
#include <xp/common/gg_common.h>

extern "C"
JNIEXPORT jint
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_initModulesJNI(
        JNIEnv *env,
        jobject thiz/* this */) {
    return GG_Module_Initialize();
}

extern "C"
JNIEXPORT jobject
JNICALL
Java_com_fitbit_goldengate_bindings_GoldenGate_getVersionJNI(
        JNIEnv *env,
        jobject /* this */,
        jclass clazz) {
    uint16_t    maj;
    uint16_t    min;
    uint16_t    patch;
    uint32_t    commit_count;
    const char* commit_hash;
    const char* branch_name;
    const char* build_date;
    const char* build_time;
    GG_Version(&maj, &min, &patch, &commit_count, &commit_hash, &branch_name, &build_date, &build_time);
    jmethodID constructor = env->GetMethodID(
            clazz, "<init>",
            "(JJJILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    return env->NewObject(clazz, constructor, maj, min, patch, commit_count,
                          env->NewStringUTF(commit_hash),
                          env->NewStringUTF(branch_name),
                          env->NewStringUTF(build_date),
                          env->NewStringUTF(build_time));
}
