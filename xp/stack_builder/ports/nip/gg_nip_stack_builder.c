/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2020 Fitbit, Inc
 * SPDX-License-Identifier: Apache-2.0
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2020-02-25
 *
 * @details
 *
 * Stack builder elements for NIP.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_common.h"
#include "xp/stack_builder/gg_stack_builder.h"
#include "xp/stack_builder/gg_stack_builder_base.h"
#include "xp/nip/gg_nip.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * NIP network interface element
 */
struct GG_StackNetworkInterfaceElement {
    GG_StackElement base;
};

//----------------------------------------------------------------------
void
GG_StackNetworkInterfaceElement_Destroy(GG_StackNetworkInterfaceElement* self)
{
    GG_COMPILER_UNUSED(self);
}

//----------------------------------------------------------------------
GG_Result
GG_StackNetworkInterfaceElement_Create(GG_Stack*                         stack,
                                       size_t                            netif_mtu,
                                       GG_StackNetworkInterfaceElement** element)
{
    // allocate the element
    GG_StackNetworkInterfaceElement* self = GG_AllocateZeroMemory(sizeof(GG_StackNetworkInterfaceElement));
    *element = self;
    if (self == NULL) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // initialize the base
    self->base.stack = stack;
    self->base.type  = GG_STACK_ELEMENT_TYPE_IP_NETWORK_INTERFACE;

    // configure NIP
    GG_Nip_Configure(&stack->ip_configuration.local_address);

    // setup the ports
    self->base.bottom_port.source = GG_Nip_AsDataSource();
    self->base.bottom_port.sink   = GG_Nip_AsDataSink();

    return GG_SUCCESS;
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
void
GG_StackNetworkInterfaceElement_Inspect(GG_StackNetworkInterfaceElement* self, GG_Inspector* inspector)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(inspector);
}
#endif
