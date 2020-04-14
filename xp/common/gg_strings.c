/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 *
 * @details
 *
 * Implementation of the string interfaces.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_strings.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_STRINGS_WHITESPACE_CHARS "\r\n\t "
const char* const GG_String_EmptyString = "";

/*----------------------------------------------------------------------
|   helpers
+---------------------------------------------------------------------*/
#define GG_UPPERCASE(x) (((x) >= 'a' && (x) <= 'z') ? (x)&0xdf : (x))
#define GG_LOWERCASE(x) (((x) >= 'A' && (x) <= 'Z') ? (x)^32   : (x))
#define GG_STRING_BUFFER_CHARS(b) ((char*)((b)+1))

/*--------------------------------------------------------------------*/
static GG_StringBuffer*
GG_StringBuffer_Allocate(size_t allocated, size_t length)
{
    size_t size = sizeof(GG_StringBuffer) + allocated + 1;
    if (size < allocated) {
        // overflow
        return NULL;
    }
    GG_StringBuffer* buffer = (GG_StringBuffer*)GG_AllocateMemory(size);

    if (buffer == NULL) {
        return NULL;
    }

    buffer->length    = length;
    buffer->allocated = allocated;

    return buffer;
}

/*--------------------------------------------------------------------*/
static char*
GG_StringBuffer_Create(size_t length)
{
    /* allocate a buffer of the requested size */
    GG_StringBuffer* buffer = GG_StringBuffer_Allocate(length, length);
    return GG_STRING_BUFFER_CHARS(buffer);
}

/*--------------------------------------------------------------------*/
static char*
GG_StringBuffer_CreateFromString(const char* str)
{
    /* allocate a buffer of the same size as str */
    size_t length = strlen(str);
    GG_StringBuffer* buffer = GG_StringBuffer_Allocate(length, length);
    if (buffer == NULL) {
        return NULL;
    }

    char* result = GG_STRING_BUFFER_CHARS(buffer);

    /* copy the string in the new buffer */
    memcpy(result, str, length + 1);

    return result;
}

/*--------------------------------------------------------------------*/
static char*
GG_StringBuffer_CreateFromStringN(const char* str, size_t length)
{
    /* allocate a buffer of the requested size */
    GG_StringBuffer* buffer = GG_StringBuffer_Allocate(length, length);
    if (buffer == NULL) {
        return NULL;
    }

    char* result = GG_STRING_BUFFER_CHARS(buffer);

    /* copy the string in the new buffer */
    memcpy(result, str, length);

    /* add a null-terminator */
    result[length] = '\0';

    return result;
}

/*--------------------------------------------------------------------*/
GG_String
GG_String_Create(const char* str)
{
    GG_String result;
    if (str == NULL || str[0] == '\0') {
        result.chars = NULL;
    } else {
        result.chars = GG_StringBuffer_CreateFromString(str);
    }

    return result;
}

/*--------------------------------------------------------------------*/
GG_String
GG_String_CreateFromSubString(const char*  str,
                              unsigned int first,
                              size_t       length)
{
    GG_String result;

    /* shortcut */
    if (str != NULL && length != 0 && first < strlen(str)) {
        /* possibly truncate length */
        size_t str_length = 0;
        const char* src_str = str + first;
        while (*src_str) {
            ++str_length;
            ++src_str;
            if (str_length >= length) break;
        }
        if (str_length != 0) {
            result.chars = GG_StringBuffer_CreateFromStringN(str+first, str_length);
            return result;
        }
    }
    result.chars = NULL;

    return result;
}

/*--------------------------------------------------------------------*/
GG_String
GG_String_Clone(const GG_String* self)
{
    GG_String result;
    if (self->chars == NULL) {
        result.chars = NULL;
    } else {
        size_t length = GG_String_GetLength(self);
        result.chars = GG_StringBuffer_Create(length);
        memcpy(result.chars, self->chars, length + 1);
    }

    return result;
}

/*--------------------------------------------------------------------*/
static void
GG_String_Reset(GG_String* self)
{
    if (self->chars != NULL) {
        GG_FreeMemory((void*)GG_String_GetBuffer(self));
        self->chars = NULL;
    }
}

/*--------------------------------------------------------------------*/
static char*
GG_String_PrepareToWrite(GG_String* self, size_t length)
{
    if (self->chars == NULL || GG_String_GetBuffer(self)->allocated < length) {
        /* the buffer is too small, we need to allocate a new one */
        size_t needed = length;
        if (self->chars != NULL) {
            size_t grow = GG_String_GetBuffer(self)->allocated*2;
            if (grow > length) needed = grow;
            GG_FreeMemory((void*)GG_String_GetBuffer(self));
        }
        self->chars = GG_STRING_BUFFER_CHARS(GG_StringBuffer_Allocate(needed, length));
    } else {
        GG_String_GetBuffer(self)->length = length;
    }
    return self->chars;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_Reserve(GG_String* self, size_t allocate)
{
    if (self->chars == NULL || GG_String_GetBuffer(self)->allocated < allocate) {
        /* the buffer is too small, we need to allocate a new one */
        size_t needed = allocate;
        if (self->chars != NULL) {
            size_t grow = GG_String_GetBuffer(self)->allocated*2;
            if (grow > allocate) needed = grow;
        }

        size_t length = GG_String_GetLength(self);
        char* copy = GG_STRING_BUFFER_CHARS(GG_StringBuffer_Allocate(needed, length));
        if (copy == NULL) {
            return GG_ERROR_OUT_OF_MEMORY;
        }
        if (self->chars != NULL) {
            memcpy(copy, self->chars, length + 1);
            GG_FreeMemory(GG_String_GetBuffer(self));
        } else {
            copy[0] = '\0';
        }
        self->chars = copy;
    }

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_Assign(GG_String* self, const char* str)
{
    if (str == NULL) {
        GG_String_Reset(self);
        return GG_SUCCESS;
    } else {
        return GG_String_AssignN(self, str, strlen(str));
    }
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_AssignN(GG_String* self, const char* str, size_t length)
{
    if (str == NULL || length == 0) {
        GG_String_Reset(self);
    } else {
        GG_String_PrepareToWrite(self, length);
        memcpy(self->chars, str, length);
        self->chars[length] = '\0';
    }

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
void
GG_String_Copy(GG_String* self, const GG_String* str)
{
    if (str == NULL || str->chars == NULL) {
        GG_String_Reset(self);
    } else {
        size_t length = GG_String_GetBuffer(str)->length;
        if (length == 0) {
            GG_String_Reset(self);
        } else {
            GG_String_PrepareToWrite(self, length);
            memcpy(self->chars, str->chars, length + 1);
        }
    }
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_SetLength(GG_String* self, size_t length)
{
    if (self->chars == NULL) {
        return (length == 0 ? GG_SUCCESS : GG_ERROR_INVALID_PARAMETERS);
    }
    if (length <= GG_String_GetBuffer(self)->allocated) {
        char* chars = GG_String_UseChars(self);
        GG_String_GetBuffer(self)->length = length;
        chars[length] = '\0';
        return GG_SUCCESS;
    } else {
        return GG_ERROR_INVALID_PARAMETERS;
    }
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_Append(GG_String* self, const char* str)
{
    /* shortcut */
    if (str == NULL || str[0] == '\0') {
        return GG_SUCCESS;
    }
    return GG_String_AppendSubString(self, str, strlen(str));
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_AppendChar(GG_String* self, char c)
{
    return GG_String_AppendSubString(self, &c, 1);
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_AppendSubString(GG_String* self, const char* str, size_t length)
{
    /* shortcut */
    if (str == NULL || length == 0) return GG_SUCCESS;

    /* compute the new length */
    size_t old_length = GG_String_GetLength(self);
    size_t new_length = old_length + length;

    /* allocate enough space */
    GG_Result result = (GG_String_Reserve(self, new_length));
    if (GG_FAILED(result)) return result;

    /* append the new string at the end of the current one */
    memcpy(self->chars+old_length, str, length);

    /* set the length and null-terminate */
    GG_String_GetBuffer(self)->length = new_length;
    self->chars[new_length] = '\0';

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
int
GG_String_Compare(const GG_String* self, const char *s, bool ignore_case)
{
    const char *r1 = GG_String_GetChars(self);
    const char *r2 = s;

    if (ignore_case) {
        while (GG_UPPERCASE(*r1) == GG_UPPERCASE(*r2)) {
            if (*r1++ == '\0') {
                return 0;
            }
            r2++;
        }
        return GG_UPPERCASE(*r1) - GG_UPPERCASE(*r2);
    } else {
        while (*r1 == *r2) {
            if (*r1++ == '\0') {
                return 0;
            }
            r2++;
        }
        return (*r1 - *r2);
    }
}

/*--------------------------------------------------------------------*/
bool
GG_String_Equals(const GG_String* self, const char *s, bool ignore_case)
{
    return GG_String_Compare(self, s, ignore_case) == 0 ? true : false;
}

/*--------------------------------------------------------------------*/
GG_String
GG_String_SubString(const GG_String* self, unsigned int first, size_t length)
{
    return GG_String_CreateFromSubString(GG_String_GetChars(self),
                                         first,
                                         length);
}

/*----------------------------------------------------------------------
|    returns:
|   1 if str starts with sub,
|   0 if str is large enough but does not start with sub
|  -1 if str is too short to start with sub
+---------------------------------------------------------------------*/
static int
GG_StringStartsWith(const char* str, const char* sub)
{
    if (str == NULL || sub == NULL) {
        return 0;
    }
    while (*str == *sub) {
        if (*str++ == '\0') {
            return 1;
        }
        sub++;
    }
    return (*sub == '\0') ? 1 : (*str == '\0' ? -1 : 0);
}

/*--------------------------------------------------------------------*/
bool
GG_String_StartsWith(const GG_String* self, const char *s)
{
    return (GG_StringStartsWith(GG_String_GetChars(self), s) == 1) ? true : false;
}

/*--------------------------------------------------------------------*/
bool
GG_String_EndsWith(const GG_String* self, const char *s)
{
    size_t str_length;
    if (s == NULL) {
        return false;
    }
    if (*s == '\0') {
        return true;
    }
    str_length = strlen(s);
    if (str_length > GG_String_GetLength(self)) {
        return false;
    }
    return (GG_StringStartsWith(self->chars + GG_String_GetLength(self) - str_length, s) == 1) ? true : false;
}

/*--------------------------------------------------------------------*/
int
GG_String_FindStringFrom(const GG_String* self,
                         const char*      str,
                         unsigned int     start)
{
    /* check args */
    if (str == NULL || start >= GG_String_GetLength(self)) {
        return -1;
    }

    /* skip to start position */
    const char* src = self->chars + start;

    /* look for a substring */
    while (*src) {
        int cmp = GG_StringStartsWith(src, str);
        if (cmp == -1) {
            /* src is too short, abort */
            return -1;
        }
        if (cmp == 1) {
            /* match */
            return (int)(src - self->chars);
        }

        /* keep looking */
        src++;
    }

    return -1;
}

/*--------------------------------------------------------------------*/
int
GG_String_FindString(const GG_String* self, const char* str)
{
    return GG_String_FindStringFrom(self, str, 0);
}

/*--------------------------------------------------------------------*/
int
GG_String_FindCharFrom(const GG_String* self, char c, unsigned int start)
{
    /* check args */
    if (start >= GG_String_GetLength(self)) {
        return -1;
    }

    /* skip to start position */
    const char* src = self->chars + start;

    /* look for the character */
    while (*src) {
        if (*src == c) return (int)(src-self->chars);
        src++;
    }

    return -1;
}

/*--------------------------------------------------------------------*/
int
GG_String_FindChar(const GG_String* self, char c)
{
    return GG_String_FindCharFrom(self, c, 0);
}

/*--------------------------------------------------------------------*/
int
GG_String_ReverseFindCharFrom(const GG_String* self, char c, unsigned int start)
{
    const char* src = GG_String_GetChars(self);

    /* check args */
    size_t length = GG_String_GetLength(self);
    int i = (int)(length - start - 1);
    if (i < 0) return -1;

    /* look for the character */
    for (; i >= 0; i--) {
        if (src[i] == c) {
            return i;
        }
    }

    return -1;
}

/*--------------------------------------------------------------------*/
int
GG_String_ReverseFindChar(const GG_String* self, char c)
{
    return GG_String_ReverseFindCharFrom(self, c, 0);
}

/*--------------------------------------------------------------------*/
int
GG_String_ReverseFindString(const GG_String* self, const char* s)
{
    /* check args */
    size_t my_length = GG_String_GetLength(self);
    size_t s_length  = strlen(s);
    int i = (int)(my_length - s_length);
    if (i < 0) {
        return -1;
    }

    /* look for the string */
    const char* src = GG_String_GetChars(self);
    for (; i >= 0; i--) {
        int cmp = GG_StringStartsWith(src + i, s);
        if (cmp == 1) {
            /* match */
            return i;
        }
    }

    return -1;
}

/*--------------------------------------------------------------------*/
void
GG_String_MakeLowercase(GG_String* self)
{
    /* the source is the current buffer */
    char* src = GG_String_UseChars(self);

    /* convert all the characters of the existing buffer */
    char* dst = src;
    while (*dst != '\0') {
        *dst = GG_LOWERCASE(*dst);
        dst++;
    }
}

/*--------------------------------------------------------------------*/
void
GG_String_MakeUppercase(GG_String* self)
{
    /* the source is the current buffer */
    char* src = GG_String_UseChars(self);

    /* convert all the characters of the existing buffer */
    char* dst = src;
    while (*dst != '\0') {
        *dst = GG_UPPERCASE(*dst);
        dst++;
    }
}

/*--------------------------------------------------------------------*/
GG_String
GG_String_ToLowercase(const GG_String* self)
{
    GG_String result = GG_String_Clone(self);
    GG_String_MakeLowercase(&result);
    return result;
}

/*--------------------------------------------------------------------*/
GG_String
GG_String_ToUppercase(const GG_String* self)
{
    GG_String result = GG_String_Clone(self);
    GG_String_MakeUppercase(&result);
    return result;
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_ToInteger(const GG_String* self, int* value)
{
    char* end = NULL;
    const char* str = GG_CSTR(*self);
    long result = strtol(str, &end, 10);

    // check the result
    if (*end != '\0' || end == str) {
        return GG_ERROR_INVALID_SYNTAX;
    }

    // check against bounds
    if (result > INT_MAX || result < INT_MIN) {
        return GG_ERROR_OVERFLOW;
    }

    *value = (int)result;

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
void
GG_String_Replace(GG_String* self, char a, char b)
{
    /* check args */
    if (self->chars == NULL || a == '\0' || b == '\0') {
        return;
    }

    /* we are going to modify the characters */
    char* src = self->chars;

    /* process the buffer in place */
    while (*src) {
        if (*src == a) *src = b;
        src++;
    }
}

/*--------------------------------------------------------------------*/
GG_Result
GG_String_Insert(GG_String* self, const char* str, unsigned int where)
{
    size_t str_length;
    size_t old_length;
    size_t new_length;

    /* check args */
    if (str == NULL) {
        return GG_SUCCESS;
    }
    if (where > GG_String_GetLength(self)) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    /* measure the string to insert */
    str_length = strlen(str);
    if (str_length == 0) {
        return GG_SUCCESS;
    }

    /* compute the size of the new string */
    old_length = GG_String_GetLength(self);
    new_length = str_length + GG_String_GetLength(self);

    /* prepare to write the new string */
    char* src = self->chars;
    char* nst = GG_StringBuffer_Create(new_length);
    char* dst = nst;

    /* check for errors */
    if (nst == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    /* copy the beginning of the old string */
    if (where > 0) {
        memcpy(dst, src, where);
        src += where;
        dst += where;
    }

    /* copy the inserted string */
    memcpy(dst, str, str_length + 1);
    dst += str_length;

    /* copy the end of the old string */
    if (old_length > where) {
        memcpy(dst, src, strlen(src) + 1);
    }

    /* use the new string */
    if (self->chars) {
        GG_FreeMemory((void*)GG_String_GetBuffer(self));
    }
    self->chars = nst;

    return GG_SUCCESS;
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimWhitespaceLeft(GG_String* self)
{
    GG_String_TrimCharsLeft(self, GG_STRINGS_WHITESPACE_CHARS);
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimCharLeft(GG_String* self, char c)
{
    char s[2];
    s[0] = c;
    s[1] = 0;
    GG_String_TrimCharsLeft(self, (const char*)s);
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimCharsLeft(GG_String* self, const char* chars)
{
    const char* s;
    char        c;

    if (self->chars == NULL) return;
    s = self->chars;
    while ((c = *s)) {
        const char* x = chars;
        while (*x) {
            if (*x == c) break;
            x++;
        }
        if (*x == 0) break; /* not found */
        s++;
    }
    if (s == self->chars) {
        /* nothing was trimmed */
        return;
    }

    /* shift chars to the left */
    {
        char* d = self->chars;
        GG_String_GetBuffer(self)->length = (size_t)(GG_String_GetLength(self)-(s-d));
        while ((*d++ = *s++)) {};
    }
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimWhitespaceRight(GG_String* self)
{
    GG_String_TrimCharsRight(self, GG_STRINGS_WHITESPACE_CHARS);
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimCharRight(GG_String* self, char c)
{
    char s[2];
    s[0] = c;
    s[1] = 0;
    GG_String_TrimCharsRight(self, (const char*)s);
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimCharsRight(GG_String* self, const char* chars)
{
    if (self->chars == NULL || self->chars[0] == '\0') {
        return;
    }

    char* tail = self->chars+GG_String_GetLength(self) - 1;
    char* s = tail;
    while (s != self->chars-1) {
        const char* x = chars;
        while (*x) {
            if (*x == *s) {
                *s = '\0';
                break;
            }
            x++;
        }
        if (*x == 0) {
            break; /* not found */
        }
        s--;
    }
    if (s == tail) {
        /* nothing was trimmed */
        return;
    }
    GG_String_GetBuffer(self)->length = 1 + (int)(s-self->chars);
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimWhitespace(GG_String* self)
{
    GG_String_TrimWhitespaceLeft(self);
    GG_String_TrimWhitespaceRight(self);
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimChar(GG_String* self, char c)
{
    char s[2];
    s[0] = c;
    s[1] = 0;
    GG_String_TrimCharsLeft(self, (const char*)s);
    GG_String_TrimCharsRight(self, (const char*)s);
}

/*--------------------------------------------------------------------*/
void
GG_String_TrimChars(GG_String* self, const char* chars)
{
    GG_String_TrimCharsLeft(self, chars);
    GG_String_TrimCharsRight(self, chars);
}

/*--------------------------------------------------------------------*/
GG_String
GG_String_Add(const GG_String* s1, const char* s2)
{
    /* shortcut */
    if (s2 == NULL || s2[0] == '\0') {
        return GG_String_Clone(s1);
    }

    /* measure strings */
    size_t s1_length = GG_String_GetLength(s1);
    size_t s2_length = strlen(s2);

    /* allocate space for the new string */
    GG_String result = GG_EMPTY_STRING;
    char* start = GG_String_PrepareToWrite(&result, s1_length + s2_length);

    /* concatenate the two strings into the result */
    memcpy(start, GG_String_GetChars(s1), s1_length);
    memcpy(start + s1_length, s2, s2_length + 1);

    return result;
}
