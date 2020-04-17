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
 * @date 2017-09-18
 *
 * @details
 *
 * Networking sockets interfaces.
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_types.h"
#include "xp/common/gg_results.h"
#include "xp/common/gg_io.h"
#include "xp/loop/gg_loop.h"

#if defined(__cplusplus)
extern "C" {
#endif

//! @addtogroup Sockets Network Sockets
//! Network Sockets
//! @{

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
/**
 * IPv4 address.
 */
typedef struct {
    uint8_t ipv4[4];
} GG_IpAddress;

/**
 * Socket address consisting of an IP address and a port number.
 * (used for UDP and TCP sockets)
 */
typedef struct {
    GG_IpAddress address;
    uint16_t     port;
} GG_SocketAddress;

/**
 * Metadata used to provide information about the source or destination for
 * a datagram sent to or received from a socket.
 */
typedef struct {
    GG_BufferMetadata base;
    /**
     * Depending on the base type, this may be interpreted as a source or destination address
     */
    GG_SocketAddress  socket_address;
} GG_SocketAddressMetadata;

/**
 * Interface implemented by objects that can send and/or receive datagrams.
 *
 * For UDP sockets, when sending data through the socket by calling GG_DataSink_PutData on the socket's
 * GG_DataSink interface, passing metadata of type GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS
 * allows the caller to specify the destination IP address and UDP port number to send to.
 * The sink receiving data from the socket would receive metadata of type
 * GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS indicating what IP address and UDP port number the
 * datagram was received from.
 */
GG_DECLARE_INTERFACE(GG_DatagramSocket)
{
    /**
     * Obtain the GG_DataSink interface for the socket.
     *
     * @param self The object on which this method is invoked.
     *
     * @return The GG_DataSink interface for the object.
     */
    GG_DataSink* (*AsDataSink)(GG_DatagramSocket* self);

    /**
     * Obtain the GG_DataSource interface for the socket.
     *
     * @param self The object on which this method is invoked.
     *
     * @return The GG_DataSource interface for the object.
     */
    GG_DataSource* (*AsDataSource)(GG_DatagramSocket* self);

    /**
     * Destroy the socket.
     *
     * @param self The object on which this method is invoked.
     */
    void (*Destroy)(GG_DatagramSocket* self);

    /**
     * Attach the socket to a loop.
     * This allows the loop to monitor I/O on the socket and call I/O event
     * handlers as appropriate.
     *
     * @param self The object on which this method is invoked.
     * @param loop The loop to attach to.
     *
     * @return GG_SUCCESS if the call succeeded, or a negative error code.
     */
    GG_Result (*Attach)(GG_DatagramSocket* self, GG_Loop* loop);
};

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
/**
 * The 0.0.0.0 IP address.
 */
extern const GG_IpAddress GG_IpAddress_Any;

/**
 * Buffer metadata type that indicates a destination address.
 * Metadata structs with this type ID must be of type GG_SocketAddressMetadata
 */
#define GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS GG_4CC('d', 's', 't', 'a')

/**
 * Buffer metadata type that indicates a source address.
 * Metadata structs with this type ID must be of type GG_SocketAddressMetadata
 */
#define GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS GG_4CC('s', 'r', 'c', 'a')

/*----------------------------------------------------------------------
|   macros
+---------------------------------------------------------------------*/
#define GG_IP_ADDRESS_NULL_INITIALIZER {{0, 0, 0, 0}}

#define GG_SOCKET_ADDRESS_NULL_INITIALIZER {GG_IP_ADDRESS_NULL_INITIALIZER, 0}

#define GG_SOURCE_SOCKET_ADDRESS_METADATA_INITIALIZER(_address, _port) \
((GG_SocketAddressMetadata){                                           \
    .base = {                                                          \
        .type = GG_BUFFER_METADATA_TYPE_SOURCE_SOCKET_ADDRESS,         \
        .size = sizeof(GG_SocketAddressMetadata)                       \
    },                                                                 \
    .socket_address = {                                                \
        .address = _address,                                           \
        .port    = (_port)                                             \
    }                                                                  \
})

#define GG_DESTINATION_SOCKET_ADDRESS_METADATA_INITIALIZER(_address, _port) \
((GG_SocketAddressMetadata){                                                \
    .base = {                                                               \
        .type = GG_BUFFER_METADATA_TYPE_DESTINATION_SOCKET_ADDRESS,         \
        .size = sizeof(GG_SocketAddressMetadata)                            \
    },                                                                      \
    .socket_address = {                                                     \
        .address = _address,                                                \
        .port    = (_port)                                                  \
    }                                                                       \
})

/*----------------------------------------------------------------------
|   results
+---------------------------------------------------------------------*/
#define GG_ERROR_CONNECTION_RESET      (GG_ERROR_BASE_SOCKET -  0)
#define GG_ERROR_CONNECTION_ABORTED    (GG_ERROR_BASE_SOCKET -  1)
#define GG_ERROR_CONNECTION_REFUSED    (GG_ERROR_BASE_SOCKET -  2)
#define GG_ERROR_CONNECTION_FAILED     (GG_ERROR_BASE_SOCKET -  3)
#define GG_ERROR_HOST_UNKNOWN          (GG_ERROR_BASE_SOCKET -  4)
#define GG_ERROR_SOCKET_FAILED         (GG_ERROR_BASE_SOCKET -  5)
#define GG_ERROR_GETSOCKOPT_FAILED     (GG_ERROR_BASE_SOCKET -  6)
#define GG_ERROR_SETSOCKOPT_FAILED     (GG_ERROR_BASE_SOCKET -  7)
#define GG_ERROR_SOCKET_CONTROL_FAILED (GG_ERROR_BASE_SOCKET -  8)
#define GG_ERROR_BIND_FAILED           (GG_ERROR_BASE_SOCKET -  9)
#define GG_ERROR_LISTEN_FAILED         (GG_ERROR_BASE_SOCKET - 10)
#define GG_ERROR_ACCEPT_FAILED         (GG_ERROR_BASE_SOCKET - 11)
#define GG_ERROR_ADDRESS_IN_USE        (GG_ERROR_BASE_SOCKET - 12)
#define GG_ERROR_NETWORK_DOWN          (GG_ERROR_BASE_SOCKET - 13)
#define GG_ERROR_NETWORK_UNREACHABLE   (GG_ERROR_BASE_SOCKET - 14)
#define GG_ERROR_HOST_UNREACHABLE      (GG_ERROR_BASE_SOCKET - 15)
#define GG_ERROR_NOT_CONNECTED         (GG_ERROR_BASE_SOCKET - 16)

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/
uint32_t GG_IpAddress_AsInteger(const GG_IpAddress* address);
void GG_IpAddress_SetFromInteger(GG_IpAddress* address, uint32_t value);
void GG_IpAddress_AsString(const GG_IpAddress* address, char* buffer, size_t buffer_size);
GG_Result GG_IpAddress_SetFromString(GG_IpAddress* address, const char* string);
void GG_IpAddress_Copy(GG_IpAddress* dst_address, const GG_IpAddress* src_address);
bool GG_IpAddress_Equal(const GG_IpAddress* address1, const GG_IpAddress* address2);
bool GG_IpAddress_IsAny(const GG_IpAddress* address);
void GG_SocketAddress_AsString(const GG_SocketAddress* addrss, char* buffer, size_t buffer_size);

/**
 * Create a bound UDP socket.
 */
GG_Result GG_DatagramSocket_Create(const GG_SocketAddress* local_address,
                                   const GG_SocketAddress* remote_address,
                                   bool                    connect_to_remote,
                                   unsigned int            max_datagram_size,
                                   GG_DatagramSocket**     socket);

GG_DataSink* GG_DatagramSocket_AsDataSink(GG_DatagramSocket* self);
GG_DataSource* GG_DatagramSocket_AsDataSource(GG_DatagramSocket* self);

/**
 * Destroy a UDP socket.
 */
void GG_DatagramSocket_Destroy(GG_DatagramSocket* self);

GG_Result GG_DatagramSocket_Attach(GG_DatagramSocket* self, GG_Loop* loop);

//! @}

#if defined(__cplusplus)
}
#endif
