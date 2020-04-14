#pragma once

#include "gg_dtls_default_config.h"

// Remove options that are not compatible with NuttX
#define MBEDTLS_NO_PLATFORM_ENTROPY

#ifdef MBEDTLS_FS_IO
#undef MBEDTLS_FS_IO
#endif

#ifdef MBEDTLS_TIMING_C
#undef MBEDTLS_TIMING_C
#endif

#ifdef MBEDTLS_HAVEGE_C
#undef MBEDTLS_HAVEGE_C
#endif

// Remove/set options to reduce memory footprint
#undef MBEDTLS_NET_C
#undef MBEDTLS_DEBUG_C
#undef MBEDTLS_ERROR_C
#undef MBEDTLS_ERROR_STRERROR_DUMMY
#undef MBEDTLS_VERSION_C
#undef MBEDTLS_SHA1_C
#undef MBEDTLS_SHA512_C
#undef MBEDTLS_MD5_C
#define MBEDTLS_AES_ROM_TABLES
#undef MBEDTLS_SSL_MAX_CONTENT_LEN
#define MBEDTLS_SSL_MAX_CONTENT_LEN             2048

#include "mbedtls/check_config.h"
