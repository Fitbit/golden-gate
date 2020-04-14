/**
 *
 * @file
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
 * Platform-independent sockets and network functions.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>
#include <stdio.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_types.h"
#include "xp/common/gg_utils.h"
#include "gg_sockets.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
const GG_IpAddress GG_IpAddress_Any = {
    {0,0,0,0}
};

/*----------------------------------------------------------------------
|   thunks
+---------------------------------------------------------------------*/
GG_DataSink*
GG_DatagramSocket_AsDataSink(GG_DatagramSocket* self)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->AsDataSink(self);
}

//----------------------------------------------------------------------
GG_DataSource*
GG_DatagramSocket_AsDataSource(GG_DatagramSocket* self)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->AsDataSource(self);
}

//----------------------------------------------------------------------
void
GG_DatagramSocket_Destroy(GG_DatagramSocket* self)
{
    if (self == NULL) return;

    GG_INTERFACE(self)->Destroy(self);
}

//----------------------------------------------------------------------
GG_Result
GG_DatagramSocket_Attach(GG_DatagramSocket* self, GG_Loop* loop)
{
    GG_ASSERT(self);
    return GG_INTERFACE(self)->Attach(self, loop);
}

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
uint32_t
GG_IpAddress_AsInteger(const GG_IpAddress* address)
{
    return GG_BytesToInt32Be(address->ipv4);
}

//----------------------------------------------------------------------
void
GG_IpAddress_SetFromInteger(GG_IpAddress* address, uint32_t value)
{
    GG_BytesFromInt32Be(address->ipv4, value);
}

//----------------------------------------------------------------------
void
GG_IpAddress_AsString(const GG_IpAddress* address, char* buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "%d.%d.%d.%d",
        (int)address->ipv4[0],
        (int)address->ipv4[1],
        (int)address->ipv4[2],
        (int)address->ipv4[3]);
}

//----------------------------------------------------------------------
GG_Result
GG_IpAddress_SetFromString(GG_IpAddress* address, const char* string)
{
    unsigned int fragment_count  = 0;
    unsigned int fragment_length = 0;
    unsigned int fragment_value  = 0;
    char c;

    GG_ASSERT(string);

    do {
        c = *string++;
        if (c == '.' || c == '\0') {
            // end of fragment

            // check that the fragment isn't past the end
            if (fragment_count >= 4) {
                return GG_ERROR_INVALID_SYNTAX;
            }

            // check tha the fragment isn't empty
            if (fragment_length == 0) {
                // empty fragment
                return GG_ERROR_INVALID_SYNTAX;
            }

            // store the fragment value and update counters
            address->ipv4[fragment_count++] = (uint8_t)fragment_value;
            fragment_value  = 0;
            fragment_length = 0;
        } else if (c >= '0' && c <= '9') {
            // update the fragment value
            fragment_value = (10 * fragment_value) + (c - '0');
            if (fragment_value > 255) {
                return GG_ERROR_INVALID_SYNTAX;
            }
            ++fragment_length;
        } else {
            return GG_ERROR_INVALID_SYNTAX;
        }
    } while (c);

    // check that we have parsed exactly 4 fragments
    if (fragment_count != 4) {
        return GG_ERROR_INVALID_SYNTAX;
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
void
GG_IpAddress_Copy(GG_IpAddress* dst_address, const GG_IpAddress* src_address)
{
    memcpy(dst_address, src_address, sizeof(GG_IpAddress));
}

//----------------------------------------------------------------------
bool
GG_IpAddress_Equal(const GG_IpAddress* address1, const GG_IpAddress* address2)
{
    return memcmp(address1->ipv4, address2->ipv4, sizeof(address1->ipv4)) == 0;
}

//----------------------------------------------------------------------
bool
GG_IpAddress_IsAny(const GG_IpAddress* address)
{
    return GG_IpAddress_Equal(address, &GG_IpAddress_Any);
}

//----------------------------------------------------------------------
void
GG_SocketAddress_AsString(const GG_SocketAddress* address, char* buffer, size_t buffer_size)
{
    snprintf(buffer, buffer_size, "%d.%d.%d.%d:%u",
        (int)address->address.ipv4[0],
        (int)address->address.ipv4[1],
        (int)address->address.ipv4[2],
        (int)address->address.ipv4[3],
        (int)address->port);
}

//----------------------------------------------------------------------
// Implements a generic functions for sockets that delegates to the
// default socket implementation for the platform
//----------------------------------------------------------------------
#if defined(GG_CONFIG_DEFAULT_SOCKETS_MODULE)
#define GG_CONCAT(x, f) GG_ ## x ## f

#define GG_SOCKETS_CONSTRUCTOR(x) GG_CONCAT(x, DatagramSocket_Create)
extern GG_Result GG_SOCKETS_CONSTRUCTOR(GG_CONFIG_DEFAULT_SOCKETS_MODULE)(
    const GG_SocketAddress*,
    const GG_SocketAddress*,
    bool,
    unsigned int,
    GG_DatagramSocket**);
GG_Result
GG_DatagramSocket_Create(const GG_SocketAddress* local_address,
                         const GG_SocketAddress* remote_address,
                         bool                    connect_to_remote,
                         unsigned int            max_datagram_size,
                         GG_DatagramSocket**     socket_object)
{
    return GG_SOCKETS_CONSTRUCTOR(GG_CONFIG_DEFAULT_SOCKETS_MODULE)(
        local_address,
        remote_address,
        connect_to_remote,
        max_datagram_size,
        socket_object);
}

#endif
