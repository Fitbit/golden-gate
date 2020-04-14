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
 * TLS/DTLS protocol
 */

#pragma once

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>

#include "xp/common/gg_common.h"

//! @addtogroup TLS TLS
//! TLS/DTLS protocol
//! @{

#if defined(__cplusplus)
extern "C" {
#endif

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

//---------------------------------------------------------------------
//! @class GG_DtlsProtocol
//
//! DTLS processor that performans the DTLS handshake and encrypts/decrypts
//! buffers once the handshake has succeeded.
//! The object exposes a "user side" data source and data sink
//! to communicate "user data" (typically an application or a library like CoAP),
//! and a "transport side" data source and data sink
//! to connect to a "transport" (typically a GG_DatagramSocket) that will transmit
//! and receive the DTLS records.
//! The object uses a GG_TimerScheduler to create retransmission timers when needed.
//!
//! ```
//!                  +         ^
//!         User     |         |
//!         Side     |         |
//!              +---v----+----+----+
//!              |  sink  | source  |
//!              +--------+---------+
//!              |                  |    +-----------------+
//!              |    DTLS state    |<-->| Timer Scheduler |
//!              |                  |    +-----------------+
//!              +--------+---------+
//!              | source |  sink   |
//!              +---+----+----^----+
//!    Transport     |         |
//!         Side     |         |
//!                  v         +
//! ```
//---------------------------------------------------------------------
typedef struct GG_DtlsProtocol GG_DtlsProtocol;

/**
 * TLS client or server role.
 */
typedef enum {
    GG_TLS_ROLE_CLIENT,  ///< TLS client
    GG_TLS_ROLE_SERVER   ///< TLS server
} GG_TlsProtocolRole;

/**
 * State of the protocol.
 */
typedef enum {
    GG_TLS_STATE_INIT,      ///< Initial state after creation
    GG_TLS_STATE_HANDSHAKE, ///< During the handshake phase
    GG_TLS_STATE_SESSION,   ///< After the handshake has completed
    GG_TLS_STATE_ERROR      ///< After an error has occurred
} GG_TlsProtocolState;

//----------------------------------------------------------------------
//! Interface implemented by objects that can resolve keys given a key identity.
//!
//! @interface GG_TlsKeyResolver
//! @ingroup Interfaces
//----------------------------------------------------------------------
GG_DECLARE_INTERFACE(GG_TlsKeyResolver) {
    /**
     * Resolve a key (for example a pre-shared key) given a key identity.
     *
     * @param self The object on which this method is invoked.
     * @param key_identity Identity of the key to resolve.
     * @param key_identity_size Size of the key identity.
     * @param key Buffer in which the resolved key will be returned.
     * @param [in,out] key_size On input, the size of the key buffer. On
     * output, the size of the key returned, or the size needed for the key
     * buffer if the call returns GG_ERROR_NOT_ENOUGH_SPACE.
     *
     * @return GG_SUCCESS if the key was found, GG_ERROR_NO_SUCH_ITEM if the
     * key was not found, GG_ERROR_NOT_ENOUGH_SPACE if the key was too large
     * for the supplied buffer, or a negative error code.
     */
    GG_Result (*ResolveKey)(GG_TlsKeyResolver* self,
                            const uint8_t*     key_identity,
                            size_t             key_identify_size,
                            uint8_t*           key,
                            size_t*            key_size);
};

//! @var GG_TlsKeyResolver::iface
//! Pointer to the virtual function table for the interface

//! @struct GG_TlsKeyResolverInterface
//! Virtual function table for the GG_TlsKeyResolver interface

//! @relates GG_TlsKeyResolver
//! @copydoc GG_TlsKeyResolverInterface::ResolveKey
GG_Result GG_TlsKeyResolver_ResolveKey(GG_TlsKeyResolver* self,
                                       const uint8_t*     key_identity,
                                       size_t             key_identity_size,
                                       uint8_t*           key,
                                       size_t*            key_size);

/**
 * Common options shared by GG_TlsClientOptions and GG_TlsServerOptions
 */
typedef struct {
    const uint16_t* cipher_suites;       ///< Pointer to an array of cipher suite identifiers
    size_t          cipher_suites_count; ///< Number of identifiers in the array
} GG_TlsOptions;

/**
 * Options passed when creating a TLS/DTLS client
 */
typedef struct {
    GG_TlsOptions  base;              ///< Common options
    const uint8_t* psk_identity;      ///< PSK identity
    size_t         psk_identity_size; ///< Size of the PSK identity
    const uint8_t* psk;               ///< PSK
    size_t         psk_size;          ///< Size of the PSK
    const uint8_t* ticket;            ///< Session ticket, or NULL if no ticket is available
    size_t         ticket_size;       ///< Size of the ticket
} GG_TlsClientOptions;

/**
 * Options passed when creating a TLS/DTLS server
 */
typedef struct {
    GG_TlsOptions      base;         ///< Common options
    GG_TlsKeyResolver* key_resolver; ///< Key resolver use to resolve a key identity to a key value
} GG_TlsServerOptions;

/**
 * Status of a DTLS protocol object.
 */
typedef struct {
    GG_TlsProtocolState state;             ///< Current state of the protocol
    GG_Result           last_error;        ///< Last error, if any (GG_SUCCESS when no error has occurred)
    const uint8_t*      psk_identity;      ///< PSK identity (only valid after a successful handshake)
    size_t              psk_identity_size; ///< Size of the PSK identity
} GG_DtlsProtocolStatus;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/

/**
 * TLS API Error Codes
 */
#define GG_ERROR_TLS_FATAL_ALERT_MESSAGE           (GG_ERROR_BASE_TLS - 0)
#define GG_ERROR_TLS_UNKNOWN_IDENTITY              (GG_ERROR_BASE_TLS - 1)
#define GG_ERROR_TLS_BAD_CLIENT_HELLO              (GG_ERROR_BASE_TLS - 2)
#define GG_ERROR_TLS_BAD_SERVER_HELLO              (GG_ERROR_BASE_TLS - 3)

/**
 * Event type emitted by a DTLS protocol object when its state changes.
 *
 * The event struct is just a plain GG_Event.
 * The event source is the GG_DtlsProtocol object that emits the event.
 */
#define GG_EVENT_TYPE_TLS_STATE_CHANGE             GG_4CC('t', 'l', 's', 's')

#define GG_DTLS_MIN_DATAGRAM_SIZE                   512
#define GG_DTLS_MAX_DATAGRAM_SIZE                   2048

#define GG_DTLS_MAX_PSK_SIZE                        16

#define GG_TLS_RSA_WITH_NULL_MD5                    0x01
#define GG_TLS_RSA_WITH_NULL_SHA                    0x02

#define GG_TLS_RSA_WITH_RC4_128_MD5                 0x04
#define GG_TLS_RSA_WITH_RC4_128_SHA                 0x05
#define GG_TLS_RSA_WITH_DES_CBC_SHA                 0x09

#define GG_TLS_RSA_WITH_3DES_EDE_CBC_SHA            0x0A

#define GG_TLS_DHE_RSA_WITH_DES_CBC_SHA             0x15
#define GG_TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA        0x16

#define GG_TLS_PSK_WITH_NULL_SHA                    0x2C
#define GG_TLS_DHE_PSK_WITH_NULL_SHA                0x2D
#define GG_TLS_RSA_PSK_WITH_NULL_SHA                0x2E
#define GG_TLS_RSA_WITH_AES_128_CBC_SHA             0x2F

#define GG_TLS_DHE_RSA_WITH_AES_128_CBC_SHA         0x33
#define GG_TLS_RSA_WITH_AES_256_CBC_SHA             0x35
#define GG_TLS_DHE_RSA_WITH_AES_256_CBC_SHA         0x39

#define GG_TLS_RSA_WITH_NULL_SHA256                 0x3B
#define GG_TLS_RSA_WITH_AES_128_CBC_SHA256          0x3C
#define GG_TLS_RSA_WITH_AES_256_CBC_SHA256          0x3D

#define GG_TLS_RSA_WITH_CAMELLIA_128_CBC_SHA        0x41
#define GG_TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA    0x45

#define GG_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256      0x67
#define GG_TLS_DHE_RSA_WITH_AES_256_CBC_SHA256      0x6B

#define GG_TLS_RSA_WITH_CAMELLIA_256_CBC_SHA        0x84
#define GG_TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA    0x88

#define GG_TLS_PSK_WITH_RC4_128_SHA                 0x8A
#define GG_TLS_PSK_WITH_3DES_EDE_CBC_SHA            0x8B
#define GG_TLS_PSK_WITH_AES_128_CBC_SHA             0x8C
#define GG_TLS_PSK_WITH_AES_256_CBC_SHA             0x8D

#define GG_TLS_DHE_PSK_WITH_RC4_128_SHA             0x8E
#define GG_TLS_DHE_PSK_WITH_3DES_EDE_CBC_SHA        0x8F
#define GG_TLS_DHE_PSK_WITH_AES_128_CBC_SHA         0x90
#define GG_TLS_DHE_PSK_WITH_AES_256_CBC_SHA         0x91

#define GG_TLS_RSA_PSK_WITH_RC4_128_SHA             0x92
#define GG_TLS_RSA_PSK_WITH_3DES_EDE_CBC_SHA        0x93
#define GG_TLS_RSA_PSK_WITH_AES_128_CBC_SHA         0x94
#define GG_TLS_RSA_PSK_WITH_AES_256_CBC_SHA         0x95

#define GG_TLS_RSA_WITH_AES_128_GCM_SHA256          0x9C
#define GG_TLS_RSA_WITH_AES_256_GCM_SHA384          0x9D
#define GG_TLS_DHE_RSA_WITH_AES_128_GCM_SHA256      0x9E
#define GG_TLS_DHE_RSA_WITH_AES_256_GCM_SHA384      0x9F

#define GG_TLS_PSK_WITH_AES_128_GCM_SHA256          0xA8
#define GG_TLS_PSK_WITH_AES_256_GCM_SHA384          0xA9
#define GG_TLS_DHE_PSK_WITH_AES_128_GCM_SHA256      0xAA
#define GG_TLS_DHE_PSK_WITH_AES_256_GCM_SHA384      0xAB
#define GG_TLS_RSA_PSK_WITH_AES_128_GCM_SHA256      0xAC
#define GG_TLS_RSA_PSK_WITH_AES_256_GCM_SHA384      0xAD

#define GG_TLS_PSK_WITH_AES_128_CBC_SHA256          0xAE
#define GG_TLS_PSK_WITH_AES_256_CBC_SHA384          0xAF
#define GG_TLS_PSK_WITH_NULL_SHA256                 0xB0
#define GG_TLS_PSK_WITH_NULL_SHA384                 0xB1

#define GG_TLS_DHE_PSK_WITH_AES_128_CBC_SHA256      0xB2
#define GG_TLS_DHE_PSK_WITH_AES_256_CBC_SHA384      0xB3
#define GG_TLS_DHE_PSK_WITH_NULL_SHA256             0xB4
#define GG_TLS_DHE_PSK_WITH_NULL_SHA384             0xB5

#define GG_TLS_RSA_PSK_WITH_AES_128_CBC_SHA256      0xB6
#define GG_TLS_RSA_PSK_WITH_AES_256_CBC_SHA384      0xB7
#define GG_TLS_RSA_PSK_WITH_NULL_SHA256             0xB8
#define GG_TLS_RSA_PSK_WITH_NULL_SHA384             0xB9

#define GG_TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256     0xBA
#define GG_TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 0xBE

#define GG_TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256     0xC0
#define GG_TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 0xC4

#define GG_TLS_ECDH_ECDSA_WITH_NULL_SHA             0xC001
#define GG_TLS_ECDH_ECDSA_WITH_RC4_128_SHA          0xC002
#define GG_TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA     0xC003
#define GG_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA      0xC004
#define GG_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA      0xC005

#define GG_TLS_ECDHE_ECDSA_WITH_NULL_SHA            0xC006
#define GG_TLS_ECDHE_ECDSA_WITH_RC4_128_SHA         0xC007
#define GG_TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA    0xC008
#define GG_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA     0xC009
#define GG_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA     0xC00A

#define GG_TLS_ECDH_RSA_WITH_NULL_SHA               0xC00B
#define GG_TLS_ECDH_RSA_WITH_RC4_128_SHA            0xC00C
#define GG_TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA       0xC00D
#define GG_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA        0xC00E
#define GG_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA        0xC00F

#define GG_TLS_ECDHE_RSA_WITH_NULL_SHA              0xC010
#define GG_TLS_ECDHE_RSA_WITH_RC4_128_SHA           0xC011
#define GG_TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA      0xC012
#define GG_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA       0xC013
#define GG_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA       0xC014

#define GG_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256  0xC023
#define GG_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384  0xC024
#define GG_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256   0xC025
#define GG_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384   0xC026
#define GG_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256    0xC027
#define GG_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384    0xC028
#define GG_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256     0xC029
#define GG_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384     0xC02A

#define GG_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256  0xC02B
#define GG_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384  0xC02C
#define GG_TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256   0xC02D
#define GG_TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384   0xC02E
#define GG_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256    0xC02F
#define GG_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384    0xC030
#define GG_TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256     0xC031
#define GG_TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384     0xC032

#define GG_TLS_ECDHE_PSK_WITH_RC4_128_SHA           0xC033
#define GG_TLS_ECDHE_PSK_WITH_3DES_EDE_CBC_SHA      0xC034
#define GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA       0xC035
#define GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA       0xC036
#define GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256    0xC037
#define GG_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384    0xC038
#define GG_TLS_ECDHE_PSK_WITH_NULL_SHA              0xC039
#define GG_TLS_ECDHE_PSK_WITH_NULL_SHA256           0xC03A
#define GG_TLS_ECDHE_PSK_WITH_NULL_SHA384           0xC03B

#define GG_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 0xC072
#define GG_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 0xC073
#define GG_TLS_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256  0xC074
#define GG_TLS_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384  0xC075
#define GG_TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256   0xC076
#define GG_TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384   0xC077
#define GG_TLS_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256    0xC078
#define GG_TLS_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384    0xC079

#define GG_TLS_RSA_WITH_CAMELLIA_128_GCM_SHA256         0xC07A
#define GG_TLS_RSA_WITH_CAMELLIA_256_GCM_SHA384         0xC07B
#define GG_TLS_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256     0xC07C
#define GG_TLS_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384     0xC07D
#define GG_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256 0xC086
#define GG_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384 0xC087
#define GG_TLS_ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256  0xC088
#define GG_TLS_ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384  0xC089
#define GG_TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256   0xC08A
#define GG_TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384   0xC08B
#define GG_TLS_ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256    0xC08C
#define GG_TLS_ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384    0xC08D

#define GG_TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256       0xC08E
#define GG_TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384       0xC08F
#define GG_TLS_DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256   0xC090
#define GG_TLS_DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384   0xC091
#define GG_TLS_RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256   0xC092
#define GG_TLS_RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384   0xC093

#define GG_TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256       0xC094
#define GG_TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384       0xC095
#define GG_TLS_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256   0xC096
#define GG_TLS_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384   0xC097
#define GG_TLS_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256   0xC098
#define GG_TLS_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384   0xC099
#define GG_TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 0xC09A
#define GG_TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 0xC09B

#define GG_TLS_RSA_WITH_AES_128_CCM                0xC09C
#define GG_TLS_RSA_WITH_AES_256_CCM                0xC09D
#define GG_TLS_DHE_RSA_WITH_AES_128_CCM            0xC09E
#define GG_TLS_DHE_RSA_WITH_AES_256_CCM            0xC09F
#define GG_TLS_RSA_WITH_AES_128_CCM_8              0xC0A0
#define GG_TLS_RSA_WITH_AES_256_CCM_8              0xC0A1
#define GG_TLS_DHE_RSA_WITH_AES_128_CCM_8          0xC0A2
#define GG_TLS_DHE_RSA_WITH_AES_256_CCM_8          0xC0A3
#define GG_TLS_PSK_WITH_AES_128_CCM                0xC0A4
#define GG_TLS_PSK_WITH_AES_256_CCM                0xC0A5
#define GG_TLS_DHE_PSK_WITH_AES_128_CCM            0xC0A6
#define GG_TLS_DHE_PSK_WITH_AES_256_CCM            0xC0A7
#define GG_TLS_PSK_WITH_AES_128_CCM_8              0xC0A8
#define GG_TLS_PSK_WITH_AES_256_CCM_8              0xC0A9
#define GG_TLS_DHE_PSK_WITH_AES_128_CCM_8          0xC0AA
#define GG_TLS_DHE_PSK_WITH_AES_256_CCM_8          0xC0AB

#define GG_TLS_ECDHE_ECDSA_WITH_AES_128_CCM        0xC0AC
#define GG_TLS_ECDHE_ECDSA_WITH_AES_256_CCM        0xC0AD
#define GG_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8      0xC0AE
#define GG_TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8      0xC0AF

#define GG_TLS_ECJPAKE_WITH_AES_128_CCM_8          0xC0FF

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Create a new DTLS protocol object
 *
 * @param role Specifies if the object is a client or server.
 * @param options Options used to configure the object. When the role is GG_TLS_ROLE_CLIENT,
 * this must be a pointer to a GG_TlsClientOptions structure. When the role is GG_TLS_ROLE_SERVER,
 * this must be a pointer to a GG_TlsServerOptions structure.
 * @param max_datagram_size Maximum size of the datagrams that may be sent and received.
 * @param timer_scheduler A timer scheduler used for scheduling retransmission timers.
 * @param [out] protocol Pointer to where the object will be returned.
 *
 * @return GG_SUCCESS if the object could be created, or a negative error code.
 */
GG_Result GG_DtlsProtocol_Create(GG_TlsProtocolRole   role,
                                 const GG_TlsOptions* options,
                                 size_t               max_datagram_size,
                                 GG_TimerScheduler*   timer_scheduler,
                                 GG_DtlsProtocol**    protocol);

/**
 * Destroy a DTLS protocol objet.
 *
 * @param self The object on which this method is invoked.
 */
void GG_DtlsProtocol_Destroy(GG_DtlsProtocol* self);

/**
 * Get the current status of a DTLS protocol object.
 *
 * @param self The object on which this method is invoked.
 * @param [out] status Pointer to where the status will be returned.
 */
void GG_DtlsProtocol_GetStatus(GG_DtlsProtocol* self, GG_DtlsProtocolStatus* status);

/**
 * Get the event emitter interface of a DTLS protocol object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_EventEmitter interface for the object.
 */
GG_EventEmitter* GG_DtlsProtocol_AsEventEmitter(GG_DtlsProtocol* self);

/**
 * Get the inspectable interface of a DTLS protocol object.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The GG_Inspectable interface for the object.
 */
GG_Inspectable* GG_DtlsProtocol_AsInspectable(GG_DtlsProtocol* self);

/**
 * Start the DTLS handshake.
 * For clients, this will start emitting datagrams to the transport.
 * For servers, this will place the object in a mode where it is waiting for datagrams
 * from the transport.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the handshake could be started, or a negative error code.
 */
GG_Result GG_DtlsProtocol_StartHandshake(GG_DtlsProtocol* self);

/**
 * Reset the DTLS session.
 * NOTE: this will not automatically re-start the handshake.
 * It is up to the caller to subsequently call GG_DtlsProtocol_StartHandshake to
 * start a new handshake.
 *
 * @param self The object on which this method is invoked.
 *
 * @return GG_SUCCESS if the handshake could be reset, or a negative error code.
 */
GG_Result GG_DtlsProtocol_Reset(GG_DtlsProtocol* self);

/**
 * Return the GG_DataSink interface for the user side of the objet.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's GG_DataSink interface.
 */
GG_DataSink* GG_DtlsProtocol_GetUserSideAsDataSink(GG_DtlsProtocol* self);

/**
 * Return the GG_DataSource interface for the user side of the objet.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's GG_DataSource interface.
 */
GG_DataSource* GG_DtlsProtocol_GetUserSideAsDataSource(GG_DtlsProtocol* self);

/**
 * Return the GG_DataSink interface for the transport side of the objet.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's GG_DataSink interface.
 */
GG_DataSink* GG_DtlsProtocol_GetTransportSideAsDataSink(GG_DtlsProtocol* self);

/**
 * Return the GG_DataSource interface for the transport side of the objet.
 *
 * @param self The object on which this method is invoked.
 *
 * @return The object's GG_DataSource interface.
 */
GG_DataSource* GG_DtlsProtocol_GetTransportSideAsDataSource(GG_DtlsProtocol* self);

//! @}

#if defined(__cplusplus)
}
#endif
