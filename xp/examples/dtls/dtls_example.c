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
 * Example usage of the TLS/DTLS API.
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_utils.h"
#include "xp/common/gg_timer.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/tls/gg_tls.h"
#include "xp/module/gg_module.h"

//----------------------------------------------------------------------
static uint8_t PSK[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
static uint8_t PSK_IDENTITY[5] = {
    'h', 'e', 'l', 'l', 'o'
};

#define MAX_DATAGRAM_SIZE 1024

//----------------------------------------------------------------------
// Object that prints the size of data it receives.
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_DataSink);
} Printer;

static GG_Result
Printer_PutData(GG_DataSink* self, GG_Buffer* data, const GG_BufferMetadata* metadata)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(metadata);

    printf("=== got data, size=%u\n", (int)GG_Buffer_GetDataSize(data));
    return GG_SUCCESS;
}

static GG_Result
Printer_SetListener(GG_DataSink* self, GG_DataSinkListener* listener)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(listener);
    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(Printer, GG_DataSink) {
    Printer_PutData,
    Printer_SetListener
};

//----------------------------------------------------------------------
// Object that can resolve a single key
//----------------------------------------------------------------------
typedef struct {
    GG_IMPLEMENTS(GG_TlsKeyResolver);

    const uint8_t* psk_identity;
    size_t         psk_identity_size;
    const uint8_t* psk;
    size_t         psk_size;
} StaticPskResolver;

static GG_Result
StaticPskResolver_ResolvePsk(GG_TlsKeyResolver* _self,
                             const uint8_t*     key_identity,
                             size_t             key_identify_size,
                             uint8_t*           key,
                             size_t*            key_size)
{
    StaticPskResolver* self = (StaticPskResolver*)GG_SELF(StaticPskResolver, GG_TlsKeyResolver);

    // check that the identity matches what we have
    if (key_identify_size != self->psk_identity_size ||
        memcmp(key_identity, self->psk_identity, key_identify_size)) {
        return GG_ERROR_NO_SUCH_ITEM;
    }

    // check that the key can fit
    if (*key_size < self->psk_size) {
        *key_size = self->psk_size;
        return GG_ERROR_NOT_ENOUGH_SPACE;
    }

    // copy the key
    memcpy(key, self->psk, self->psk_size);
    *key_size = self->psk_size;

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(StaticPskResolver, GG_TlsKeyResolver) {
    StaticPskResolver_ResolvePsk
};

static StaticPskResolver PskResolver;

//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    bool client_mode = false;

    // parse the command line arguments
    if (argc != 2) {
        fprintf(stderr, "ERROR: missing argument\n");
        return 1;
    }
    if (!strcmp(argv[1], "c")) {
        client_mode = true;
    } else if (!strcmp(argv[1], "s")) {
        client_mode = false;
    } else {
        fprintf(stderr, "ERROR: invalid argument\n");
        return 1;
    }

    // init Golden Gate
    GG_Module_Initialize();

    // create a loop
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_Loop_BindToCurrentThread(loop);
    GG_ASSERT(GG_SUCCEEDED(result));

    // create a client socket on port 5684
    GG_DatagramSocket* transport_socket = NULL;
    if (client_mode) {
        GG_SocketAddress local_address  = { GG_IP_ADDRESS_NULL_INITIALIZER, 5685 };
        GG_SocketAddress remote_address = {GG_IP_ADDRESS_NULL_INITIALIZER, 5684};
        GG_IpAddress_SetFromString(&remote_address.address, "127.0.0.1");
        GG_BsdDatagramSocket_Create(&local_address, &remote_address, false, MAX_DATAGRAM_SIZE, &transport_socket);
    } else {
        GG_SocketAddress local_address = {GG_IP_ADDRESS_NULL_INITIALIZER, 5684};
        GG_SocketAddress remote_address = {GG_IP_ADDRESS_NULL_INITIALIZER, 5685};
        GG_IpAddress_SetFromString(&remote_address.address, "127.0.0.1");
        GG_BsdDatagramSocket_Create(&local_address, &remote_address, false, MAX_DATAGRAM_SIZE, &transport_socket);
    }
    GG_DatagramSocket_Attach(transport_socket, loop);

    // create a DTLS protocol object
    GG_DtlsProtocol* dtls = NULL;
    if (client_mode) {
        GG_TlsClientOptions options = {
            .base = {
                .cipher_suites = NULL,
                .cipher_suites_count = 0
            },
            .psk_identity      = PSK_IDENTITY,
            .psk_identity_size = sizeof(PSK_IDENTITY),
            .psk               = PSK,
            .psk_size          = sizeof(PSK)
        };
        result = GG_DtlsProtocol_Create(GG_TLS_ROLE_CLIENT,
                                        &options.base,
                                        1024,
                                        GG_Loop_GetTimerScheduler(loop),
                                        &dtls);
    } else {
        GG_SET_INTERFACE(&PskResolver, StaticPskResolver, GG_TlsKeyResolver);
        PskResolver.psk_identity      = PSK_IDENTITY;
        PskResolver.psk_identity_size = sizeof(PSK_IDENTITY);
        PskResolver.psk               = PSK;
        PskResolver.psk_size          = sizeof(PSK);
        GG_TlsServerOptions options = {
            .base = {
                .cipher_suites = NULL,
                .cipher_suites_count = 0
            },
            .key_resolver = GG_CAST(&PskResolver, GG_TlsKeyResolver)
        };
        result = GG_DtlsProtocol_Create(GG_TLS_ROLE_SERVER,
                                        &options.base,
                                        1024,
                                        GG_Loop_GetTimerScheduler(loop),
                                        &dtls);
    }
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_DtlsProtocol_Create failed (%d)\n", (int)result);
        return 1;
    }

    // connect the transport to the DTLS protocol
    GG_DataSource_SetDataSink(GG_DatagramSocket_AsDataSource(transport_socket),
                              GG_DtlsProtocol_GetTransportSideAsDataSink(dtls));
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetTransportSideAsDataSource(dtls),
                              GG_DatagramSocket_AsDataSink(transport_socket));

    // init a printer sink and connect it to the user side of the dtls protocol
    Printer printer;
    GG_SET_INTERFACE(&printer, Printer, GG_DataSink);
    GG_DataSource_SetDataSink(GG_DtlsProtocol_GetUserSideAsDataSource(dtls),
                              GG_CAST(&printer, GG_DataSink));

    // start the handshake
    GG_DtlsProtocol_StartHandshake(dtls);

    // run the loop
    GG_Loop_Run(loop);

    // cleanup
    GG_DtlsProtocol_Destroy(dtls);
    GG_DatagramSocket_Destroy(transport_socket);
    GG_Loop_Destroy(loop);
}
