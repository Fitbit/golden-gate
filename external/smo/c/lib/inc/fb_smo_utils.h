/**
*
* @file: fb_smo_utils.h
*
* @copyright
* Copyright 2016-2020 Fitbit, Inc
* SPDX-License-Identifier: Apache-2.0
*
* @author Gilles Boccon-Gibod
*
* @date 2016-11-05
*
* @details
*
* Simple Message Object Serialization/Deserialization
*
*/

#ifndef FB_SMO_UTILS_H
#define FB_SMO_UTILS_H

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include "fb_smo.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    Fb_SmoAllocator base;
    uint8_t*        blocks;
    unsigned int    size;
    unsigned int    block_size;
    unsigned int    blocks_used;
} Fb_SmoSimpleBlockAllocator;

typedef struct {
    Fb_SmoAllocator base;
    uint8_t*        heap;
    unsigned int    size;
    unsigned int    used;
} Fb_SmoGrowOnlyAllocator;

/*----------------------------------------------------------------------
|    prototypes
+---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void Fb_SmoSimpleBlockAllocator_Init(Fb_SmoSimpleBlockAllocator* allocator, uint8_t* blocks, unsigned int size);
void Fb_SmoGrowOnlyAllocator_Init(Fb_SmoGrowOnlyAllocator* allocator, uint8_t* heap, unsigned int size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FB_SMO_UTILS_H */
