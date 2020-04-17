# Copyright 2017-2020 Fitbit, Inc
# SPDX-License-Identifier: Apache-2.0

gg_add_test(test_gg_gattlink.cpp "gg-gattlink;gg-common")
gg_add_test(test_gg_gattlink_generic_client.cpp "gg-gattlink;gg-common;gg-protocols;gg-loop;gg-utils")
gg_add_test(test_gg_gattlink_states.cpp "gg-gattlink;gg-common;gg-loop;gg-utils")
