/**
*
* @file: fb_smo_serialization.h
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

#ifndef FB_SMO_SERIALIZATION_H
#define FB_SMO_SERIALIZATION_H

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include "fb_smo.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
typedef enum {
    FB_SMO_SERIALIZATION_FORMAT_CBOR
} Fb_SmoSerializationFormat;

/*----------------------------------------------------------------------
|    prototypes
+---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

int Fb_Smo_Serialize(Fb_Smo* self, Fb_SmoSerializationFormat format, uint8_t* serialized, unsigned int* size);
int Fb_Smo_Deserialize(Fb_SmoAllocator*          object_allocator,
                       Fb_SmoAllocator*          parser_allocator,
                       Fb_SmoSerializationFormat format,
                       const uint8_t*            serialized,
                       unsigned int              size,
                       Fb_Smo**                  smo);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FB_SMO_SERIALIZATION_H */
