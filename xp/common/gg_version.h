/**
 * @file
 * @brief Golden Gate Version Info
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Michael Hoyt
 *
 * @date 2017-11-29
 */

#pragma once

#include <stdint.h>

#if defined(__cplusplus)
extern "C" {
#endif

/**
 * Retrieve version info from GG library
 *
 * @param maj Major Version
 * @param min Minor Version
 * @param patch Patch Version
 * @param commit_count Number of commits made to the Branch the library was built from
 * @param commit_hash Latest commit hash of build
 * @param branch_name Branch name library was built from
 * @param build_date Date library was built
 * @param build_time Time library was built
 */
void GG_Version(uint16_t*    maj,
                uint16_t*    min,
                uint16_t*    patch,
                uint32_t*    commit_count,
                const char** commit_hash,
                const char** branch_name,
                const char** build_date,
                const char** build_time);

#if defined(__cplusplus)
}
#endif
