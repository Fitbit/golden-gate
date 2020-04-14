/**
 * @file
 * @brief String objects and functions
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 */
 
#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>

#include "xp/common/gg_memory.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_types.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

//! @addtogroup Strings
//! General purpose strings
//! @{

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_STRING_SEARCH_FAILED (-1)
#define GG_EMPTY_STRING {0}

extern const char* const GG_String_EmptyString;

/*----------------------------------------------------------------------
|   plain C strings
+---------------------------------------------------------------------*/
#define GG_StringsEqual(s1, s2) (strcmp((s1), (s2)) == 0)

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    char* chars;
} GG_String;

typedef struct {
    size_t length;
    size_t allocated;
    /* the actual string characters follow */
} GG_StringBuffer;

/*----------------------------------------------------------------------
|   GG_String inline functions as macros
+---------------------------------------------------------------------*/
#define GG_String_GetBuffer(str) ( ((GG_StringBuffer*)((void*)(str)->chars))-1 )
#define GG_String_Construct(str) do { \
    (str)->chars = NULL;              \
} while(0)
#define GG_String_Destruct(str) do {                       \
    if ((str)->chars) {                                    \
        GG_FreeMemory((void*)GG_String_GetBuffer((str)));  \
        (str)->chars = NULL;                               \
    }                                                      \
} while(0)
#define GG_String_GetChar(str, index) ((str)->chars[(index)])
#define GG_String_SetChar(str, index, c) do {          \
    (str)->chars[(index)] = (c);                       \
} while(0)
#define GG_String_GetLength(str) ((str)->chars?(GG_String_GetBuffer(str)->length):0)
#define GG_String_GetChars(str) ((str)->chars?(const char*)((str)->chars):GG_String_EmptyString)
#define GG_String_UseChars(str) ((str)->chars?(str)->chars:GG_CONST_CAST(GG_String_EmptyString, char*))
#define GG_CSTR(str) GG_String_GetChars(&(str))
#define GG_String_IsEmpty(str) (GG_String_GetLength((str))==0)
#define GG_String_Init(str) do {(str).chars = NULL; } while(0)

/*----------------------------------------------------------------------
|   GG_String functions
+---------------------------------------------------------------------*/
GG_String 
GG_String_Create(const char* s);

GG_String 
GG_String_CreateFromSubString(const char* s, unsigned int first, size_t length);

GG_String 
GG_String_Clone(const GG_String* str);

void
GG_String_Copy(GG_String* str, const GG_String* other);

GG_Result 
GG_String_SetLength(GG_String* str, size_t length);

GG_Result 
GG_String_Assign(GG_String* str, const char* chars);

GG_Result 
GG_String_AssignN(GG_String* str, const char* chars, size_t size);

GG_Result 
GG_String_Append(GG_String* str, const char* other);

GG_Result 
GG_String_AppendSubString(GG_String* str, const char* other, size_t size);

GG_Result 
GG_String_AppendChar(GG_String* str, char c);

GG_String
GG_String_Add(const GG_String* str1, const char* str2);

int 
GG_String_Compare(const GG_String* str, const char* s, bool ignore_case);

bool 
GG_String_Equals(const GG_String* str, const char* s, bool ignore_case);

GG_String 
GG_String_SubString(const GG_String* str, unsigned int first, size_t length);

GG_String 
GG_String_Left(const GG_String* str, size_t length);

GG_String 
GG_String_Right(const GG_String* str, size_t length);

GG_Result
GG_String_Reserve(GG_String* str, size_t length);

GG_String
GG_String_ToLowercase(const GG_String* str);

GG_String
GG_String_ToUppercase(const GG_String* str);

GG_Result
GG_String_ToInteger(const GG_String* str, int* value);

void
GG_String_MakeLowercase(GG_String* str);

void
GG_String_MakeUppercase(GG_String* str);

void
GG_String_Replace(GG_String* str, char a, char b);

int  
GG_String_FindChar(const GG_String* str, char c);

int  
GG_String_FindCharFrom(const GG_String* str, char c, unsigned int start);

int  
GG_String_FindString(const GG_String* str, const char* s);

int  
GG_String_FindStringFrom(const GG_String* str, const char* s, unsigned int start);

int  
GG_String_ReverseFindChar(const GG_String* str, char c);

int  
GG_String_ReverseFindCharFrom(const GG_String* str, char c, unsigned int start);

int  
GG_String_ReverseFindString(const GG_String* str, const char* s);

int  
GG_String_ReverseFindStringFrom(const GG_String* str, const char* s, unsigned int start);

bool
GG_String_StartsWith(const GG_String* str, const char* s);

bool 
GG_String_EndsWith(const GG_String* str, const char* s);

void
GG_String_TrimWhitespaceLeft(GG_String* str);

void
GG_String_TrimCharLeft(GG_String* str, char c);

void
GG_String_TrimCharsLeft(GG_String* str, const char* chars);

void
GG_String_TrimWhitespaceRight(GG_String* str);

void
GG_String_TrimCharRight(GG_String* str, char c);

void
GG_String_TrimCharsRight(GG_String* str, const char* chars);

void
GG_String_TrimWhitespace(GG_String* str);

void
GG_String_TrimChar(GG_String* str, char c);

void
GG_String_TrimChars(GG_String* str, const char* chars);

GG_Result
GG_String_Insert(GG_String* str, const char* s, unsigned int where);

//! @}

#ifdef __cplusplus
}
#endif /* __cplusplus */

