/**
 * @file
 * @brief Mynewt TLS functions
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2018-01-16
 *
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "mbedtls/ctr_drbg.h"
#include "mbedtls/ssl.h"

#include "xp/common/gg_results.h"

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static mbedtls_ctr_drbg_context g_ctr_drbg;

/*----------------------------------------------------------------------
|   Mynewt initialization of the RNG for an mbedtls configuration
+---------------------------------------------------------------------*/
static int entropy_func(void *context, unsigned char *buf, size_t len)
{
    int num;

    while (len > 0) {
        num = rand();

        for (int i = 0; len > 0 && i < 4; i++, len--) {
            *buf = (unsigned char) num;
            num >>= 8;
            buf++;
        }
    }

  return 0;
}

GG_Result GG_mbedtls_ssl_conf_rng(mbedtls_ssl_config* ssl_config)
{
    int rc;

    mbedtls_ctr_drbg_init(&g_ctr_drbg);

    // Configure the DRNG in mbed TLS
    rc  = mbedtls_ctr_drbg_seed(&g_ctr_drbg, entropy_func, NULL, NULL, 0);
    if (rc != 0) {
        return GG_FAILURE;
    }

    // configure mbedtls
    mbedtls_ssl_conf_rng(ssl_config, mbedtls_ctr_drbg_random, &g_ctr_drbg);

    return GG_SUCCESS;
}
