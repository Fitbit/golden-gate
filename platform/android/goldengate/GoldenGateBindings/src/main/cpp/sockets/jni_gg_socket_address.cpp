#include "jni_gg_socket_address.h"
#include <string.h>
#include <stdlib.h>
#include <jni_gg_loop.h>
#include <util/jni_gg_utils.h>
#include <xp/loop/gg_loop.h>
#include <xp/common/gg_port.h>
#include <logging/jni_gg_logging.h>
#include <xp/sockets/gg_sockets.h>
#include <xp/common/gg_memory.h>

extern "C" {

GG_IpAddress GG_IpAddress_From_Inet4Address_Object(JNIEnv *env, jobject inet_address_object) {
    GG_ASSERT(env);
    GG_ASSERT(inet_address_object);

    jclass inet_address_class = env->FindClass(JAVA_INET_ADDRESS);
    GG_ASSERT(inet_address_class);
    jclass inet4_address_class = env->FindClass(JAVA_INET4_ADDRESS);
    GG_ASSERT(inet4_address_class);

    // ensure Inet4Address object
    GG_ASSERT(env->IsInstanceOf(inet_address_object, inet4_address_class));

    jmethodID get_address_id = env->GetMethodID(
            inet_address_class,
            JAVA_INET_ADDRESS_GET_ADDRESS_NAME,
            JAVA_INET_ADDRESS_GET_ADDRESS_SIG);
    GG_ASSERT(get_address_id);

    jbyteArray address_byte_array = (jbyteArray) env->CallObjectMethod(
            inet_address_object,
            get_address_id);
    GG_ASSERT(address_byte_array);
    jint address_byte_array_length = env->GetArrayLength(address_byte_array);
    GG_ASSERT(address_byte_array_length == 4);
    jbyte *address_byte = env->GetByteArrayElements(address_byte_array, NULL);

    GG_IpAddress gg_ip_address = {
            {
                    (u_int8_t) address_byte[0],
                    (u_int8_t) address_byte[1],
                    (u_int8_t) address_byte[2],
                    (u_int8_t) address_byte[3]
            }
    };

    env->DeleteLocalRef(inet_address_class);
    env->DeleteLocalRef(inet4_address_class);
    env->ReleaseByteArrayElements(address_byte_array, address_byte, JNI_ABORT);
    env->DeleteLocalRef(address_byte_array);

    return gg_ip_address;
}

}
