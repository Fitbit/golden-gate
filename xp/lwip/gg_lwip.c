/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-20
 *
 * @details
 *
 * LWIP library support.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_results.h"
#include "lwip/init.h"

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

// we declare a local prototype for this because those init functions don't
// appear in any header file, they are referenced by macro construction
extern GG_Result GG_LWIP_Init(void);
GG_Result GG_LWIP_Init(void) {
    lwip_init();
    return GG_SUCCESS;
}
