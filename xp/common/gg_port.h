/**
 * @file
 * @brief Cross-platform and compiler portability support
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
 * Collection of portability macros.
 * All platform and compiler specific feature testing and definitions
 * must reside here.
 */

#pragma once

//! @addtogroup Portability Portability
//! Cross-platform and compiler portability support
//! @{

/*----------------------------------------------------------------------
|   platform constants
+---------------------------------------------------------------------*/
#define GG_PLATFORM_ANY         0 ///< Generic
#define GG_PLATFORM_BISON       1 ///< Bison FW platform
#define GG_PLATFORM_LINUX       2 ///< Linux platform
#define GG_PLATFORM_WINDOWS     3 ///< Windows platform
#define GG_PLATFORM_NUTTX       4 ///< NuttX platform
#define GG_PLATFORM_PYLON       5 ///< Pylon platform
#define GG_PLATFORM_DARKHORSE   6 ///< Darkhorse platform

// platform auto-detection
#if !defined(GG_CONFIG_PLATFORM)
#if defined(_WIN32) || defined(_WIN64)
#define GG_CONFIG_PLATFORM GG_PLATFORM_WINDOWS
#else
#define GG_CONFIG_PLATFORM GG_PLATFORM_ANY
#endif
#endif

/*----------------------------------------------------------------------
|   initial defaults
+---------------------------------------------------------------------*/
//! CPU Byte order. Assume little endian by default as this is the most common
#define GG_CONFIG_CPU_BYTE_ORDER GG_CPU_LITTLE_ENDIAN

#define GG_CONFIG_HAVE_STD_C    ///< The platform/compiler has Standard C support
#define GG_CONFIG_HAVE_STDLIB_H ///< The platform/compiler has stdlib.h
#define GG_CONFIG_HAVE_STRING_H ///< The platform/compiler has string.h
#define GG_CONFIG_HAVE_STDIO_H  ///< The platform/compiler has stdio.h
#define GG_CONFIG_HAVE_STDARG_H ///< The platform/compiler has stdarg.h
#define GG_CONFIG_HAVE_STDDEF_H ///< The platform/compiler has stddef.h
#define GG_CONFIG_HAVE_CTYPE_H  ///< The platform/compiler has ctype.h
#define GG_CONFIG_HAVE_MATH_H   ///< The platform/compiler has math.h
#define GG_CONFIG_HAVE_ASSERT_H ///< The platform/compiler has assert.h
#define GG_CONFIG_HAVE_LIMITS_H ///< The platform/compiler has limits.h

#define GG_CONFIG_HAVE_SOCKADDR_SA_LEN ///< The platform's sockets have sa_len

#if defined(GG_CONFIG_HAVE_STD_C)
#define GG_CONFIG_HAVE_MALLOC   ///< The platform/compiler has malloc
#define GG_CONFIG_HAVE_CALLOC   ///< The platform/compiler has calloc
#define GG_CONFIG_HAVE_REALLOC  ///< The platform/compiler has realloc
#define GG_CONFIG_HAVE_FREE     ///< The platform/compiler has free
#define GG_CONFIG_HAVE_MEMCPY   ///< The platform/compiler has memcpy
#define GG_CONFIG_HAVE_MEMMOVE  ///< The platform/compiler has memmove
#define GG_CONFIG_HAVE_MEMSET   ///< The platform/compiler has memset
#define GG_CONFIG_HAVE_MEMCMP   ///< The platform/compiler has memcmp
#define GG_CONFIG_HAVE_ATEXIT   ///< The platform/compiler has atexit
#define GG_CONFIG_HAVE_GETENV   ///< The platform/compiler has getenv
#endif /* GG_CONFIG_HAS_STD_C */

/*----------------------------------------------------------------------
|   useful macros
+---------------------------------------------------------------------*/
#define GG_CONST_CAST(var, type) ((type)(uintptr_t)(var)) ///< cast-away constness (use with care!)

/*----------------------------------------------------------------------
|   CPU byte order
+---------------------------------------------------------------------*/
#define GG_CPU_BIG_ENDIAN    1  ///< Big-endian byte order
#define GG_CPU_LITTLE_ENDIAN 2  ///< Little-endian byte order

/*----------------------------------------------------------------------
|   platform specifics
+---------------------------------------------------------------------*/
#if defined(__linux__)
#define GG_CONFIG_HAVE_GETADDRINFO
#undef GG_CONFIG_HAVE_SOCKADDR_SA_LEN
#endif

#if defined(__APPLE__)
#define GG_CONFIG_HAVE_GETADDRINFO
#endif

#if defined(__ANDROID__)
#define GG_CONFIG_HAVE_GETADDRINFO
#undef GG_CONFIG_HAVE_SOCKADDR_SA_LEN
#endif

/*----------------------------------------------------------------------
|   assert
+---------------------------------------------------------------------*/
void
__gg_assert_func(const char *file, int line, const char *func, const char *mesg);

#if GG_CONFIG_PLATFORM == GG_PLATFORM_PYLON
#define GG_ASSERT(x) ((x) ? (void)0 : __gg_assert_func(NULL, 0, NULL, NULL))

#elif GG_CONFIG_PLATFORM == GG_PLATFORM_BISON
#include <sys/cdefs.h>

#ifndef SEC_ASSERT_STRINGS
#define SEC_ASSERT_STRINGS "ASSERT_STRINGS"
#endif

#ifndef GG_BASE_FILE_NAME
#if defined(__BASE_FILE__)
// __BASE_FILE__ will use the name of the input file (foo.c), rather than the
// current file (this file, gg_port.h). it also has the advantage of using the
// path passed to the compiler in this case, instead of the absolute path of the
// header. this prevents absolute path segments from being inserted into the
// final binary inside the logging base file name; this is important because it
// keeps binary sizes the same no matter what the base build directory happened
// to be.
// Only available on __GNUC__ compilers (gcc, clang).
#define GG_BASE_FILE_NAME __BASE_FILE__
#else
#define GG_BASE_FILE_NAME __FILE__
#endif
#endif

static const char _gg_assert_base_file[] __section(SEC_ASSERT_STRINGS) = GG_BASE_FILE_NAME;

#define GG_ASSERT(x) \
    ((x) ? ((void)0) : __gg_assert_func(_gg_assert_base_file, __LINE__, NULL, NULL))

#elif defined(GG_CONFIG_HAVE_ASSERT_H)
#include <assert.h>
#define GG_ASSERT(x) assert(x)
#else
#error "Platform needs to support assert!"
#endif

/*----------------------------------------------------------------------
|   compiler specifics
+---------------------------------------------------------------------*/
/* defaults */
#define GG_COMPILER_UNUSED(var) (void)var ///< Macro used to signal that a function parameter isn't used

/* GCC */
#if defined(__GNUC__)
#define GG_LocalFunctionName __FUNCTION__
#define GG_COMPILER_ATTRIBUTE_PACKED __attribute__((packed))
#endif

/* Clang */
#if defined(__clang__)
#define GG_LocalFunctionName __FUNCTION__
#define GG_COMPILER_ATTRIBUTE_PACKED __attribute__((packed))
#endif

/* Microsoft Visual C */
#if defined(_MSC_VER)
#undef GG_CONFIG_HAVE_GETENV
#define GG_CONFIG_HAVE_DUPENV_S
#define dupenv_s _dupenv_s
#define strdup _strdup
#endif

/*----------------------------------------------------------------------
|   final defaults
+---------------------------------------------------------------------*/
#if !defined(GG_LocalFunctionName)
#define GG_LocalFunctionName (NULL)
#endif

#if !defined(GG_COMPILER_ATTRIBUTE_PACKED)
#define GG_COMPILER_ATTRIBUTE_PACKED
#endif

//! @}
