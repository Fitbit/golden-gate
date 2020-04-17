/**
*
* @file: fb_smo.h
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

#ifndef FB_SMO_H
#define FB_SMO_H

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stdlib.h>

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct Fb_Smo          Fb_Smo;
typedef struct Fb_SmoAllocator Fb_SmoAllocator;

struct Fb_SmoAllocator {
    void* (*allocate_memory)(Fb_SmoAllocator* self, size_t size);
    void  (*free_memory)(Fb_SmoAllocator* self, void* memory);
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
typedef enum {
    FB_SMO_TYPE_OBJECT,
    FB_SMO_TYPE_ARRAY,
    FB_SMO_TYPE_STRING,
    FB_SMO_TYPE_BYTES,
    FB_SMO_TYPE_INTEGER,
    FB_SMO_TYPE_FLOAT,
    FB_SMO_TYPE_SYMBOL
} Fb_SmoType;

typedef enum {
    FB_SMO_SYMBOL_FALSE,
    FB_SMO_SYMBOL_TRUE,
    FB_SMO_SYMBOL_NULL,
    FB_SMO_SYMBOL_UNDEFINED
} Fb_SmoSymbol;

enum {
    FB_SMO_SUCCESS = 0,
    FB_SMO_ERROR_INTERNAL,
    FB_SMO_ERROR_INVALID_PARAMETERS,
    FB_SMO_ERROR_NOT_SUPPORTED,
    FB_SMO_ERROR_NOT_ENOUGH_DATA,
    FB_SMO_ERROR_NOT_ENOUGH_SPACE,
    FB_SMO_ERROR_OVERFLOW,
    FB_SMO_ERROR_OUT_OF_MEMORY,
    FB_SMO_ERROR_INVALID_FORMAT
};

/*----------------------------------------------------------------------
|    globals
+---------------------------------------------------------------------*/
extern Fb_SmoAllocator* const Fb_SmoDefaultAllocator;

/*----------------------------------------------------------------------
|    prototypes
+---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

Fb_Smo*        Fb_Smo_CreateObject(Fb_SmoAllocator* allocator);
Fb_Smo*        Fb_Smo_CreateArray(Fb_SmoAllocator* allocator);
Fb_Smo*        Fb_Smo_CreateString(Fb_SmoAllocator* allocator, const char* value, unsigned int value_length);
Fb_Smo*        Fb_Smo_CreateBytes(Fb_SmoAllocator* allocator, const uint8_t* value, unsigned int value_size);
Fb_Smo*        Fb_Smo_CreateInteger(Fb_SmoAllocator* allocator, int64_t value);
Fb_Smo*        Fb_Smo_CreateFloat(Fb_SmoAllocator* allocator, double value);
Fb_Smo*        Fb_Smo_CreateSymbol(Fb_SmoAllocator* allocator, Fb_SmoSymbol value);
Fb_Smo*        Fb_Smo_Create(Fb_SmoAllocator* allocator, const char* spec, ...);

int            Fb_Smo_Destroy(Fb_Smo* self);

unsigned int   Fb_Smo_GetChildrenCount(Fb_Smo* self);
Fb_Smo*        Fb_Smo_GetFirstChild(Fb_Smo* self);
Fb_Smo*        Fb_Smo_GetChildByName(Fb_Smo* self, const char* name);
Fb_Smo*        Fb_Smo_GetDescendantByPath(Fb_Smo* self, const char* path);
Fb_Smo*        Fb_Smo_GetNext(Fb_Smo* self);
Fb_Smo*        Fb_Smo_GetParent(Fb_Smo* self);
int            Fb_Smo_AddChild(Fb_Smo* self, const char* name, unsigned int name_length, Fb_Smo* child);

Fb_SmoType     Fb_Smo_GetType(Fb_Smo* self);
const char*    Fb_Smo_GetName(Fb_Smo* self);
const char*    Fb_Smo_GetValueAsString(Fb_Smo* self);
const uint8_t* Fb_Smo_GetValueAsBytes(Fb_Smo* self, unsigned int* size);
int64_t        Fb_Smo_GetValueAsInteger(Fb_Smo* self);
double         Fb_Smo_GetValueAsFloat(Fb_Smo* self);
Fb_SmoSymbol   Fb_Smo_GetValueAsSymbol(Fb_Smo* self);
Fb_Smo*        Fb_Smo_SetValueAsString(Fb_Smo* self, const char* value, unsigned int value_length);
Fb_Smo*        Fb_Smo_SetValueAsBytes(Fb_Smo* self, const uint8_t* value, unsigned int value_size);
Fb_Smo*        Fb_Smo_SetValueAsInteger(Fb_Smo* self, int64_t value);
Fb_Smo*        Fb_Smo_SetValueAsFloat(Fb_Smo* self, double value);
Fb_Smo*        Fb_Smo_SetValueAsSymbol(Fb_Smo* self, Fb_SmoSymbol value);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* FB_SMO_H */
