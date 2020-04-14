/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-03-15
 *
 * @details
 *
 * Stack builder elements for NuttX.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "xp/common/gg_common.h"
#include "xp/stack_builder/gg_stack_builder.h"
#include "xp/stack_builder/gg_stack_builder_base.h"
#include "xp/netif/nuttx/gg_nuttx_generic_netif.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * Nuttx network interface element
 */
struct GG_StackNetworkInterfaceElement {
    GG_StackElement                  base;
    GG_NuttxGenericNetworkInterface* netif;
};

//----------------------------------------------------------------------
void
GG_StackNetworkInterfaceElement_Destroy(GG_StackNetworkInterfaceElement* self)
{
    if (self == NULL) {
        return;
    }

    if (self->netif) {
        GG_NuttxGenericNetworkInterface_Deregister(self->netif);
    }
    GG_NuttxGenericNetworkInterface_Destroy(self->netif);
    GG_FreeMemory(self);
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

    // instantiate the network interface
    GG_Result result = GG_NuttxGenericNetworkInterface_Create(netif_mtu, &self->netif);
    if (GG_FAILED(result)) {
        goto end;
    }

    // register the netif
    GG_IpAddress netif_netmask = {{ 255, 255, 255, 0 }};
    GG_NuttxGenericNetworkInterface_Register(self->netif,
                                             &stack->ip_configuration.local_address,
                                             &netif_netmask,
                                             &stack->ip_configuration.remote_address,
                                             true);

    // setup the ports
    self->base.bottom_port.source = GG_NuttxGenericNetworkInterface_AsDataSource(self->netif);
    self->base.bottom_port.sink   = GG_NuttxGenericNetworkInterface_AsDataSink(self->netif);

end:
    if (GG_FAILED(result)) {
        GG_StackNetworkInterfaceElement_Destroy(self);
        *element = NULL;
    }

    return result;
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
void
GG_StackNetworkInterfaceElement_Inspect(GG_StackNetworkInterfaceElement* self, GG_Inspector* inspector)
{
    GG_Inspector_OnInspectable(inspector, "netif", GG_NuttxGenericNetworkInterface_AsInspectable(self->netif));
}
#endif
