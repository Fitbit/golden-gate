/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <assert.h>
#include <string.h>

#include "os/os.h"
#include "sysinit/sysinit.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define MAIN_TASK_STACK_SIZE    (OS_STACK_ALIGN(1024))
#define MAIN_TASK_PRIORITY      (OS_MAIN_TASK_PRIO - 1)

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static os_stack_t g_main_task_stack[MAIN_TASK_STACK_SIZE];
static struct os_task g_main_task;

/*----------------------------------------------------------------------
|   main_task
+---------------------------------------------------------------------*/
static void main_task(void *arg)
{
    (void)arg;

    // Add your code here

    // In Mynewt, task functions should never return
    while (1) {
        os_time_delay(OS_TICKS_PER_SEC);
    }
}

/*----------------------------------------------------------------------
|   main
+---------------------------------------------------------------------*/
int main(int argc, char **argv)
{
    sysinit();

    os_task_init(&g_main_task, "_main", main_task, NULL,
                 MAIN_TASK_PRIORITY, OS_WAIT_FOREVER, g_main_task_stack,
                 MAIN_TASK_STACK_SIZE);

    while (1) {
        os_eventq_run(os_eventq_dflt_get());
    }

    return 0;
}
