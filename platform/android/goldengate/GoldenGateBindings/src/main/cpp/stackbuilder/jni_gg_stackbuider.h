// Copyright 2017-2020 Fitbit, Inc
// SPDX-License-Identifier: Apache-2.0

//
// Created by Michael Uyttersprot on 4/16/18.
//

#ifndef GOLDENGATEHOST_JNI_GG_STACKBUIDER_H
#define GOLDENGATEHOST_JNI_GG_STACKBUIDER_H

#define TLS_KEY_RESOLVER_CLASS_NAME "com/fitbit/goldengate/bindings/dtls/TlsKeyResolver"

#define TLS_KEY_RESOLVER_RESOLVE_NAME "resolve"

#define TLS_KEY_RESOLVER_RESOLVE_SIG "(Lcom/fitbit/goldengate/bindings/node/NodeKey;[B)[B"

#define STACK_CREATION_RESULT_CLASS_NAME "com/fitbit/goldengate/bindings/stack/StackCreationResult"
#define STACK_CREATION_RESULT_CONSTRUCTOR_SIG "(IJ)V"

#define GATTLINK_PROBE_DEFAULT_BUFFER_USAGE_THRESHOLD 400   // byte*seconds
#define GATTLINK_PROBE_DEFAULT_EVENT_DAMPING_TIME     20    // seconds
#define GATTLINK_PROBE_DEFAULT_WINDOW_SPAN            1000  // ms
#define GATTLINK_PROBE_DEFAULT_BUFFER_SAMPLE_COUNT    50    // samples

#endif
