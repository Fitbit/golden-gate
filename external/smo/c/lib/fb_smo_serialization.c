/**
*
* @file: fb_smo.c
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
* Simple Message Object Model
*
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "fb_smo_serialization.h"
#include "fb_smo_cbor.h"

/*----------------------------------------------------------------------
|   Fb_Smo_Serialize
+---------------------------------------------------------------------*/
int
Fb_Smo_Serialize(Fb_Smo* self, Fb_SmoSerializationFormat format, uint8_t* serialized, unsigned int* size)
{
    switch (format) {
      case FB_SMO_SERIALIZATION_FORMAT_CBOR:
        return Fb_Smo_Serialize_CBOR(self, serialized, size);

      default:
        if (size) *size = 0;
        return FB_SMO_ERROR_NOT_SUPPORTED;
    }
}

/*----------------------------------------------------------------------
|   Fb_Smo_Deserialize
+---------------------------------------------------------------------*/
int
Fb_Smo_Deserialize(Fb_SmoAllocator*          object_allocator,
                   Fb_SmoAllocator*          parser_allocator,
                   Fb_SmoSerializationFormat format,
                   const uint8_t*            serialized,
                   unsigned int              size,
                   Fb_Smo**                  smo)
{
    /* default return value */
    *smo = NULL;

    switch (format) {
      case FB_SMO_SERIALIZATION_FORMAT_CBOR:
        return Fb_Smo_Deserialize_CBOR(object_allocator, parser_allocator, serialized, size, smo);

      default:
        return FB_SMO_ERROR_NOT_SUPPORTED;
    }
}
