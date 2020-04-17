# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

gg_add_test(test_gg_timer.cpp gg-common)
gg_add_test(test_gg_lists.cpp gg-common)
gg_add_test(test_gg_utils.cpp gg-common)
gg_add_test(test_gg_buffer.cpp gg-common)
gg_add_test(test_gg_ring_buffer.cpp gg-common)
gg_add_test(test_gg_strings.cpp gg-common)
gg_add_test(test_gg_logging.cpp gg-common)
gg_add_test(test_gg_event_dispatcher.cpp gg-common)
gg_add_test(test_gg_inspect.cpp gg-common)
gg_add_test(test_gg_bitstream.cpp gg-common)

if (GG_PORTS_ENABLE_POSIX_THREADS)
    gg_add_test(test_gg_logging_with_threads.cpp gg-common)
endif()
