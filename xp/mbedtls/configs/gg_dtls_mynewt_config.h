#ifndef __GG_DTLS_MYNEWT_CONFIG_H__
#define __GG_DTLS_MYNEWT_CONFIG_H__

#include "gg_dtls_default_config.h"

// file copied from @apache-mynewt-core/crypto/mbedtls/include/mbedtls/config_mynewt.h
#include "config_mynewt.h"

// Remove options that are not compatible with Mynewt
#ifdef MBEDTLS_TIMING_C
#undef MBEDTLS_TIMING_C
#endif

#ifdef MBEDTLS_HAVEGE_C
#undef MBEDTLS_HAVEGE_C
#endif

// Remove options to reduce memory footprint on Mynew
#undef MBEDTLS_DEBUG_C
#undef MBEDTLS_ERROR_C
#undef MBEDTLS_ERROR_STRERROR_DUMMY
#undef MBEDTLS_VERSION_C
#undef MBEDTLS_SHA1_C
#undef MBEDTLS_SHA512_C

#undef MBEDTLS_MD5_C

#define MBEDTLS_AES_ROM_TABLES

#undef MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_MAX_CONTENT_LEN             1152

#include "mbedtls/check_config.h"

#endif /* __GG_DTLS_MYNEWT_CONFIG_H__ */
