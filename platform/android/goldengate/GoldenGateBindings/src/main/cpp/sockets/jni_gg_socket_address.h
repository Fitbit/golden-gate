// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

#include <jni.h>
#include <xp/sockets/gg_sockets.h>

#ifndef JNI_GG_SOCKET_ADDRESS_H
#define JNI_GG_SOCKET_ADDRESS_H

extern "C" {

// class names
#define JAVA_INET_ADDRESS "java/net/InetAddress"
#define JAVA_INET4_ADDRESS "java/net/Inet4Address"

// method names
#define JAVA_INET_ADDRESS_GET_ADDRESS_NAME "getAddress"

// method signature
#define JAVA_INET_ADDRESS_GET_ADDRESS_SIG "()[B"

/**
 * Created GG_IpAddress from given [Inet4Address] object
 *
 * @param inet_address_object instance of kotlin [Inet4Address] object
 * @return new XP GG_IpAddress instance
 */
GG_IpAddress GG_IpAddress_From_Inet4Address_Object(JNIEnv *env, jobject inet_address_object);

}
#endif // JNI_GG_SOCKET_ADDRESS_H
