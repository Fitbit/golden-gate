/**
 *
 * @file
 * @brief Mynewt implementation of the assert functions.
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Bogdan Davidoaia
 *
 * @date 2017-11-20
 *
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdlib.h>

#include "xp/common/gg_port.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
extern void
__assert_func(const char *file, int line, const char *func, const char *e)
    __attribute((noreturn));

void
_gg_assert_func(const char *file, int line, const char *func, const char *mesg)
{
    __assert_func(file, line, func, mesg);
}
