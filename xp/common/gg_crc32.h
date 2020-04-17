/**
 * @file
 * @brief CRC32 checksum computation
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @date 2017-09-18
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*----------------------------------------------------------------------
|    functions
+---------------------------------------------------------------------*/

//! \addtogroup Utilities
//! CRC32 algorithm
//! @{

uint32_t GG_Crc32(uint32_t crc, const void *buf, size_t size);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */
