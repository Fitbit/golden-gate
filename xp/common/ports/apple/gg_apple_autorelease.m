/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-04-26
 *
 * @details
 *
 * Apple Autorelease support
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_results.h"
#include "xp/common/gg_memory.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
GG_Result
GG_AutoreleaseWrap(GG_Result (*function)(void* arg), void* arg)
{
    @autoreleasepool {
        return (*function)(arg);
    }
}
