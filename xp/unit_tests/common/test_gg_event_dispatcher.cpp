/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include "xp/common/gg_events.h"
#include "xp/common/gg_event_dispatcher.h"

#define TEST_EVENT_TYPE_1 10
#define TEST_EVENT_TYPE_2 11
#define TEST_EVENT_TYPE_3 12

static GG_EventDispatcher*  gg_event_dispatcher;

typedef struct {
    GG_IMPLEMENTS(GG_EventListener);
    GG_Event last_received_event;
    int total_events_received;
    bool deregister_on_event;
    bool deregister_all;
    bool emit_event;
    bool reregister_with_event1;
} TestListener;

typedef struct {
    GG_EventEmitterBase event_emitter;
} TestSource;

const int NUM_LISTENERS = 4;

// Init all listeners
static TestListener listeners[4];
static TestSource test_source_1;

static uint32_t one_event = TEST_EVENT_TYPE_1;

static uint32_t two_events[2] = {TEST_EVENT_TYPE_1, TEST_EVENT_TYPE_2};
static GG_EventDispatcher_ListenerNode node_2;

static uint32_t three_events[3] = {TEST_EVENT_TYPE_1, TEST_EVENT_TYPE_2, TEST_EVENT_TYPE_3};
static GG_EventDispatcher_ListenerNode node_3;

static uint32_t fake_event_type = 1;
static GG_EventDispatcher_ListenerNode node_4;

static void
TestListner_OnEvent(GG_EventListener* _self, const GG_Event* event) {
    TestListener* self = GG_SELF(TestListener, GG_EventListener);

    self->last_received_event = *event;
    ++self->total_events_received;

    if (self->deregister_on_event) {
        GG_EventDispatcher_RemoveListener(gg_event_dispatcher,
                                          GG_CAST(self, GG_EventListener));
    }

    if (self->deregister_all) {
        for (int i = 0; i < NUM_LISTENERS; ++i) {
            GG_EventDispatcher_RemoveListener(gg_event_dispatcher,
                                              GG_CAST(&listeners[i], GG_EventListener));
        }
    }

    if (self->emit_event) {
        GG_Event event_2 = {
            .type   = TEST_EVENT_TYPE_2,
            .source = &test_source_1,
        };

        GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_2);
    }

    if (self->reregister_with_event1) {
        GG_EventDispatcher_RemoveListener(gg_event_dispatcher,
                                          GG_CAST(self, GG_EventListener));

        GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                       GG_CAST(self, GG_EventListener),
                                       &one_event,
                                       1,
                                       NULL);
    }
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(TestListener, GG_EventListener) {
    .OnEvent = TestListner_OnEvent
};

static void TestListener_Init(TestListener* listener) {

    listener->total_events_received = 0;
    listener->deregister_on_event = false;
    listener->deregister_all = false;
    listener->emit_event = false;
    listener->reregister_with_event1 = false;

    GG_SET_INTERFACE(listener, TestListener, GG_EventListener);
}


TEST_GROUP(GG_EVENT_DISPATCHER)
{
  void setup(void) {

    for (int i = 0; i < NUM_LISTENERS; ++i) {
        TestListener_Init(&listeners[i]);
    }
    // Initialize The Event Dispatcher
    GG_EventDispatcher_Create(&gg_event_dispatcher);

    // Initialize the event emitter inside the test source
    GG_EventEmitterBase_Init(&test_source_1.event_emitter);

    // Set the listener for the source to be the dispatcher
    GG_EventEmitter_SetListener(GG_CAST(&test_source_1.event_emitter, GG_EventEmitter),
                                GG_EventDispatcher_AsEventListener(gg_event_dispatcher));
  }

  void teardown(void) {
  }
};

// Source -> EventDispatcher -> Event Listeners

TEST(GG_EVENT_DISPATCHER, Test_BasicDispatching) {
    int result;

    // Register with the event dispatcher, Dont allocate memory for the node
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[0], GG_EventListener),
                                            &one_event,
                                            1,
                                            NULL);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[1], GG_EventListener),
                                            two_events,
                                            2,
                                            &node_2);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[2], GG_EventListener),
                                            three_events,
                                            3,
                                            &node_3);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[3], GG_EventListener),
                                            &fake_event_type,
                                            1,
                                            &node_4);

    GG_Event event = {
        .type   = TEST_EVENT_TYPE_1,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_Event event_2 = {
        .type   = TEST_EVENT_TYPE_2,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_2);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[1].last_received_event.type);
    LONGS_EQUAL(2, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);

    GG_Event event_3 = {
        .type   = TEST_EVENT_TYPE_3,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_3);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[1].last_received_event.type);
    LONGS_EQUAL(3, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_3, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_EventDispatcher_Destroy(gg_event_dispatcher);
}


TEST(GG_EVENT_DISPATCHER, Test_Deregister) {
    int result;

    // Register with the event dispatcher, Dont allocate memory for the node
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[0], GG_EventListener),
                                            &one_event,
                                            1,
                                            NULL);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[1], GG_EventListener),
                                            two_events,
                                            2,
                                            &node_2);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[2], GG_EventListener),
                                            three_events,
                                            3,
                                            &node_3);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[3], GG_EventListener),
                                            &fake_event_type,
                                            1,
                                            &node_4);

    GG_Event event = {
        .type   = TEST_EVENT_TYPE_1,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_Event event_2 = {
        .type   = TEST_EVENT_TYPE_2,
        .source = &test_source_1,
    };

    GG_EventDispatcher_RemoveListener(gg_event_dispatcher,
                                      GG_CAST(&listeners[1], GG_EventListener));

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_2);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(2, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);

    GG_Event event_3 = {
        .type   = TEST_EVENT_TYPE_3,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_3);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(3, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_3, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_EventDispatcher_Destroy(gg_event_dispatcher);
}

TEST(GG_EVENT_DISPATCHER, Test_DeleteOnEvent) {
    int result;

    // Register with the event dispatcher, Dont allocate memory for the node
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[0], GG_EventListener),
                                            &one_event,
                                            1,
                                            NULL);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[1], GG_EventListener),
                                            two_events,
                                            2,
                                            &node_2);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[2], GG_EventListener),
                                            three_events,
                                            3,
                                            &node_3);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[3], GG_EventListener),
                                            &fake_event_type,
                                            1,
                                            &node_4);

    GG_Event event = {
        .type   = TEST_EVENT_TYPE_1,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_Event event_2 = {
        .type   = TEST_EVENT_TYPE_2,
        .source = &test_source_1,
    };

    listeners[2].deregister_on_event = true;

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_2);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[1].last_received_event.type);
    LONGS_EQUAL(2, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);

    GG_Event event_3 = {
        .type   = TEST_EVENT_TYPE_3,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_3);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(2, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_EventDispatcher_Destroy(gg_event_dispatcher);
}

TEST(GG_EVENT_DISPATCHER, Test_DeleteAllOnEvent) {
    int result;

    // Register with the event dispatcher, Dont allocate memory for the node
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[0], GG_EventListener),
                                            &one_event,
                                            1,
                                            NULL);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[1], GG_EventListener),
                                            two_events,
                                            2,
                                            &node_2);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[2], GG_EventListener),
                                            three_events,
                                            3,
                                            &node_3);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[3], GG_EventListener),
                                            &fake_event_type,
                                            1,
                                            &node_4);

    GG_Event event = {
        .type   = TEST_EVENT_TYPE_1,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_Event event_2 = {
        .type   = TEST_EVENT_TYPE_2,
        .source = &test_source_1,
    };

    listeners[1].deregister_all = true;

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_2);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);

    GG_Event event_3 = {
        .type   = TEST_EVENT_TYPE_3,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_3);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_EventDispatcher_Destroy(gg_event_dispatcher);
}


TEST(GG_EVENT_DISPATCHER, Test_DeleteLast) {
    int result;

    // Register with the event dispatcher, Dont allocate memory for the node
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[0], GG_EventListener),
                                            &one_event,
                                            1,
                                            NULL);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[1], GG_EventListener),
                                            two_events,
                                            2,
                                            &node_2);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[2], GG_EventListener),
                                            three_events,
                                            3,
                                            &node_3);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[3], GG_EventListener),
                                            &fake_event_type,
                                            1,
                                            &node_4);

    GG_Event event = {
        .type   = TEST_EVENT_TYPE_1,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_Event event_4 = {
        .type   = 1,
        .source = &test_source_1,
    };

    listeners[3].deregister_on_event = true;

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_4);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(1, listeners[3].total_events_received);

    GG_Event event_3 = {
        .type   = TEST_EVENT_TYPE_3,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_3);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(2, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_3, listeners[2].last_received_event.type);
    LONGS_EQUAL(1, listeners[3].total_events_received);


    GG_EventDispatcher_Destroy(gg_event_dispatcher);
}



TEST(GG_EVENT_DISPATCHER, Test_EmitOnEvent) {
    int result;

    // Register with the event dispatcher, Dont allocate memory for the node
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[0], GG_EventListener),
                                            &one_event,
                                            1,
                                            NULL);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[1], GG_EventListener),
                                            two_events,
                                            2,
                                            &node_2);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[2], GG_EventListener),
                                            three_events,
                                            3,
                                            &node_3);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[3], GG_EventListener),
                                            &fake_event_type,
                                            1,
                                            &node_4);

    GG_Event event = {
        .type   = TEST_EVENT_TYPE_1,
        .source = &test_source_1,
    };

    listeners[0].emit_event = true;
    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(2, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_Event event_2 = {
        .type   = TEST_EVENT_TYPE_2,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_2);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(3, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[1].last_received_event.type);
    LONGS_EQUAL(3, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);

    GG_Event event_3 = {
        .type   = TEST_EVENT_TYPE_3,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_3);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(3, listeners[1].total_events_received);
    LONGS_EQUAL(4, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_3, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_EventDispatcher_Destroy(gg_event_dispatcher);
}


TEST(GG_EVENT_DISPATCHER, Test_Reregister) {
    int result;

    // Register with the event dispatcher, Dont allocate memory for the node
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[0], GG_EventListener),
                                            &one_event,
                                            1,
                                            NULL);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[1], GG_EventListener),
                                            two_events,
                                            2,
                                            &node_2);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[2], GG_EventListener),
                                            three_events,
                                            3,
                                            &node_3);

    // Register with the event dispatcher
    result = GG_EventDispatcher_AddListener(gg_event_dispatcher,
                                            GG_CAST(&listeners[3], GG_EventListener),
                                            &fake_event_type,
                                            1,
                                            &node_4);

    GG_Event event = {
        .type   = TEST_EVENT_TYPE_1,
        .source = &test_source_1,
    };

    listeners[0].reregister_with_event1 = true;
    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(1, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(1, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);


    GG_Event event_2 = {
        .type   = TEST_EVENT_TYPE_2,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_2);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[1].last_received_event.type);
    LONGS_EQUAL(2, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_2, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);

    GG_Event event_3 = {
        .type   = TEST_EVENT_TYPE_3,
        .source = &test_source_1,
    };

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event_3);

    LONGS_EQUAL(1, listeners[0].total_events_received);
    LONGS_EQUAL(2, listeners[1].total_events_received);
    LONGS_EQUAL(3, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_3, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);

    GG_EventListener_OnEvent(test_source_1.event_emitter.listener, &event);

    LONGS_EQUAL(2, listeners[0].total_events_received);
    LONGS_EQUAL(3, listeners[1].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[1].last_received_event.type);
    LONGS_EQUAL(4, listeners[2].total_events_received);
    LONGS_EQUAL(TEST_EVENT_TYPE_1, listeners[2].last_received_event.type);
    LONGS_EQUAL(0, listeners[3].total_events_received);



    GG_EventDispatcher_Destroy(gg_event_dispatcher);
}
