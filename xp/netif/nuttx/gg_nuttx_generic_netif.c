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
 * @date 2018-04-15
 *
 * @details
 *
 * Generic NuttX netif implementation.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>

#include <nuttx/net/net.h>
#include <nuttx/net/netdev.h>

#include "xp/common/gg_logging.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_threads.h"
#include "xp/sockets/gg_sockets.h"
#include "gg_nuttx_generic_netif.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_NUTTX_GENERIC_NETIF_DEFAULT_MTU 1280

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
struct GG_NuttxGenericNetworkInterface {
    GG_IMPLEMENTS(GG_DataSink);
    GG_IMPLEMENTS(GG_DataSinkListener);
    GG_IMPLEMENTS(GG_DataSource);
    GG_IF_INSPECTION_ENABLED(GG_IMPLEMENTS(GG_Inspectable);)

    // GG XP fields
    GG_DataSink* transport_sink;
    bool         registered;
    bool         up;
    size_t       mtu;
    uint8_t*     send_buffer;         // storage for outgoing packets
    size_t       send_buffer_pending; // number of bytes waiting to be sent

    // NuttX fields
    struct net_driver_s driver;
    size_t driver_buffer_size;  // size of the send/receive buffer used to
                                // receive data from or send data to the stack
                                // (at least MTU)

    GG_THREAD_GUARD_ENABLE_BINDING
};

/*----------------------------------------------------------------------
|   logging
+---------------------------------------------------------------------*/
GG_SET_LOCAL_LOGGER("gg.xp.nuttx.netif")

/*----------------------------------------------------------------------
|   forward declarations
+---------------------------------------------------------------------*/
static void GG_NuttxGenericNetworkInterface_OnCanPut(GG_DataSinkListener* _self);

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static GG_Result
GG_NuttxMapErrorCode(int result)
{
    switch (result) {
        case OK: return GG_SUCCESS;
        default: return GG_FAILURE;
    }
}

//----------------------------------------------------------------------
// This function is called when data arrives from the link transport
//----------------------------------------------------------------------
static GG_Result
GG_NuttxGenericNetworkInterface_PutData(GG_DataSink* _self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_NuttxGenericNetworkInterface* self = GG_SELF(GG_NuttxGenericNetworkInterface, GG_DataSink);
    GG_THREAD_GUARD_CHECK_BINDING(self);
    GG_COMPILER_UNUSED(data);
    GG_COMPILER_UNUSED(metadata);

    // lock the stack
    net_lock();

    // update stats
    NETDEV_RXIPV4(&self->driver);

    // prepare the packet to send up the stack
    size_t packet_size = GG_Buffer_GetDataSize(data);
    GG_LOG_FINER("received %u bytes", (int)packet_size);
    if (packet_size <= self->driver_buffer_size) {
        // copy data into the receive buffer and set the size
        memcpy(self->driver.d_buf, GG_Buffer_GetData(data), packet_size);
        self->driver.d_len = (uint16_t)packet_size;

        // send the packet up the stack
        GG_LOG_FINER("sending packet up the stack");
        ipv4_input(&self->driver);
    } else {
        GG_LOG_WARNING("packet larger that driver buffer, dropping");
    }

    // unlock the stack
    net_unlock();

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_NuttxGenericNetworkInterface_SetListener(GG_DataSink* _self, GG_DataSinkListener* listener)
{
    // not used
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
GG_NuttxGenericNetworkInterface_SetDataSink(GG_DataSource* _self, GG_DataSink* sink)
{
    GG_NuttxGenericNetworkInterface* self = GG_SELF(GG_NuttxGenericNetworkInterface, GG_DataSource);
    GG_THREAD_GUARD_CHECK_BINDING(self);

    // de-register as a listener from the current sink
    if (self->transport_sink) {
        GG_DataSink_SetListener(self->transport_sink, NULL);
    }

    // keep a reference to the new sink
    self->transport_sink = sink;

    // register as a listener
    GG_DataSink_SetListener(sink, GG_CAST(self, GG_DataSinkListener));

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_NuttxGenericNetworkInterface, GG_DataSink) {
    GG_NuttxGenericNetworkInterface_PutData,
    GG_NuttxGenericNetworkInterface_SetListener
};

GG_IMPLEMENT_INTERFACE(GG_NuttxGenericNetworkInterface, GG_DataSinkListener) {
    GG_NuttxGenericNetworkInterface_OnCanPut
};

GG_IMPLEMENT_INTERFACE(GG_NuttxGenericNetworkInterface, GG_DataSource) {
    GG_NuttxGenericNetworkInterface_SetDataSink
};

//----------------------------------------------------------------------
// This function does the actual sending out of the current outgoing packet
//----------------------------------------------------------------------
static GG_Result
GG_NuttxGenericNetworkInterface_TransmitPacket(GG_NuttxGenericNetworkInterface* self,
                                               const uint8_t*                   packet,
                                               size_t                           packet_size)
{
    GG_LOG_FINER("transmitting packet");

    // update stats
    NETDEV_TXPACKETS(&self->driver);

    // drop the packet and return now if we have no sink to send to
    if (self->transport_sink == NULL) {
        return GG_SUCCESS;
    }

    /*
     * WARNING
     *
     * Using a static buffer here is unsafe if this netif is used
     * in a generic way, since we don't have any guarantee that the
     * transport will make its own copy of the data.
     *
     * However this is ok for now since we are using it only with Gattlink
     * as the transport which does copy the data in its own buffer. We have
     * this compromise in order to avoid making another big allocation here
     * that may not be possible on some platforms.
     */
    GG_StaticBuffer buffer;
    GG_StaticBuffer_Init(&buffer, packet, packet_size);

    GG_Result result = GG_DataSink_PutData(self->transport_sink, GG_StaticBuffer_AsBuffer(&buffer), NULL);
    if (result == GG_ERROR_WOULD_BLOCK) {
        GG_LOG_FINEST("GG_DataSink_PutData would block");
    } else if (GG_FAILED(result)) {
        GG_LOG_WARNING("GG_DataSink_PutData failed (%d)", result);
    }

    return result;
}

//----------------------------------------------------------------------
static int
GG_NuttxGenericNetworkInterface_OnIfUp(FAR struct net_driver_s *dev)
{
    GG_NuttxGenericNetworkInterface* self = (GG_NuttxGenericNetworkInterface*)dev->d_private;
    GG_COMPILER_UNUSED(self);

    GG_LOG_FINER("interface up");
    self->up = true;

    return OK;
}

//----------------------------------------------------------------------
static int
GG_NuttxGenericNetworkInterface_OnIfDown(FAR struct net_driver_s *dev)
{
    GG_NuttxGenericNetworkInterface* self = (GG_NuttxGenericNetworkInterface*)dev->d_private;
    GG_COMPILER_UNUSED(self);

    GG_LOG_FINER("interface down");
    self->up = false;

    return OK;
}

//----------------------------------------------------------------------
static int
GG_NuttxGenericNetworkInterface_OnTxPoll(FAR struct net_driver_s *dev)
{
    GG_NuttxGenericNetworkInterface* self = (GG_NuttxGenericNetworkInterface*)dev->d_private;

    GG_LOG_FINEST("tx poll");

    // we must have checked that we're able to receive the data prior to
    // calling devif_poll with this method as callback
    GG_ASSERT(self->send_buffer_pending == 0);

    // check if there's data to be sent
    if (self->driver.d_sndlen > 0) {
        GG_LOG_FINER("%u bytes to send", (unsigned int)self->driver.d_sndlen);

        // try to send the packet
        GG_Result result = GG_NuttxGenericNetworkInterface_TransmitPacket(self,
                                                                          self->driver.d_buf,
                                                                          self->driver.d_len);

        // if the packet wasn't accepted because the sink was full, keep a copy of the
        // data so we can send it later
        if (result == GG_ERROR_WOULD_BLOCK) {
            GG_LOG_FINER("sink would block, saving packet for later");
            GG_ASSERT(self->driver.d_len <= self->mtu);
            memcpy(self->send_buffer, self->driver.d_buf, self->driver.d_len);
            self->send_buffer_pending = self->driver.d_len;
            return 1;
        }
    }

    // return 0 to indicate that the polling can continue
    return 0;
}

//----------------------------------------------------------------------
// This function is called by the sink when it may be ready to accept more data
//----------------------------------------------------------------------
static void
GG_NuttxGenericNetworkInterface_OnCanPut(GG_DataSinkListener* _self)
{
    GG_NuttxGenericNetworkInterface* self = GG_SELF(GG_NuttxGenericNetworkInterface, GG_DataSinkListener);

    // if we have some pending data, try to send it now
    if (self->send_buffer_pending) {
        GG_Result result = GG_NuttxGenericNetworkInterface_TransmitPacket(self,
                                                                          self->send_buffer,
                                                                          self->send_buffer_pending);
        if (GG_SUCCEEDED(result)) {
            GG_LOG_FINER("pending data sent");
            self->send_buffer_pending = 0;
        } else if (result != GG_ERROR_WOULD_BLOCK) {
            // something went wrong, just drop the packet
            self->send_buffer_pending = 0;
        }

        // if there's no more data pending, poll the stack for more
        net_lock();
        devif_poll(&self->driver, GG_NuttxGenericNetworkInterface_OnTxPoll);
        net_unlock();
    }
}

//----------------------------------------------------------------------
// This callback is called by the IP stack when data is available to send.
// Since this callback is invoked with the network stack already locked,
// we can call functions that can only be called under lock without having
// to lock it here.
//
// NOTE: this implementation assumes that this method is always called
// from the same thread that created and registered the network interface,
// which is normally the main Golden Gate loop thread.
// If there's ever a need to use this from other threads, it would be
// possible, by simply using GG_Loop_InvokeSync() to remote the call to
// the loop thread (which is a simple passthrough when called from  the
// loop thread itself), but it doesn't seem to be needed at this time.
//----------------------------------------------------------------------
static int
GG_NuttxGenericNetworkInterface_OnTxAvailable(FAR struct net_driver_s* dev)
{
    GG_NuttxGenericNetworkInterface* self = (GG_NuttxGenericNetworkInterface*)dev->d_private;

    GG_LOG_FINER("tx data available");

    // only do something if the interface is up
    if (!self->up) {
        return OK;
    }

    // if we still have a packet pending, do nothing now, we'll poll when it is sent
    if (self->send_buffer_pending) {
        return OK;
    }

    // poll the stack for any data ready to be sent
    devif_poll(&self->driver, GG_NuttxGenericNetworkInterface_OnTxPoll);

    return OK;
}

//----------------------------------------------------------------------
#ifdef CONFIG_NETDEV_IOCTL
static int
GG_NuttxGenericNetworkInterface_OnIoctl(FAR struct net_driver_s* dev,
                                        int                      cmd,
                                        unsigned long            arg)
{
    GG_NuttxGenericNetworkInterface* self = (GG_NuttxGenericNetworkInterface*)dev->d_private;
    GG_COMPILER_UNUSED(self);

    GG_LOG_FINE("received ioctl %u", arg);
    switch (cmd) {
        default:
            GG_LOG_WARNING("unsupported ioctl");
            return -ENOTTY;  /* Special return value for this case */
    }

    return OK;
}
#endif

#if defined(GG_CONFIG_ENABLE_INSPECTION)
//----------------------------------------------------------------------
static GG_Result
GG_NuttxGenericNetworkInterface_Inspect(GG_Inspectable*             _self,
                                        GG_Inspector*               inspector,
                                        const GG_InspectionOptions* options)
{
    GG_NuttxGenericNetworkInterface* self = GG_SELF(GG_NuttxGenericNetworkInterface, GG_Inspectable);
    char ip_address_string[32];
    unsigned char *ptr;

    GG_COMPILER_UNUSED(options);

    ptr = (unsigned char *)&self->driver.d_ipaddr;
    sprintf(ip_address_string, "%u.%u.%u.%u", ptr[0], ptr[1], ptr[2], ptr[3]);
    GG_Inspector_OnString(inspector, "ip_address", ip_address_string);

    ptr = (unsigned char *)&self->driver.d_netmask;
    sprintf(ip_address_string, "%u.%u.%u.%u", ptr[0], ptr[1], ptr[2], ptr[3]);
    GG_Inspector_OnString(inspector, "netmask", ip_address_string);

    ptr = (unsigned char *)&self->driver.d_draddr;
    sprintf(ip_address_string, "%u.%u.%u.%u", ptr[0], ptr[1], ptr[2], ptr[3]);
    GG_Inspector_OnString(inspector, "gateway", ip_address_string);

    GG_Inspector_OnInteger(inspector, "mtu", self->driver.d_mtu, GG_INSPECTOR_FORMAT_HINT_UNSIGNED);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(GG_NuttxGenericNetworkInterface, GG_Inspectable) {
    .Inspect = GG_NuttxGenericNetworkInterface_Inspect
};

//----------------------------------------------------------------------
GG_Inspectable*
GG_NuttxGenericNetworkInterface_AsInspectable(GG_NuttxGenericNetworkInterface* self)
{
    return GG_CAST(self, GG_Inspectable);
}
#endif

//----------------------------------------------------------------------
GG_Result
GG_NuttxGenericNetworkInterface_Create(size_t mtu, GG_NuttxGenericNetworkInterface** netif)
{
    GG_THREAD_GUARD_CHECK_MAIN_LOOP();

    // use defaults if needed
    if (mtu == 0) {
        mtu = GG_NUTTX_GENERIC_NETIF_DEFAULT_MTU;
    }

    // check bounds
    if (mtu < MIN_NET_DEV_MTU || mtu > MAX_NET_DEV_MTU) {
        return GG_ERROR_INVALID_PARAMETERS;
    }

    // reserve some space after the object for a driver buffer as well as a send buffer.
    size_t driver_buffer_size = mtu + CONFIG_NET_GUARDSIZE;
    size_t alloc_size = sizeof(GG_NuttxGenericNetworkInterface) + driver_buffer_size + mtu;

    // allocate a new interface object
    GG_NuttxGenericNetworkInterface* self =
        (GG_NuttxGenericNetworkInterface*)GG_AllocateZeroMemory(alloc_size);

    *netif = self;
    if (!self) {
        return GG_ERROR_OUT_OF_MEMORY;
    }

    // setup the device callbacks
    self->driver.d_buf     = (uint8_t *)(self + 1);                         // packet buffer
    self->driver.d_ifup    = GG_NuttxGenericNetworkInterface_OnIfUp;        // I/F up callback
    self->driver.d_ifdown  = GG_NuttxGenericNetworkInterface_OnIfDown;      // I/F down callback
    self->driver.d_txavail = GG_NuttxGenericNetworkInterface_OnTxAvailable; // New TX data callback
#ifdef CONFIG_NETDEV_IOCTL
    self->driver.d_ioctl   = GG_NuttxGenericNetworkInterface_OnIoctl;       // IOCTL command callback
#endif
    self->driver.d_private = self;                                          // Object instance pointer
    // TODO: configure NuttX with CONFIG_NET_USER_DEVFMT and set driver.d_ifname to "gg0"

    // setup other fields
    self->driver_buffer_size = driver_buffer_size;
    self->send_buffer = self->driver.d_buf + driver_buffer_size;
    self->mtu = mtu;

    // setup interfaces
    GG_SET_INTERFACE(self, GG_NuttxGenericNetworkInterface, GG_DataSink);
    GG_SET_INTERFACE(self, GG_NuttxGenericNetworkInterface, GG_DataSinkListener);
    GG_SET_INTERFACE(self, GG_NuttxGenericNetworkInterface, GG_DataSource);
    GG_IF_INSPECTION_ENABLED(GG_SET_INTERFACE(*netif, GG_NuttxGenericNetworkInterface, GG_Inspectable));

    // bind to the current thread
    GG_THREAD_GUARD_BIND(self);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_NuttxGenericNetworkInterface_Destroy(GG_NuttxGenericNetworkInterface* self)
{
    if (self == NULL) {
        return;
    }

    // ensure that we're unregistered
    if (self->registered) {
        GG_NuttxGenericNetworkInterface_Deregister(self);
    }

    // de-register as a listener from the current sink
    if (self->transport_sink) {
        GG_DataSink_SetListener(self->transport_sink, NULL);
    }

    GG_FreeMemory(self);
}

//----------------------------------------------------------------------
GG_DataSink*
GG_NuttxGenericNetworkInterface_AsDataSink(GG_NuttxGenericNetworkInterface* self)
{
    return GG_CAST(self, GG_DataSink);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_NuttxGenericNetworkInterface_AsDataSource(GG_NuttxGenericNetworkInterface* self)
{
    return GG_CAST(self, GG_DataSource);
}

//----------------------------------------------------------------------
GG_Result
GG_NuttxGenericNetworkInterface_Register(GG_NuttxGenericNetworkInterface* self,
                                         const GG_IpAddress*              source_address,
                                         const GG_IpAddress*              netmask,
                                         const GG_IpAddress*              gateway,
                                         bool                             is_default)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);
    GG_COMPILER_UNUSED(is_default);

    GG_LOG_FINE("registering NuttX network interface %s", self->driver.d_ifname);

    // setup the interface addresses
    self->driver.d_ipaddr  = ntohl(GG_IpAddress_AsInteger(source_address));
    self->driver.d_draddr  = ntohl(GG_IpAddress_AsInteger(gateway));
    self->driver.d_netmask = ntohl(GG_IpAddress_AsInteger(netmask));

    // register the interface with the IP stack.
    // (we register as a TUN interface because that's currently the closest we have
    // to a 'generic' interface)
    int n_result = netdev_register(&self->driver, NET_LL_TUN);
    if (n_result != OK) {
        GG_LOG_WARNING("netdev_register failed (%d)", n_result);
        return GG_NuttxMapErrorCode(n_result);
    }

    // mark the interface as being UP and IPv4
    IFF_SET_UP(self->driver.d_flags);
    IFF_SET_IPv4(self->driver.d_flags);
    self->up = true;

    // remember that we're registered
    self->registered = true;

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_Result
GG_NuttxGenericNetworkInterface_Deregister(GG_NuttxGenericNetworkInterface* self)
{
    GG_THREAD_GUARD_CHECK_BINDING(self);
    GG_COMPILER_UNUSED(self);

    // check that we're registered
    if (!self->registered) {
        return GG_ERROR_INVALID_STATE;
    }

    // unregister the interface
    int n_result = netdev_unregister(&self->driver);
    if (n_result != OK) {
        return GG_NuttxMapErrorCode(n_result);
    }

    // remember that we've unregistered
    self->registered = false;

    return GG_SUCCESS;
}
