# Copyright 2021 Google LLC
# SPDX-License-Identifier: Apache-2.0

gg_add_fuzzer(coap_parser_fuzzer.c "gg-coap;gg-utils")
gg_add_fuzzer(coap_extended_error_fuzzer.c "gg-coap;gg-utils")
