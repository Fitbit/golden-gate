/**
 *
 * @file
 *
 * @copyright
 * Copyright 2017-2018 by Fitbit, Inc., all rights reserved.
 *
 * @author Gilles Boccon-Gibod
 *
 * @date 2018-04-5
 *
 * @details
 *
 * Service Host Example application
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include "xp/common/gg_port.h"
#include "xp/common/gg_queues.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_io.h"
#include "xp/common/gg_utils.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/sockets/ports/bsd/gg_bsd_sockets.h"
#include "xp/loop/gg_loop.h"
#include "xp/remote/gg_remote.h"
#include "xp/services/blast/gg_blast_service.h"
#include "xp/services/coap_client/gg_coap_client_service.h"
#include "xp/stack_builder/gg_stack_builder.h"

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_RemoteTransport);

    FILE* send_fifo;
    FILE* receive_fifo;
} FifoTransport;

typedef struct {
    GG_IMPLEMENTS(GG_RemoteSmoHandler);

    GG_StaticBuffer canned_response;
} HelloWorldHandler;

typedef struct {
    GG_Loop*         loop;
    FifoTransport*   fifo_transport;
    GG_DataSource*   stack_source;
    GG_DataSink*     stack_sink;
    GG_CoapEndpoint* coap_endpoint;
} ShellThreadArgs;

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define GG_SERVICE_HOST_EXAMPLE_MAX_TRANSORT_LINE_LENGTH 65536

#define GG_SERVICE_HOST_EXAMPLE_TRANSPORT_UDP_PORT_1     9000
#define GG_SERVICE_HOST_EXAMPLE_TRANSPORT_UDP_PORT_2     9001
#define GG_SERVICE_HOST_EXAMPLE_TRANSPORT_IP_ADDR        "127.0.0.1"
#define GG_SERVICE_HOST_EXAMPLE_MAX_DATAGRAM_SIZE        1152

//----------------------------------------------------------------------
static GG_Result
FifoTransport_Send(GG_RemoteTransport* _self, GG_Buffer* data)
{
    FifoTransport* self = GG_SELF(FifoTransport, GG_RemoteTransport);

    // encode to base64
    char encoded[(((GG_SERVICE_HOST_EXAMPLE_MAX_TRANSORT_LINE_LENGTH / 3) + 1) * 4) + 2];
    size_t encoded_size = sizeof(encoded) - 2;
    if (GG_Buffer_GetDataSize(data) > GG_SERVICE_HOST_EXAMPLE_MAX_TRANSORT_LINE_LENGTH) {
        return GG_ERROR_INTERNAL;
    }
    GG_Result result = GG_Base64_Encode(GG_Buffer_GetData(data),
                                        GG_Buffer_GetDataSize(data),
                                        encoded,
                                        &encoded_size,
                                        false);
    if (GG_FAILED(result)) {
        return result;
    }
    if (encoded_size > sizeof(encoded) - 2) {
        return GG_ERROR_INTERNAL;
    }
    encoded[encoded_size]     = '\n';
    encoded[encoded_size + 1] = '\0'; // terminate with a newline and NULL

    // write to the fifo
    size_t written = fwrite(encoded, encoded_size + 1, 1, self->send_fifo);
    if (written != 1) {
        fprintf(stderr, "ERROR: failed to write to the send fifo\n");
        return GG_FAILURE;
    }
    fflush(self->send_fifo);

    printf(">>> sent %d bytes: %s", (int)encoded_size, encoded);
    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result
FifoTransport_Receive(GG_RemoteTransport* _self, GG_Buffer** data)
{
    FifoTransport* self = GG_SELF(FifoTransport, GG_RemoteTransport);

    // default
    *data = NULL;

    // read a line from the fifo (we make the buffer static here so as to not burden the stack,
    // since we only have one thread using this, we're ok)
    static char line[GG_SERVICE_HOST_EXAMPLE_MAX_TRANSORT_LINE_LENGTH];
    size_t bytes_read = 0;

    while (bytes_read < GG_SERVICE_HOST_EXAMPLE_MAX_TRANSORT_LINE_LENGTH) {
        size_t result = fread(&line[bytes_read], 1, 1, self->receive_fifo);
        if (result != 1) {
            fprintf(stderr, "ERROR: failed to read from the receive fifo\n");
            return GG_FAILURE;
        }

        if (line[bytes_read] == '\n') {
            // end of line
            line[bytes_read] = '\0'; // null-terminate the line
            break;
        }

        ++bytes_read;
    }

    printf("<<< received %d bytes: %s\n", (int)bytes_read, line);

    // decode the base64 payload
    uint8_t decoded[GG_SERVICE_HOST_EXAMPLE_MAX_TRANSORT_LINE_LENGTH];
    size_t decoded_size = sizeof(decoded);
    GG_Result result = GG_Base64_Decode(line,
                                        bytes_read,
                                        decoded,
                                        &decoded_size,
                                        false);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: invalid base64\n");
        return result;
    }

    GG_DynamicBuffer* buffer = NULL;
    GG_DynamicBuffer_Create(decoded_size, &buffer);
    GG_DynamicBuffer_SetData(buffer, decoded, decoded_size);
    *data = GG_DynamicBuffer_AsBuffer(buffer);

    return GG_SUCCESS;
}

GG_IMPLEMENT_INTERFACE(FifoTransport, GG_RemoteTransport) {
    FifoTransport_Send,
    FifoTransport_Receive
};

//----------------------------------------------------------------------
static GG_Result
HelloWorldHandler_HandleRequest(GG_RemoteSmoHandler* _self,
                                const char*          request_method,
                                Fb_Smo*              request_params,
                                GG_JsonRpcErrorCode* rpc_error_code,
                                Fb_Smo**             rpc_result)
{
    GG_COMPILER_UNUSED(_self);
    GG_COMPILER_UNUSED(request_method);
    GG_COMPILER_UNUSED(request_params);
    GG_COMPILER_UNUSED(rpc_error_code);

    // create the result as a single integer
    *rpc_result = Fb_Smo_Create(NULL, "i", 3);

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(HelloWorldHandler, GG_RemoteSmoHandler) {
    HelloWorldHandler_HandleRequest
};

static void
InitHelloWorldHandler(HelloWorldHandler* handler)
{
    // setup interfaces
    GG_SET_INTERFACE(handler, HelloWorldHandler, GG_RemoteSmoHandler);
}

//----------------------------------------------------------------------
static void*
RemoteShellThreadMain(void* _args)
{
    ShellThreadArgs* args = _args;

    // init the hello world handler
    HelloWorldHandler hello_world_handler;
    InitHelloWorldHandler(&hello_world_handler);

    // create a remote shell
    GG_RemoteShell* shell = NULL;
    GG_RemoteShell_Create(GG_CAST(args->fifo_transport, GG_RemoteTransport), &shell);

    // register the local handlers with the shell
    GG_RemoteShell_RegisterSmoHandler(shell, "hello-world", GG_CAST(&hello_world_handler, GG_RemoteSmoHandler));

    // create a CoAP or blast service
    GG_CoapClientService* coap_client_service = NULL;
    GG_BlastService* blast_service = NULL;
    if (args->coap_endpoint) {
        // create and register a CoAP client service
        GG_Result result = GG_CoapClientService_Create(args->loop, args->coap_endpoint, &coap_client_service);
        if (GG_SUCCEEDED(result)) {
            GG_CoapClientService_Register(coap_client_service, shell);
        }
    } else {
        // create and register a blast service
        GG_Result result = GG_BlastService_Create(args->loop, &blast_service);
        if (GG_SUCCEEDED(result)) {
            GG_BlastService_Register(blast_service, shell);
            GG_BlastService_Attach(blast_service, args->stack_source, args->stack_sink);
        }
    }

    printf("=== remote shell thread starting\n");
    GG_RemoteShell_Run(shell);
    printf("=== remote shell thread ending\n");

    GG_CoapClientService_Destroy(coap_client_service);
    GG_BlastService_Destroy(blast_service);

    return NULL;
}

//----------------------------------------------------------------------
// Constants
//----------------------------------------------------------------------
static uint8_t PskIdentity[9] = {
  'B', 'O', 'O', 'T', 'S', 'T', 'R', 'A', 'P'
};
static uint8_t Psk[16] = {
    0x81, 0x06, 0x54, 0xe3, 0x36, 0xad, 0xca, 0xb0,
    0xa0, 0x3c, 0x60, 0xf7, 0x4a, 0xa0, 0xb6, 0xfb
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
// This example is designed to be used in combination with the python
// service_host_proxy.py example proxy that allows connecting this example
// command-line process to a remote API relay.
// This works by using fifos (on macOS and Linux)
// Two fifos need to be created for each instance of this command-line
// process. One fifo to send data from the process to the relay, and one
// to receive data back from the relay.
// ex:
// Setup:
//   mkfifo proc_to_relay_fifo
//   mkfifo relay_to_proc_fifo
// (only needs to be done once, the fifo files are persistent)
//
// In one shell:
//   gg-service-host-example hub coap proc_to_relay_fifo relay_to_proc_fifo
//
// In a second shell:
//   python service_host_proxy.py <url_of_relay> proc_to_relay_fifo relay_to_proc_fifo
// (ex: python service_host_proxy.py ws://127.0.0.1:8888/svchost/tracker proc_to_relay_fifo relay_to_proc_fifo)
//
// This should show the process as having joined a room with a role on the relay.
// Once that's done, you can run a remote API script that interacts with the
// process (example: remote_api_script_example.py)
//----------------------------------------------------------------------
int
main(int argc, char** argv)
{
    printf("=== Golden Gate Service Host Example ===\n");

    // parse command-line arguments
    if (argc < 5) {
        fprintf(stderr,
                "Usage: gg-service-host-example hub|node coap|blast <proc-to-relay-fifo> <relay-to-proc-fifo>\n");
        return 1;
    }

    // parse command line args
    GG_StackRole role;
    bool         coap = false;
    if (!strcmp(argv[1], "hub")) {
        role = GG_STACK_ROLE_HUB;
    } else if (!strcmp(argv[1], "node")) {
        role = GG_STACK_ROLE_NODE;
    } else {
        fprintf(stderr, "ERROR: invalid role\n");
        return 1;
    }
    if (!strcmp(argv[2], "coap")) {
        coap = true;
    } else if (!strcmp(argv[1], "blast")) {
        coap = false;
    } else {
        fprintf(stderr, "ERROR: invalid argument\n");
        return 1;
    }

    // create a fifo transport
    FifoTransport fifo_transport;
    printf(">>> opening process -> relay fifo\n");
    fifo_transport.send_fifo = fopen(argv[3], "w");
    if (fifo_transport.send_fifo == NULL) {
        fprintf(stderr, "ERROR: cannot open send fifo\n");
        return 1;
    }
    printf("<<< opening process <- relay fifo\n");
    fifo_transport.receive_fifo = fopen(argv[4], "r");
    if (fifo_transport.receive_fifo == NULL) {
        fprintf(stderr, "ERROR: cannot open receive fifo\n");
        return 1;
    }
    GG_SET_INTERFACE(&fifo_transport, FifoTransport, GG_RemoteTransport);

    // create a loop
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    GG_ASSERT(GG_SUCCEEDED(result));
    GG_Loop_BindToCurrentThread(loop);
    GG_ASSERT(GG_SUCCEEDED(result));

    // create a BSD socket to use as a transport
    uint16_t transport_receive_port = role == GG_STACK_ROLE_HUB ?
                                      GG_SERVICE_HOST_EXAMPLE_TRANSPORT_UDP_PORT_2 :
                                      GG_SERVICE_HOST_EXAMPLE_TRANSPORT_UDP_PORT_1;
    uint16_t transport_send_port = role == GG_STACK_ROLE_HUB ?
                                   GG_SERVICE_HOST_EXAMPLE_TRANSPORT_UDP_PORT_1 :
                                   GG_SERVICE_HOST_EXAMPLE_TRANSPORT_UDP_PORT_2;

    GG_DatagramSocket* transport_socket = NULL;
    GG_SocketAddress transport_local_address  = { GG_IP_ADDRESS_NULL_INITIALIZER, transport_receive_port };
    GG_SocketAddress transport_remote_address = { GG_IpAddress_Any,               transport_send_port    };
    GG_IpAddress_SetFromString(&transport_remote_address.address, GG_SERVICE_HOST_EXAMPLE_TRANSPORT_IP_ADDR);
    result = GG_BsdDatagramSocket_Create(&transport_local_address,
                                         &transport_remote_address,
                                         false,
                                         GG_SERVICE_HOST_EXAMPLE_MAX_DATAGRAM_SIZE,
                                         &transport_socket);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_BsdDatagramSocket_Create failed (%d)\n", result);
        return 1;
    }
    GG_DatagramSocket_Attach(transport_socket, loop);

    // prepare construction parameters
    GG_StackBuilderParameters parameters[4];
    size_t parameter_count = 0;

    // initialize a key resolver if needed
    static uint16_t cipher_suites[3] = {
        GG_TLS_PSK_WITH_AES_128_CCM,
        GG_TLS_PSK_WITH_AES_128_GCM_SHA256,
        GG_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256
    };

    if (role == GG_STACK_ROLE_HUB) {
        // setup a DTLS key resolver
        GG_SET_INTERFACE(&PskResolver, StaticPskResolver, GG_TlsKeyResolver);
        PskResolver.psk_identity      = PskIdentity;
        PskResolver.psk_identity_size = sizeof(PskIdentity);
        PskResolver.psk               = Psk;
        PskResolver.psk_size          = sizeof(Psk);

        static GG_TlsServerOptions dtls_server_parameters = {
            .base = {
                .cipher_suites       = cipher_suites,
                .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
            },
            .key_resolver = GG_CAST(&PskResolver, GG_TlsKeyResolver)
        };
        parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_SERVER;
        parameters[parameter_count].element_parameters = &dtls_server_parameters;
        ++parameter_count;
    } else {
        static GG_TlsClientOptions tls_options = {
            .base = {
                .cipher_suites       = cipher_suites,
                .cipher_suites_count = GG_ARRAY_SIZE(cipher_suites)
            },
            .psk_identity      = PskIdentity,
            .psk_identity_size = sizeof(PskIdentity),
            .psk               = Psk,
            .psk_size          = sizeof(Psk)
        };
        parameters[parameter_count].element_type = GG_STACK_ELEMENT_TYPE_DTLS_CLIENT;
        parameters[parameter_count].element_parameters = &tls_options;
        ++parameter_count;
    }

    // build the stack with all defaults
    GG_Stack* stack = NULL;
    result = GG_StackBuilder_BuildStack(GG_STACK_DESCRIPTOR_DTLS_SOCKET_NETIF_GATTLINK,
                                        parameters,
                                        parameter_count,
                                        role,
                                        NULL,
                                        loop,
                                        GG_DatagramSocket_AsDataSource(transport_socket),
                                        GG_DatagramSocket_AsDataSink(transport_socket),
                                        &stack);
    if (GG_FAILED(result)) {
        fprintf(stderr, "ERROR: GG_StackBuilder_BuildStack failed (%d)\n", result);
        return 1;
    }

    // get the stack source and sink
    GG_StackElementPortInfo stack_port;
    GG_Stack_GetPortById(stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &stack_port);

    // start the stack
    GG_Stack_Start(stack);

    // create a CoAP endpoint
    GG_CoapEndpoint* coap_endpoint = NULL;
    if (coap) {
        result = GG_CoapEndpoint_Create(GG_Loop_GetTimerScheduler(loop),
                                        stack_port.sink,
                                        stack_port.source,
                                        &coap_endpoint);
        if (GG_FAILED(result)) {
            fprintf(stderr, "ERROR: GG_CoapEndpoint_Create failed (%d)\n", result);
            return 1;
        }
    }

    // spawn a thread for the transport
    printf("=== spawning thread\n");
    ShellThreadArgs thread_args = {
        .loop           = loop,
        .fifo_transport = &fifo_transport,
        .stack_source   = stack_port.source,
        .stack_sink     = stack_port.sink,
        .coap_endpoint  = coap_endpoint
    };
    pthread_t thread;
    pthread_create(&thread, NULL, RemoteShellThreadMain, &thread_args);

    printf("+++ running loop\n");
    GG_Loop_Run(loop);
    printf("--- loop done\n");

    pthread_join(thread, NULL);
    GG_Loop_Destroy(loop);
    return 0;
}
