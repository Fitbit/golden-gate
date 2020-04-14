/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "CppUTest/TestHarness.h"
#include "CppUTest/MemoryLeakDetectorNewMacros.h"

#include <pthread.h>
#include <errno.h>
#include <sys/time.h>

#include "xp/common/gg_common.h"

//----------------------------------------------------------------------
static pthread_cond_t wait_cond;
static pthread_mutex_t wait_mutex;

GG_SET_LOCAL_LOGGER("test.foo.bar")

//----------------------------------------------------------------------
TEST_GROUP(GG_LOGGING_THREADS)
{
    void setup(void) {
        int result = pthread_mutex_init(&wait_mutex, NULL);
        LONGS_EQUAL(0, result);
        result = pthread_cond_init(&wait_cond, NULL);
        LONGS_EQUAL(0, result);
    }

    void teardown(void) {
    }
};

//----------------------------------------------------------------------
static void
blocking_wait(uint64_t time_ms)
{
    struct timespec wait_time = {
        .tv_sec  = 0,
        .tv_nsec = (long)time_ms * 1000000
    };
    int lock_result = pthread_mutex_lock(&wait_mutex);
    LONGS_EQUAL(0, lock_result);
    int wait_result = pthread_cond_timedwait(&wait_cond, &wait_mutex, &wait_time);
    LONGS_EQUAL(ETIMEDOUT, wait_result);
    lock_result = pthread_mutex_unlock(&wait_mutex);
    LONGS_EQUAL(0, lock_result);
}

//----------------------------------------------------------------------
static void
log_some_stuff(const char* name)
{
    GG_LOG_FATAL("FATAL log from %s", name);
    GG_LOG_SEVERE("SEVERE log from %s", name);
    GG_LOG_WARNING("WARNING log from %s", name);
    GG_LOG_INFO("INFO log from %s", name);
    GG_LOG_FINE("FINE log from %s", name);
    GG_LOG_FINER("FINER log from %s", name);
    GG_LOG_FINEST("FINEST log from %s", name);
}

//----------------------------------------------------------------------
static volatile bool logging_thread_should_exit;
static void *
logging_thread_main(void * arg)
{
    (void)arg;

    while (!logging_thread_should_exit) {
        log_some_stuff("thread");

        blocking_wait(1);
    }

    GG_LOG_INFO("worker thread exiting");

    return 0;
}

//----------------------------------------------------------------------
TEST(GG_LOGGING_THREADS, Test_ThreadedLogging) {
    GG_LogManager_Initialize();

    pthread_t thread;
    int result = pthread_create(&thread, NULL, logging_thread_main, NULL);
    LONGS_EQUAL(0, result);

    struct timeval start;
    gettimeofday(&start, NULL);

    for (;;) {
        struct timeval now;
        gettimeofday(&now, NULL);
        double elapsed = ((double)now.tv_sec   + (double)now.tv_usec / 1000000.0) -
                         ((double)start.tv_sec + (double)start.tv_usec / 1000000.0);
        if (elapsed > 2.0) {
            break;
        }

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=OFF");
        log_some_stuff("main");

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=FINEST");
        log_some_stuff("main");

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=FINER");
        log_some_stuff("main");

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=FINE");
        log_some_stuff("main");

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=INFO");
        log_some_stuff("main");

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=WARNING");
        log_some_stuff("main");

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=SEVERE");
        log_some_stuff("main");

        blocking_wait(1);
        GG_LogManager_Configure("plist:.level=FATAL");
        log_some_stuff("main");
    }

    logging_thread_should_exit = true;
    result = pthread_join(thread, NULL);
    LONGS_EQUAL(0, result);
}
