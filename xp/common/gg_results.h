/**
 * @file
 * @brief Master header for error codes
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-09-18
 */

#pragma once

//! @addtogroup Errors Error Codes
//! Error codes.
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Integer result value used by a wide number of functions and methods.
 *
 * Error values are always negative, so this type may be overloaded
 * to return positive values that are not considered error results.
 */
typedef int GG_Result;

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
/**
 * Test if a GG_Result value represents a failure condition
 */
#define GG_FAILED(result)              ((result) != GG_SUCCESS)

/**
 * Test if a GG_Result value represents a success condition
 */
#define GG_SUCCEEDED(result)           ((result) == GG_SUCCESS)

/**
 * Check if a GG_Result return value represents a failure, and return
 * that value if it does
 */
#define GG_CHECK(_result) do { \
    GG_Result _x = (_result);  \
    if (_x != GG_SUCCESS) {    \
        return _x;             \
    }                          \
} while(0)

/*----------------------------------------------------------------------
|    result codes
+---------------------------------------------------------------------*/
/** Result indicating that the operation or call succeeded */
#define GG_SUCCESS                     0

/** Result indicating an unspecified failure condition */
#define GG_FAILURE                     (-1)

/* general error codes */
#ifndef GG_ERROR_BASE
#define GG_ERROR_BASE                  (-10000)
#endif

#define GG_ERROR_BASE_GENERAL          (GG_ERROR_BASE - 0)          ///< Base for general purpose error codes.
#define GG_ERROR_OUT_OF_MEMORY         (GG_ERROR_BASE_GENERAL -  0) ///< Out of memory
#define GG_ERROR_OUT_OF_RESOURCES      (GG_ERROR_BASE_GENERAL -  1) ///< Out of resources
#define GG_ERROR_INTERNAL              (GG_ERROR_BASE_GENERAL -  2) ///< Internal error
#define GG_ERROR_INVALID_PARAMETERS    (GG_ERROR_BASE_GENERAL -  3) ///< Invalid Parameters
#define GG_ERROR_INVALID_STATE         (GG_ERROR_BASE_GENERAL -  4) ///< Invalid State
#define GG_ERROR_NOT_IMPLEMENTED       (GG_ERROR_BASE_GENERAL -  5) ///< Not implemented
#define GG_ERROR_OUT_OF_RANGE          (GG_ERROR_BASE_GENERAL -  6) ///< Out of range
#define GG_ERROR_ACCESS_DENIED         (GG_ERROR_BASE_GENERAL -  7) ///< Access denied
#define GG_ERROR_INVALID_SYNTAX        (GG_ERROR_BASE_GENERAL -  8) ///< Invalid Syntax
#define GG_ERROR_NOT_SUPPORTED         (GG_ERROR_BASE_GENERAL -  9) ///< Not supported
#define GG_ERROR_INVALID_FORMAT        (GG_ERROR_BASE_GENERAL - 10) ///< Invalid format
#define GG_ERROR_NOT_ENOUGH_SPACE      (GG_ERROR_BASE_GENERAL - 11) ///< Not enough space
#define GG_ERROR_NO_SUCH_ITEM          (GG_ERROR_BASE_GENERAL - 12) ///< No such item (item not found)
#define GG_ERROR_OVERFLOW              (GG_ERROR_BASE_GENERAL - 13) ///< Overflow
#define GG_ERROR_TIMEOUT               (GG_ERROR_BASE_GENERAL - 14) ///< A timeout occurred
#define GG_ERROR_WOULD_BLOCK           (GG_ERROR_BASE_GENERAL - 15) ///< The operation would block / try again later
#define GG_ERROR_PERMISSION_DENIED     (GG_ERROR_BASE_GENERAL - 16) ///< Permission denied
#define GG_ERROR_INTERRUPTED           (GG_ERROR_BASE_GENERAL - 17) ///< Operation interrupted
#define GG_ERROR_IN_USE                (GG_ERROR_BASE_GENERAL - 18) ///< Resource already in use


#define GG_ERROR_BASE_IO               (GG_ERROR_BASE - 100) ///< Base for I/O error codes
#define GG_ERROR_BASE_SOCKET           (GG_ERROR_BASE - 200) ///< Base for Socket error codes
#define GG_ERROR_BASE_COAP             (GG_ERROR_BASE - 300) ///< Base for CoAP error codes
#define GG_ERROR_BASE_REMOTE           (GG_ERROR_BASE - 400) ///< Base for Remote API error codes
#define GG_ERROR_BASE_GATTLINK         (GG_ERROR_BASE - 500) ///< Base for Gattlink error codes
#define GG_ERROR_BASE_TLS              (GG_ERROR_BASE - 600) ///< Base for TLS error codes

/* Special codes to convey an errno                      */
/* the error code is (GG_ERROR_BASE_ERRNO - errno)       */
/* where errno is the positive integer from errno.h      */
#define GG_ERROR_BASE_ERRNO            (GG_ERROR_BASE-2000)        ///< Base for `errno` error codes
#define GG_ERROR_ERRNO(e)              (GG_ERROR_BASE_ERRNO - (e)) ///< Wrapper for an `errno` error code

//! @}
