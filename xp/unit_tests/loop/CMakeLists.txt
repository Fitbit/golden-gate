gg_add_test(test_gg_loop.cpp "gg-loop;gg-utils")
if (GG_PORTS_ENABLE_POSIX_THREADS)
    gg_add_test(test_gg_loop_with_threads.cpp "gg-loop")
endif()
