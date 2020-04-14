/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2017-11-13
 *
 * @details
 *
 * Generic LWIP netif implementation.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "lwip/opt.h"
#include "lwip/def.h"
#include "lwip/pbuf.h"
#include "lwip/stats.h"
#include "lwip/sys.h"
#include "lwip/ip4.h"
#include "lwip/netif.h"
#include "lwip/ip.h"

#include "xp/common/gg_common.h"
#include "xp/sockets/gg_sockets.h"
#include "gg_lwip_generic_netif.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_LwipGenericNetworkInterface {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_DataSource);
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    struct netif netif;
    GG_Loop*     loop;
    GG_DataSink* transport_sink;

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.lwip.netif")

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_LWIP_GENERIC_NETIF_DEFAULT_MTU 1280

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static err_t
LwipNetworkInterface_Output(struct netif* netif, struct pbuf* data, const ip4_addr_t* address)
{
    GG_COMPILER_UNUSED(address);
    GG_LwipGenericNetworkInterface* self = (GG_LwipGenericNetworkInterface*)netif->state;

    GG_LOG_FINE("sending packet, ADDR=%d.%d.%d.%d, size=%d",
                (int)((address->addr      ) & 0xFF),
                (int)((address->addr >>  8) & 0xFF),
                (int)((address->addr >> 16) & 0xFF),
                (int)((address->addr >> 24) & 0xFF),
                (int)data->tot_len);

    // create a buffer to copy the data into
    GG_DynamicBuffer* buffer;
    GG_Result result = GG_DynamicBuffer_Create(data->tot_len, &buffer);
    if (GG_FAILED(result)) {
        return ERR_MEM;
    }

    // copy the data
    pbuf_copy_partial(data, GG_DynamicBuffer_UseData(buffer), data->tot_len, 0);
    GG_DynamicBuffer_SetDataSize(buffer, data->tot_len);

    // try to send the packet
    if (self->transport_sink) {
        result = GG_DataSink_PutData(self->transport_sink, GG_DynamicBuffer_AsBuffer(buffer), NULL);
        if (GG_FAILED(result)) {
            GG_DynamicBuffer_Release(buffer);
            if (result == GG_ERROR_WOULD_BLOCK) {
                GG_LOG_FINEST("GG_DataSink_PutData would block");
                return ERR_WOULDBLOCK;
            } else {
                GG_LOG_WARNING("GG_DataSink_PutData failed (%d)", result);
                return ERR_IF;
            }
        }
    }

    // don't hold on to the buffer
    GG_DynamicBuffer_Release(buffer);

    return ERR_OK;
}

//----------------------------------------------------------------------
// This is not needed, so the implementation is just a stub.
//----------------------------------------------------------------------
static err_t
LwipNetworkInterface_LinkOutput(struct netif* netif, struct pbuf* p)
{
    GG_COMPILER_UNUSED(netif);
    GG_COMPILER_UNUSED(p);
    return ERR_OK;
}

//----------------------------------------------------------------------
static err_t
LwipNetworkInterface_Init(struct netif* netif)
{
    netif->name[0] = 'g';
    netif->name[1] = 'g';
#if LWIP_IPV4
    netif->output = LwipNetworkInterface_Output;
#endif /* LWIP_IPV4 */
#if LWIP_IPV6
    netif->output_ip6 = NULL;
#endif /* LWIP_IPV6 */
    netif->linkoutput = LwipNetworkInterface_LinkOutput;

    return ERR_OK;
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipGenericNetworkInterface_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_LwipGenericNetworkInterface* self = GG_SELF(GG_LwipGenericNetworkInterface, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);
    GG_COMPILER_UNUSED(metadata);

    // check that we have an input function to push packets into the stack
    if (!self->netif.input) {
        return GG_ERROR_INVALID_STATE;
    }

    // allocate a buffer to copy the data into
    struct pbuf* buffer = pbuf_alloc(PBUF_LINK, (u16_t)GG_Buffer_GetDataSize(data), PBUF_POOL);
    if (buffer == NULL) {
        GG_LOG_WARNING("pbuf_alloc returned NULL");
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // copy the data
    pbuf_take(buffer, GG_Buffer_GetData(data), (u16_t)GG_Buffer_GetDataSize(data));

    // send the packet up the stack
    err_t result = self->netif.input(buffer, &self->netif);
    if (result != ERR_OK) {
        GG_LOG_WARNING("netif.input returned %d", result);
        return GG_FAILURE;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipGenericNetworkInterface_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    // not used
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static void
GG_LwipGenericNetworkInterface_OnCanPut(GG_DataSinkListener* _self)
{
    // not used
    GG_COMPILER_UNUSED(_self);
}

//----------------------------------------------------------------------
static GG_Result
GG_LwipGenericNetworkInterface_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_LwipGenericNetworkInterface* self = GG_SELF(GG_LwipGenericNetworkInterface, GG_DataSource);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // de-register as a listener from the current sink
    if (self->transport_sink) {
        GG_DataSink_SetListener(self->transport_sink, NULL);
    }

    // keep a reference to the new sink
    self->transport_sink = sink;

    // register as a listener
    if (sink) {
        GG_DataSink_SetListener(sink, GG_CAST(self, GG_DataSinkListener));
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LwipGenericNetworkInterface, GG_DataSink) {
    GG_LwipGenericNetworkInterface_PutData,
    GG_LwipGenericNetworkInterface_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_LwipGenericNetworkInterface, GG_DataSinkListener) {
    GG_LwipGenericNetworkInterface_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_LwipGenericNetworkInterface, GG_DataSource) {
    GG_LwipGenericNetworkInterface_SetDataSink
};

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
static GG_Result
GG_LwipGenericNetworkInterface_Inspect(GG_Inspectable*             _self,
                                       GG_Inspector*               inspector,
                                       const GG_InspectionOptions* options)
{
    GG_LwipGenericNetworkInterface* self = GG_SELF(GG_LwipGenericNetworkInterface, GG_Inspectable);
    GG_COMPILER_UNUSED(options);

    char ip_address_string[32];
    ipaddr_ntoa_r(&self->netif.ip_addr, ip_address_string, sizeof(ip_address_string));
    GG_Inspector_OnString(inspector, "ip_address", ip_address_string);
    ipaddr_ntoa_r(&self->netif.netmask, ip_address_string, sizeof(ip_address_string));
    GG_Inspector_OnString(inspector, "netmask", ip_address_string);
    ipaddr_ntoa_r(&self->netif.gw, ip_address_string, sizeof(ip_address_string));
    GG_Inspector_OnString(inspector, "gateway", ip_address_string);
    GG_Inspector_OnInteger(inspector, "mtu", self->netif.mtu, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);
    GG_Inspector_OnInteger(inspector, "number", self->netif.num, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_LwipGenericNetworkInterface, GG_Inspectable) {
    .Inspect = GG_LwipGenericNetworkInterface_Inspect
};

//----------------------------------------------------------------------
GG_Inspectable*
GG_LwipGenericNetworkInterface_AsInspectable(GG_LwipGenericNetworkInterface* self)
{
    return GG_CAST(self, GG_Inspectable);
}
#endif

//----------------------------------------------------------------------
GG_Result
GG_LwipGenericNetworkInterface_Create(size_t                           mtu,
                                      GG_Loop*                         loop,
                                      GG_LwipGenericNetworkInterface** netif)
{
    // allocate a new interface object
    *netif = (GG_LwipGenericNetworkInterface*)GG_AllocateZeroMemory(sizeof(GG_LwipGenericNetworkInterface));
    if (!*netif) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // keep a reference to the loop.
    // NOTE: the current implementation doesn't (yet) support delegating the delivery
    // of packets to the loop thread, so until that is implemented, it can only
    // be used when LwIP operates in "direct" mode
    // (i.e "NO_SYS = 1" or "LWIP_TCPIP_CORE_LOCKING = 1")
    // where the network interace calls are made from the same thread as the one where
    // the socket functions are invoked.
    (*netif)->loop = loop;

    if (mtu) {
        (*netif)->netif.mtu = (uint16_t)mtu;
    } else {
        (*netif)->netif.mtu = GG_LWIP_GENERIC_NETIF_DEFAULT_MTU;
    }

    // setup interfaces
    GG_SET_INTERFACE(*netif, GG_LwipGenericNetworkInterface, GG_DataSink);
    GG_SET_INTERFACE(*netif, GG_LwipGenericNetworkInterface, GG_DataSinkListener);
    GG_SET_INTERFACE(*netif, GG_LwipGenericNetworkInterface, GG_DataSource);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(*netif, GG_LwipGenericNetworkInterface, GG_Inspectable));

    // bind to the current thread
    GG_THREAD_GUARD_BIND(*netif);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_LwipGenericNetworkInterface_Destroy(GG_LwipGenericNetworkInterface* self)
{
    if (self == NULL) return;

    // de-register as a listener from the current sink
    if (self->transport_sink) {
        GG_DataSink_SetListener(self->transport_sink, NULL);
    }

    GG_ClearAndFreeObject(self, 3);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_LwipGenericNetworkInterface_AsDataSink(GG_LwipGenericNetworkInterface* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_LwipGenericNetworkInterface_AsDataSource(GG_LwipGenericNetworkInterface* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_Result
GG_LwipGenericNetworkInterface_Register(GG_LwipGenericNetworkInterface* self,
                                        const GG_IpAddress*             source_address,
                                        const GG_IpAddress*             netmask,
                                        const GG_IpAddress*             gateway,
                                        bool                            is_default)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    ip4_addr_t my_addr;
    ip4_addr_t my_netmask;
    ip4_addr_t my_gateway;
    ip4_addr_set_u32(&my_addr,    lwip_ntohl(GG_IpAddress_AsInteger(source_address)));
    ip4_addr_set_u32(&my_netmask, lwip_ntohl(GG_IpAddress_AsInteger(netmask)));
    ip4_addr_set_u32(&my_gateway, lwip_ntohl(GG_IpAddress_AsInteger(gateway)));

    netif_add(&self->netif, &my_addr, &my_netmask, &my_gateway, self, LwipNetworkInterface_Init, ip_input);
    netif_set_link_up(&self->netif);
    netif_set_up(&self->netif);

    if (is_default) {
        netif_set_default(&self->netif);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_LwipGenericNetworkInterface_Deregister(GG_LwipGenericNetworkInterface* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);

    netif_remove(&self->netif);

    return GG_SUCCESS;
}
