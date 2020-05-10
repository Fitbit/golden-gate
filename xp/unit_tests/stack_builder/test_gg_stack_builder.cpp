// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_common.h"
#include "xp/loop/gg_loop.h"
#include "xp/sockets/gg_sockets.h"
#include "xp/utils/gg_async_pipe.h"
#include "xp/utils/gg_memory_data_source.h"
#include "xp/utils/gg_memory_data_sink.h"
#include "xp/tls/gg_tls.h"
#include "xp/stack_builder/gg_stack_builder.h"
#include "xp/gattlink/gg_gattlink_generic_client.h"
#include "xp/sockets/gg_sockets.h"

//----------------------------------------------------------------------
TEST_GROUP(GG_STACK_BUILDER)
{
    void setup(void) {
    }

    void teardown(void) {
    }
};

TEST(GG_STACK_BUILDER, Test_Constructor) {
    GG_Stack* stack;

    GG_Loop* loop;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // null destructor (helps with code coverage)
    GG_Stack_Destroy(NULL);

    // empty stack descriptor
    result = GG_StackBuilder_BuildStack("", NULL, 0, GG_STACK_ROLE_HUB, NULL, loop, NULL, NULL, &stack);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

    // unknown element in the stack descriptor
    result = GG_StackBuilder_BuildStack("?", NULL, 0, GG_STACK_ROLE_HUB, NULL, loop, NULL, NULL, &stack);
    LONGS_EQUAL(GG_ERROR_NOT_SUPPORTED, result);

    // duplicate element in the stack descriptor
    result = GG_StackBuilder_BuildStack("GG", NULL, 0, GG_STACK_ROLE_HUB, NULL, loop, NULL, NULL, &stack);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

    // stack with a DTLS hub but no DTLS config
    result = GG_StackBuilder_BuildStack("DSNG", NULL, 0, GG_STACK_ROLE_HUB, NULL, loop, NULL, NULL, &stack);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

    // stack with a DTLS node but no DTLS config
    result = GG_StackBuilder_BuildStack("DSNG", NULL, 0, GG_STACK_ROLE_NODE, NULL, loop, NULL, NULL, &stack);
    LONGS_EQUAL(GG_ERROR_INVALID_PARAMETERS, result);

    // stack with an IP config
    GG_StackIpConfiguration ip_config = { 0 };
    GG_IpAddress_SetFromString(&ip_config.local_address, "1.2.3.4");
    GG_IpAddress_SetFromString(&ip_config.remote_address, "5.6.7.8");
    result = GG_StackBuilder_BuildStack(GG_STACK_DESCRIPTOR_SOCKET_NETIF_GATTLINK,
                                        NULL,
                                        0,
                                        GG_STACK_ROLE_NODE,
                                        &ip_config,
                                        loop,
                                        NULL,
                                        NULL,
                                        &stack);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_StackIpConfiguration stack_ip_config;
    GG_Stack_GetIpConfiguration(stack, &stack_ip_config);
    CHECK_TRUE(stack_ip_config.ip_mtu != 0);
    LONGS_EQUAL(GG_IpAddress_AsInteger(&ip_config.local_address),
                GG_IpAddress_AsInteger(&stack_ip_config.local_address));
    LONGS_EQUAL(GG_IpAddress_AsInteger(&ip_config.remote_address),
                GG_IpAddress_AsInteger(&stack_ip_config.remote_address));
    GG_Stack_Destroy(stack);

    // stack with a Gattlink config
    GG_StackElementGattlinkParameters gl_config = {
        .rx_window = 4,
        .tx_window = 4,
        .buffer_size = 2048,
        .initial_max_fragment_size = 128,
        .probe_config = NULL,
    };
    GG_StackBuilderParameters build_params[1] = {
        {
            .element_type = GG_STACK_ELEMENT_TYPE_GATTLINK,
            .element_parameters = &gl_config
        }
    };
    result = GG_StackBuilder_BuildStack(GG_STACK_DESCRIPTOR_GATTLINK_ONLY,
                                        build_params,
                                        GG_ARRAY_SIZE(build_params),
                                        GG_STACK_ROLE_NODE,
                                        NULL,
                                        loop,
                                        NULL,
                                        NULL,
                                        &stack);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(1, GG_Stack_GetElementCount(stack));
    GG_DtlsProtocolStatus dummy;
    result = GG_Stack_GetDtlsProtocolStatus(stack, &dummy);
    LONGS_EQUAL(result, GG_ERROR_NO_SUCH_ITEM);
    GG_Stack_Destroy(stack);

    // create a stack with a datagram socket
    GG_StackElementDatagramSocketParameters socket_parameters = {
        .local_port = 1234,
        .remote_port = 4567
    };
    GG_StackBuilderParameters socket_build_params[1] = {
        {
            .element_type = GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET,
            .element_parameters = &socket_parameters
        }
    };
    result = GG_StackBuilder_BuildStack(GG_STACK_DESCRIPTOR_SOCKET_NETIF_GATTLINK,
                                        socket_build_params,
                                        GG_ARRAY_SIZE(socket_build_params),
                                        GG_STACK_ROLE_NODE,
                                        NULL,
                                        loop,
                                        NULL,
                                        NULL,
                                        &stack);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(3, GG_Stack_GetElementCount(stack));

    GG_Loop_Destroy(loop);
}

//----------------------------------------------------------------------
TEST(GG_STACK_BUILDER, Test_ElementAccess) {
    GG_Stack* stack;

    GG_Loop* loop;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    result = GG_StackBuilder_BuildStack(GG_STACK_DESCRIPTOR_SOCKET_NETIF_GATTLINK,
                                        NULL,
                                        0,
                                        GG_STACK_ROLE_NODE,
                                        NULL,
                                        loop,
                                        NULL,
                                        NULL,
                                        &stack);
    LONGS_EQUAL(GG_SUCCESS, result);

    LONGS_EQUAL(3, GG_Stack_GetElementCount(stack));
    GG_StackElementInfo element_info;

    result = GG_Stack_GetElementByIndex(stack, 0, &element_info);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_STACK_ELEMENT_TYPE_DATAGRAM_SOCKET, element_info.type);

    GG_StackElementPortInfo port_info1;
    result = GG_Stack_GetPortById(stack, element_info.id, GG_STACK_PORT_ID_TOP, &port_info1);
    LONGS_EQUAL(GG_SUCCESS, result);

    GG_StackElementPortInfo port_info2;
    result = GG_Stack_GetPortById(stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &port_info2);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(port_info1.id, port_info2.id);

    result = GG_Stack_GetPortById(stack, 99999, GG_STACK_PORT_ID_TOP, &port_info1);
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_Stack_GetPortById(stack, GG_STACK_ELEMENT_ID_TOP, 99999, &port_info1);
    LONGS_EQUAL(GG_ERROR_NO_SUCH_ITEM, result);

    result = GG_Stack_GetElementByIndex(stack, 1, &element_info);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_STACK_ELEMENT_TYPE_IP_NETWORK_INTERFACE, element_info.type);

    result = GG_Stack_GetElementByIndex(stack, 2, &element_info);
    LONGS_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_STACK_ELEMENT_TYPE_GATTLINK, element_info.type);

    result = GG_Stack_GetElementByIndex(stack, 3, &element_info);
    LONGS_EQUAL(GG_ERROR_OUT_OF_RANGE, result);

    GG_Stack_Destroy(stack);
    GG_Loop_Destroy(loop);
}

//----------------------------------------------------------------------
static uint8_t PSK[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
static uint8_t PSK_IDENTITY[5] = {
    'h', 'e', 'l', 'l', 'o'
};

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

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);
    GG_Loop* loop;
    bool     timer_fired;
} Terminator;

static void
Terminator_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t elapsed)
{
    Terminator* self = GG_SELF(Terminator, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(elapsed);

    self->timer_fired = true;
    GG_Loop_RequestTermination(self->loop);
}

typedef struct {
    GG_IMPLEMENTS(GG_TimerListener);

    GG_Stack* stack;
} Resetter;

static void
Resetter_OnTimerFired(GG_TimerListener* _self, GG_Timer* timer, uint32_t time_elapsed)
{
    Resetter* self = GG_SELF(Resetter, GG_TimerListener);
    GG_COMPILER_UNUSED(timer);
    GG_COMPILER_UNUSED(time_elapsed);

    GG_Stack_Reset(self->stack);
}

GG_IMPLEMENT_INTERFACE(Resetter, GG_TimerListener) {
    .OnTimerFired = Resetter_OnTimerFired
};

typedef struct {
    GG_IMPLEMENTS(GG_EventListener);

    GG_Loop*          loop;
    GG_Stack*         node_stack;
    GG_Stack*         hub_stack;
    struct {
        GG_Timer*         timer;
        GG_TimerListener* timer_listener;
        uint32_t          delay;
    } next_steps[3];
    unsigned int next_steps_count;
    unsigned int step;
} DtlsStackListener;

static void
DtlsStackListener_OnEvent(GG_EventListener* _self, const GG_Event* event)
{
    DtlsStackListener* self = GG_SELF(DtlsStackListener, GG_EventListener);
    GG_Result  result;

    if (event->type == GG_EVENT_TYPE_STACK_EVENT_FORWARD) {
        const GG_StackForwardEvent* forwarded = (const GG_StackForwardEvent*)event;
        if (forwarded->forwarded->type == GG_EVENT_TYPE_TLS_STATE_CHANGE) {
            GG_DtlsProtocolStatus node_dtls_status;
            result = GG_Stack_GetDtlsProtocolStatus(self->node_stack, &node_dtls_status);
            CHECK_EQUAL(GG_SUCCESS, result);
            GG_DtlsProtocolStatus hub_dtls_status;
            result = GG_Stack_GetDtlsProtocolStatus(self->hub_stack, &hub_dtls_status);
            CHECK_EQUAL(GG_SUCCESS, result);
            if (node_dtls_status.state == GG_TLS_STATE_SESSION && hub_dtls_status.state == GG_TLS_STATE_SESSION) {
                // both stacks are in the SESSION state, move on to the next step
                CHECK_TRUE(self->step < self->next_steps_count);
                GG_Timer_Schedule(self->next_steps[self->step].timer,
                                  self->next_steps[self->step].timer_listener,
                                  self->next_steps[self->step].delay);
                ++self->step;
            }
        }
    }
}

GG_IMPLEMENT_INTERFACE(DtlsStackListener, GG_EventListener) {
    .OnEvent = DtlsStackListener_OnEvent
};

TEST(GG_STACK_BUILDER, Test_TwoStacksDtls) {
    GG_Result result;

    GG_Loop* loop;
    result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup async pipes for transport
    GG_AsyncPipe* p0;
    result = GG_AsyncPipe_Create(GG_Loop_GetTimerScheduler(loop), 8, &p0);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* p1;
    result = GG_AsyncPipe_Create(GG_Loop_GetTimerScheduler(loop), 8, &p1);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup a node stack
    GG_TlsClientOptions tls_client_options = {
        .base = {
            .cipher_suites       = NULL,
            .cipher_suites_count = 0
        },
        .psk_identity      = PSK_IDENTITY,
        .psk_identity_size = sizeof(PSK_IDENTITY),
        .psk               = PSK,
        .psk_size          = sizeof(PSK),
        .ticket            = NULL,
        .ticket_size       = 0
    };
    GG_StackBuilderParameters node_build_params[1] = {
        {
            .element_type = GG_STACK_ELEMENT_TYPE_DTLS_CLIENT,
            .element_parameters = &tls_client_options
        }
    };
    GG_Stack* node_stack = NULL;
    result = GG_StackBuilder_BuildStack("D",
                                        node_build_params,
                                        GG_ARRAY_SIZE(node_build_params),
                                        GG_STACK_ROLE_NODE,
                                        NULL,
                                        loop,
                                        GG_AsyncPipe_AsDataSource(p0),
                                        GG_AsyncPipe_AsDataSink(p1),
                                        &node_stack);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup a hub stack
    StaticPskResolver psk_resolver;
    GG_SET_INTERFACE(&psk_resolver, StaticPskResolver, GG_TlsKeyResolver);
    psk_resolver.psk_identity      = PSK_IDENTITY;
    psk_resolver.psk_identity_size = sizeof(PSK_IDENTITY);
    psk_resolver.psk               = PSK;
    psk_resolver.psk_size          = sizeof(PSK);

    GG_TlsServerOptions tls_server_options = {
        .base = {
            .cipher_suites       = NULL,
            .cipher_suites_count = 0
        },
        .key_resolver = GG_CAST(&psk_resolver, GG_TlsKeyResolver)
    };
    GG_StackBuilderParameters hub_build_params[1] = {
        {
            .element_type = GG_STACK_ELEMENT_TYPE_DTLS_SERVER,
            .element_parameters = &tls_server_options
        }
    };
    GG_Stack* hub_stack = NULL;
    result = GG_StackBuilder_BuildStack("D",
                                        hub_build_params,
                                        GG_ARRAY_SIZE(node_build_params),
                                        GG_STACK_ROLE_HUB,
                                        NULL,
                                        loop,
                                        GG_AsyncPipe_AsDataSource(p1),
                                        GG_AsyncPipe_AsDataSink(p0),
                                        &hub_stack);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check some ports on the node stack
    GG_StackElementPortInfo port_info;
    result = GG_Stack_GetPortById(node_stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &port_info);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(port_info.source != NULL);
    CHECK_TRUE(port_info.sink != NULL);
    result = GG_Stack_GetPortById(node_stack, GG_STACK_ELEMENT_ID_BOTTOM, GG_STACK_PORT_ID_BOTTOM, &port_info);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(port_info.source != NULL);
    CHECK_TRUE(port_info.sink != NULL);

    // check some ports on the hub stack
    result = GG_Stack_GetPortById(hub_stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &port_info);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(port_info.source != NULL);
    CHECK_TRUE(port_info.sink != NULL);
    result = GG_Stack_GetPortById(hub_stack, GG_STACK_ELEMENT_ID_BOTTOM, GG_STACK_PORT_ID_BOTTOM, &port_info);
    LONGS_EQUAL(GG_SUCCESS, result);
    CHECK_TRUE(port_info.source != NULL);
    CHECK_TRUE(port_info.sink != NULL);

    // listen to the stacks
    DtlsStackListener stack_listener;
    GG_SET_INTERFACE(&stack_listener, DtlsStackListener, GG_EventListener);
    stack_listener.loop = loop;
    stack_listener.node_stack = node_stack;
    stack_listener.hub_stack = hub_stack;
    GG_EventEmitter_SetListener(GG_Stack_AsEventEmitter(node_stack), GG_CAST(&stack_listener, GG_EventListener));
    GG_EventEmitter_SetListener(GG_Stack_AsEventEmitter(hub_stack), GG_CAST(&stack_listener, GG_EventListener));

    // setup a terminator
    Terminator terminator;
    GG_IMPLEMENT_INTERFACE(Terminator, GG_TimerListener) {
        Terminator_OnTimerFired
    };
    GG_SET_INTERFACE(&terminator, Terminator, GG_TimerListener);
    terminator.loop = loop;
    terminator.timer_fired = false;

    // create a termination timer with a long timeout (shouldn't fire) as a safety to ensure we don't get stuck
    GG_Timer* termination_timer;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &termination_timer);
    result = GG_Timer_Schedule(termination_timer, GG_CAST(&terminator, GG_TimerListener), 10000);

    // setup the steps for the DTLS listener
    stack_listener.next_steps[0].timer = termination_timer;
    stack_listener.next_steps[0].timer_listener = GG_CAST(&terminator, GG_TimerListener);
    stack_listener.next_steps[0].delay = 0;
    stack_listener.next_steps_count = 1;
    stack_listener.step = 0;

    // start the stacks
    GG_Stack_Start(node_stack);
    GG_Stack_Start(hub_stack);

    GG_Loop_Run(loop);

    // check the resulting states
    LONGS_EQUAL(1, stack_listener.step);
    GG_DtlsProtocolStatus dtls_status;
    result = GG_Stack_GetDtlsProtocolStatus(node_stack, &dtls_status);
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_TLS_STATE_SESSION, dtls_status.state);
    result = GG_Stack_GetDtlsProtocolStatus(hub_stack, &dtls_status);
    CHECK_EQUAL(GG_SUCCESS, result);
    LONGS_EQUAL(GG_TLS_STATE_SESSION, dtls_status.state);

    // cleanup
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(p0), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(p1), NULL);
    GG_Timer_Destroy(termination_timer);
    GG_Stack_Destroy(node_stack);
    GG_Stack_Destroy(hub_stack);
    GG_AsyncPipe_Destroy(p0);
    GG_AsyncPipe_Destroy(p1);
    GG_Loop_Destroy(loop);
}

//----------------------------------------------------------------------
typedef struct {
    unsigned int step;
    struct {
        GG_Timer*         timer;
        GG_TimerListener* timer_listener;
        uint32_t          delay;
    } steps[3];
    unsigned int step_count;
} SharedSteps;

typedef struct {
    GG_IMPLEMENTS(GG_EventListener);

    GG_Loop*     loop;
    bool         ready;
    bool*        peer_ready;
    SharedSteps* next_steps;
} GattlinkStackListener;

static void
GattlinkStackListener_OnEvent(GG_EventListener* _self, const GG_Event* event)
{
    GattlinkStackListener* self = GG_SELF(GattlinkStackListener, GG_EventListener);

    if (event->type == GG_EVENT_TYPE_STACK_EVENT_FORWARD) {
        const GG_StackForwardEvent* forwarded = (const GG_StackForwardEvent*)event;
        if (forwarded->forwarded->type == GG_EVENT_TYPE_GATTLINK_SESSION_READY) {
            self->ready = true;
            if (*self->peer_ready) {
                if (self->next_steps->step < self->next_steps->step_count) {
                    GG_Timer_Schedule(self->next_steps->steps[self->next_steps->step].timer,
                                      self->next_steps->steps[self->next_steps->step].timer_listener,
                                      self->next_steps->steps[self->next_steps->step].delay);
                }
                ++self->next_steps->step;
            }
        } else if (forwarded->forwarded->type == GG_EVENT_TYPE_GATTLINK_SESSION_RESET) {
            self->ready = false;
        }
    }
}

GG_IMPLEMENT_INTERFACE(GattlinkStackListener, GG_EventListener) {
    .OnEvent = GattlinkStackListener_OnEvent
};

TEST(GG_STACK_BUILDER, Test_TwoStacksGattlink) {
    GG_Result result;

    GG_Loop* loop;
    result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup async pipes for transport
    GG_AsyncPipe* p0;
    result = GG_AsyncPipe_Create(GG_Loop_GetTimerScheduler(loop), 8, &p0);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_AsyncPipe* p1;
    result = GG_AsyncPipe_Create(GG_Loop_GetTimerScheduler(loop), 8, &p1);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup a node stack
    GG_Stack* node_stack = NULL;
    result = GG_StackBuilder_BuildStack("G",
                                        NULL,
                                        0,
                                        GG_STACK_ROLE_NODE,
                                        NULL,
                                        loop,
                                        GG_AsyncPipe_AsDataSource(p0),
                                        GG_AsyncPipe_AsDataSink(p1),
                                        &node_stack);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup a hub stack
    GG_Stack* hub_stack = NULL;
    result = GG_StackBuilder_BuildStack("G",
                                        NULL,
                                        0,
                                        GG_STACK_ROLE_HUB,
                                        NULL,
                                        loop,
                                        GG_AsyncPipe_AsDataSource(p1),
                                        GG_AsyncPipe_AsDataSink(p0),
                                        &hub_stack);
    LONGS_EQUAL(GG_SUCCESS, result);

    // listen to the stacks
    GattlinkStackListener node_stack_listener;
    GattlinkStackListener hub_stack_listener;
    GG_SET_INTERFACE(&node_stack_listener, GattlinkStackListener, GG_EventListener);
    node_stack_listener.loop = loop;
    node_stack_listener.ready = false;
    node_stack_listener.peer_ready = &hub_stack_listener.ready;
    GG_SET_INTERFACE(&hub_stack_listener, GattlinkStackListener, GG_EventListener);
    hub_stack_listener.loop = loop;
    hub_stack_listener.ready = false;
    hub_stack_listener.peer_ready = &node_stack_listener.ready;

    GG_EventEmitter_SetListener(GG_Stack_AsEventEmitter(node_stack), GG_CAST(&node_stack_listener, GG_EventListener));
    GG_EventEmitter_SetListener(GG_Stack_AsEventEmitter(hub_stack), GG_CAST(&hub_stack_listener, GG_EventListener));

    // setup the terminator
    Terminator terminator;
    GG_IMPLEMENT_INTERFACE(Terminator, GG_TimerListener) {
        Terminator_OnTimerFired
    };
    GG_SET_INTERFACE(&terminator, Terminator, GG_TimerListener);
    terminator.loop = loop;
    terminator.timer_fired = false;

    // create a timer and schedule it for 10 seconds (but it shouldn't fire)
    GG_Timer* termination_timer;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &termination_timer);
    CHECK_EQUAL(GG_SUCCESS, result);
    result = GG_Timer_Schedule(termination_timer, GG_CAST(&terminator, GG_TimerListener), 10000);

    // setup one resetter for each stack
    Resetter node_resetter;
    node_resetter.stack = node_stack;
    GG_SET_INTERFACE(&node_resetter, Resetter, GG_TimerListener);
    GG_Timer* node_resetter_timer = NULL;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &node_resetter_timer);
    CHECK_EQUAL(GG_SUCCESS, result);
    Resetter hub_resetter;
    hub_resetter.stack = hub_stack;
    GG_SET_INTERFACE(&hub_resetter, Resetter, GG_TimerListener);
    GG_Timer* hub_resetter_timer = NULL;
    result = GG_TimerScheduler_CreateTimer(GG_Loop_GetTimerScheduler(loop), &hub_resetter_timer);
    CHECK_EQUAL(GG_SUCCESS, result);

    // setup the steps for the listeners
    SharedSteps steps;
    steps.steps[0].timer = node_resetter_timer;
    steps.steps[0].timer_listener = GG_CAST(&node_resetter, GG_TimerListener);
    steps.steps[0].delay = 10;
    steps.steps[1].timer = hub_resetter_timer;
    steps.steps[1].timer_listener = GG_CAST(&hub_resetter, GG_TimerListener);
    steps.steps[1].delay = 10;
    steps.steps[2].timer = termination_timer;
    steps.steps[2].timer_listener = GG_CAST(&terminator, GG_TimerListener);
    steps.steps[2].delay = 10;
    steps.step_count = 3;
    steps.step = 0;
    node_stack_listener.next_steps = &steps;
    hub_stack_listener.next_steps = &steps;

    // emit an MTU change event for the node stack
    GG_StackLinkMtuChangeEvent mtu_change_event = {
        .base = {
            .type = GG_EVENT_TYPE_LINK_MTU_CHANGE,
            .source = NULL
        },
        .link_mtu = 100
    };
    GG_EventListener_OnEvent(GG_Stack_AsEventListener(node_stack), &mtu_change_event.base);

    // start the stacks
    GG_Stack_Start(node_stack);
    GG_Stack_Start(hub_stack);

    GG_Loop_Run(loop);

    // check the states
    LONGS_EQUAL(3, steps.step);
    CHECK_TRUE(node_stack_listener.ready);
    CHECK_TRUE(hub_stack_listener.ready);

    // cleanup
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(p0), NULL);
    GG_DataSource_SetDataSink(GG_AsyncPipe_AsDataSource(p1), NULL);
    GG_Timer_Destroy(termination_timer);
    GG_Timer_Destroy(node_resetter_timer);
    GG_Timer_Destroy(hub_resetter_timer);
    GG_Stack_Destroy(node_stack);
    GG_Stack_Destroy(hub_stack);
    GG_AsyncPipe_Destroy(p0);
    GG_AsyncPipe_Destroy(p1);
    GG_Loop_Destroy(loop);
}

TEST(GG_STACK_BUILDER, Test_TwoStacksIpOnly) {
    GG_Result result;

    GG_Loop* loop;
    result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup a node stack
    GG_Stack* node_stack = NULL;
    result = GG_StackBuilder_BuildStack("SN",
                                        NULL,
                                        0,
                                        GG_STACK_ROLE_NODE,
                                        NULL,
                                        loop,
                                        NULL,
                                        NULL,
                                        &node_stack);
    LONGS_EQUAL(GG_SUCCESS, result);

    // setup a hub stack
    GG_Stack* hub_stack = NULL;
    result = GG_StackBuilder_BuildStack("SN",
                                        NULL,
                                        0,
                                        GG_STACK_ROLE_HUB,
                                        NULL,
                                        loop,
                                        NULL,
                                        NULL,
                                        &hub_stack);
    LONGS_EQUAL(GG_SUCCESS, result);

    // start the stacks
    GG_Stack_Start(node_stack);
    GG_Stack_Start(hub_stack);

    // setup a sink to receive data from the bottom of the hub stack
    GG_MemoryDataSink* hub_bottom_memory_sink = NULL;
    result = GG_MemoryDataSink_Create(&hub_bottom_memory_sink);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_StackElementPortInfo hub_bottom;
    result = GG_Stack_GetPortById(hub_stack, GG_STACK_ELEMENT_ID_BOTTOM, GG_STACK_PORT_ID_BOTTOM, &hub_bottom);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_DataSource_SetDataSink(hub_bottom.source, GG_MemoryDataSink_AsDataSink(hub_bottom_memory_sink));
    LONGS_EQUAL(GG_SUCCESS, result);

    // send a message through the top of the hub stack
    GG_StackElementPortInfo hub_top;
    result = GG_Stack_GetPortById(hub_stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &hub_top);
    LONGS_EQUAL(GG_SUCCESS, result);
    uint8_t message[4] = { 0x01, 0x02, 0x03, 0x04 };
    GG_StaticBuffer message_buffer;
    GG_StaticBuffer_Init(&message_buffer, message, sizeof(message));
    result = GG_DataSink_PutData(hub_top.sink,
                                 GG_StaticBuffer_AsBuffer(&message_buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the IP packet we received is what we expect
    GG_Buffer* ip_packet_buffer = GG_MemoryDataSink_GetBuffer(hub_bottom_memory_sink);
    LONGS_EQUAL(20 + 8 + sizeof(message), GG_Buffer_GetDataSize(ip_packet_buffer));
    const uint8_t* ip_packet = GG_Buffer_GetData(ip_packet_buffer);
    LONGS_EQUAL(169, ip_packet[12]);                       // IP src addr
    LONGS_EQUAL(254, ip_packet[13]);                       // IP src addr
    LONGS_EQUAL(0,   ip_packet[14]);                       // IP src addr
    LONGS_EQUAL(4,   ip_packet[15]);                       // IP src addr
    LONGS_EQUAL(169, ip_packet[16]);                       // IP dst addr
    LONGS_EQUAL(254, ip_packet[17]);                       // IP dst addr
    LONGS_EQUAL(0,   ip_packet[18]);                       // IP dst addr
    LONGS_EQUAL(5,   ip_packet[19]);                       // IP dst addr
    GG_MemoryDataSink_Reset(hub_bottom_memory_sink);

    // setup a sink to receive data from the bottom of the node stack
    GG_MemoryDataSink* node_bottom_memory_sink = NULL;
    result = GG_MemoryDataSink_Create(&node_bottom_memory_sink);
    LONGS_EQUAL(GG_SUCCESS, result);
    GG_StackElementPortInfo node_bottom;
    result = GG_Stack_GetPortById(node_stack, GG_STACK_ELEMENT_ID_BOTTOM, GG_STACK_PORT_ID_BOTTOM, &node_bottom);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_DataSource_SetDataSink(node_bottom.source, GG_MemoryDataSink_AsDataSink(node_bottom_memory_sink));
    LONGS_EQUAL(GG_SUCCESS, result);

    // send a message through the top of the node stack
    GG_StackElementPortInfo node_top;
    result = GG_Stack_GetPortById(node_stack, GG_STACK_ELEMENT_ID_TOP, GG_STACK_PORT_ID_TOP, &node_top);
    LONGS_EQUAL(GG_SUCCESS, result);
    result = GG_DataSink_PutData(node_top.sink,
                                 GG_StaticBuffer_AsBuffer(&message_buffer),
                                 NULL);
    LONGS_EQUAL(GG_SUCCESS, result);

    // check that the IP packet we received is what we expect
    ip_packet_buffer = GG_MemoryDataSink_GetBuffer(node_bottom_memory_sink);
    LONGS_EQUAL(20 + 8 + sizeof(message), GG_Buffer_GetDataSize(ip_packet_buffer));
    ip_packet = GG_Buffer_GetData(ip_packet_buffer);
    LONGS_EQUAL(169, ip_packet[12]);                       // IP src addr
    LONGS_EQUAL(254, ip_packet[13]);                       // IP src addr
    LONGS_EQUAL(0,   ip_packet[14]);                       // IP src addr
    LONGS_EQUAL(3,   ip_packet[15]);                       // IP src addr
    LONGS_EQUAL(169, ip_packet[16]);                       // IP dst addr
    LONGS_EQUAL(254, ip_packet[17]);                       // IP dst addr
    LONGS_EQUAL(0,   ip_packet[18]);                       // IP dst addr
    LONGS_EQUAL(2,   ip_packet[19]);                       // IP dst addr
    GG_MemoryDataSink_Reset(node_bottom_memory_sink);

    // cleanup
    GG_Stack_Destroy(node_stack);
    GG_Stack_Destroy(hub_stack);
    GG_Loop_Destroy(loop);
}

#if defined(GG_CONFIG_ENABLE_INSPECTION)
TEST(GG_STACK_BUILDER, Test_Inspect) {
    GG_Loop* loop = NULL;
    GG_Result result = GG_Loop_Create(&loop);
    LONGS_EQUAL(GG_SUCCESS, result)

    GG_Stack* stack = NULL;
    GG_TlsServerOptions tls_server_options = {
        .base = {
            .cipher_suites       = NULL,
            .cipher_suites_count = 0
        },
        .key_resolver = NULL
    };
    GG_StackBuilderParameters build_params[1] = {
        {
            .element_type = GG_STACK_ELEMENT_TYPE_DTLS_SERVER,
            .element_parameters = &tls_server_options
        }
    };
    result = GG_StackBuilder_BuildStack("DSNGA",
                                        build_params,
                                        GG_ARRAY_SIZE(build_params),
                                        GG_STACK_ROLE_HUB,
                                        NULL,
                                        loop,
                                        NULL,
                                        NULL,
                                        &stack);
    LONGS_EQUAL(GG_SUCCESS, result)

    GG_LoggingInspector* inspector = NULL;
    result = GG_LoggingInspector_Create("foo", GG_LOG_LEVEL_OFF, &inspector);

    result = GG_Inspectable_Inspect(GG_Stack_AsInspectable(stack), GG_LoggingInspector_AsInspector(inspector), NULL);
    LONGS_EQUAL(GG_SUCCESS, result)

    // we MUST reset the logging subsystem before destroying the logging inspector
    GG_LogManager_Configure(NULL);
    GG_LoggingInspector_Destroy(inspector);

    GG_Stack_Destroy(stack);
    GG_Loop_Destroy(loop);
}
#endif
