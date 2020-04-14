/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-12-01
 *
 * @details
 *
 * TLS protocol shared functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "xp/common/gg_port.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_tls.h"

/*----------------------------------------------------------------------
|   thunk
+---------------------------------------------------------------------*/
GG_Result
GG_TlsKeyResolver_ResolveKey(GG_TlsKeyResolver* self,
                             const uint8_t*     key_identity,
                             size_t             key_identify_size,
                             uint8_t*           key,
                             size_t*            key_size)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->ResolveKey(self,
                                          key_identity,
                                          key_identify_size,
                                          key,
                                          key_size);
}
