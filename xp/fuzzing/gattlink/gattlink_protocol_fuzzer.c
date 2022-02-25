/**
 *
 * @file
 *
 * @copyright
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "xp/gattlink/gg_gattlink.h"
#include "xp/common/gg_timer.h"
#include "xp/common/gg_utils.h"

/*----------------------------------------------------------------------
|   test Gattlink client
+---------------------------------------------------------------------*/
typedef struct {
    GG_IMPLEMENTS(GG_GattlinkClient);

    uint8_t mtu;
    uint8_t in_buffer[GG_GATTLINK_MAX_PACKET_SIZE];
    size_t  in_buffer_fullness;
    uint8_t out_buffer[GG_GATTLINK_MAX_PACKET_SIZE * 8];
    size_t  out_buffer_fullness;
} GattlinkFuzzClient;

static size_t
GattlinkFuzzClient_GetOutgoingDataAvailable(GG_GattlinkClient* _self)
{
    GattlinkFuzzClient* self = GG_SELF(GattlinkFuzzClient, GG_GattlinkClient);
    return self->out_buffer_fullness;
}

static GG_Result
GattlinkFuzzClient_GetOutgoingData(GG_GattlinkClient* _self, size_t offset, void* buffer, size_t size)
{
    GattlinkFuzzClient* self = GG_SELF(GattlinkFuzzClient, GG_GattlinkClient);
    memcpy(buffer, self->out_buffer, size);
    return GG_SUCCESS;
}

static void
GattlinkFuzzClient_ConsumeOutgoingData(GG_GattlinkClient* _self, size_t size)
{
    GattlinkFuzzClient* self = GG_SELF(GattlinkFuzzClient, GG_GattlinkClient);
    memmove(self->out_buffer, &self->out_buffer[size], size);
    self->out_buffer_fullness -= size;
}

static void
GattlinkFuzzClient_NotifyIncomingDataAvailable(GG_GattlinkClient* self)
{
    GG_COMPILER_UNUSED(self);
}

static size_t
GattlinkFuzzClient_GetTransportMaxPacketSize(GG_GattlinkClient* _self)
{
    GattlinkFuzzClient* self = GG_SELF(GattlinkFuzzClient, GG_GattlinkClient);
    return self->mtu;
}

static GG_Result
GattlinkFuzzClient_SendRawData(GG_GattlinkClient* self, const void* buffer, size_t size)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(buffer);
    GG_COMPILER_UNUSED(size);
    return GG_SUCCESS;
}

static void
GattlinkFuzzClient_NotifySessionReady(GG_GattlinkClient* self)
{
    GG_COMPILER_UNUSED(self);
}

static void
GattlinkFuzzClient_NotifySessionReset(GG_GattlinkClient* self)
{
    GG_COMPILER_UNUSED(self);
}

static void
GattlinkFuzzClient_NotifySessionStalled(GG_GattlinkClient* self, uint32_t stalled_time)
{
    GG_COMPILER_UNUSED(self);
    GG_COMPILER_UNUSED(stalled_time);
}

GG_IMPLEMENT_INTERFACE(GattlinkFuzzClient, GG_GattlinkClient) {
    GattlinkFuzzClient_GetOutgoingDataAvailable,
    GattlinkFuzzClient_GetOutgoingData,
    GattlinkFuzzClient_ConsumeOutgoingData,
    GattlinkFuzzClient_NotifyIncomingDataAvailable,
    GattlinkFuzzClient_GetTransportMaxPacketSize,
    GattlinkFuzzClient_SendRawData,
    GattlinkFuzzClient_NotifySessionReady,
    GattlinkFuzzClient_NotifySessionReset,
    GattlinkFuzzClient_NotifySessionStalled
};

/*----------------------------------------------------------------------
|   fuzzer target entry point
+---------------------------------------------------------------------*/
int
LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    GG_GattlinkSessionConfig config;
    GG_TimerScheduler* scheduler = NULL;
    GG_GattlinkProtocol* protocol = NULL;
    uint32_t now = 0;

    // Setup a timer scheduler
    GG_Result result = GG_TimerScheduler_Create(&scheduler);
    if (GG_FAILED(result)) {
        return 0;
    }

    // Setup a client
    GattlinkFuzzClient client = {0};
    client.mtu = 20;
    config.max_rx_window_size = 4;
    config.max_tx_window_size = 4;
    GG_SET_INTERFACE(&client, GattlinkFuzzClient, GG_GattlinkClient);

    // Create a protocol object
    result = GG_GattlinkProtocol_Create(
        GG_CAST(&client, GG_GattlinkClient),
        &config,
        scheduler,
        &protocol
    );
    if (GG_FAILED(result)) {
        goto end;
    }

    // The first input byte is the MTU
    if (size) {
        client.mtu = GG_MAX(client.mtu, *data);
        ++data;
        --size;
    }

    // Next byte if the config
    if (size) {
        config.max_rx_window_size = 1 + *data % 8;
        config.max_rx_window_size = 1 + *data % 8;
        ++data;
        --size;
    }

    // Start the protocol, we're ready to communicate
    result = GG_GattlinkProtocol_Start(protocol);
    if (GG_FAILED(result)) {
        goto end;
    }

    // Process the input as commands
    while (size--) {
        switch (*data++) {
            case 0:
                // Add the next byte to the input buffer
                if (size) {
                    client.in_buffer[client.in_buffer_fullness] = *data++;
                    ++client.in_buffer_fullness;

                    // Flush if we've reached the MTU
                    if (client.in_buffer_fullness >= client.mtu) {
                        GG_GattlinkProtocol_HandleIncomingRawData(protocol, client.in_buffer, client.in_buffer_fullness);
                        client.in_buffer_fullness = 0;
                    }

                    --size;
                }
                break;

            case 1:
                // Add the next byte to the output buffer
                if (size) {
                    // If there's too much out data, cut it in half
                    if (client.out_buffer_fullness == sizeof(client.out_buffer)) {
                        client.out_buffer_fullness /= 2;
                    }

                    client.out_buffer[client.out_buffer_fullness] = *data++;
                    ++client.out_buffer_fullness;
                    --size;
                }
                break;

            case 2:
                // Flush the input buffer
                if (client.in_buffer_fullness) {
                    GG_GattlinkProtocol_HandleIncomingRawData(protocol, client.in_buffer, client.in_buffer_fullness);
                    client.in_buffer_fullness = 0;
                }
                break;

            case 3:
                // Notify we have some output data
                if (client.out_buffer_fullness) {
                    GG_GattlinkProtocol_NotifyOutgoingDataAvailable(protocol);
                }
                break;

            case 4:
                // Consume some data
                if (size) {
                    size_t available = GG_GattlinkProtocol_GetIncomingDataAvailable(protocol);
                    size_t bytes_to_consume = GG_MIN(available, *data);
                    uint8_t throwaway[GG_GATTLINK_MAX_PACKET_SIZE];
                    if (bytes_to_consume) {
                        GG_GattlinkProtocol_GetIncomingData(protocol, 0, throwaway, bytes_to_consume);
                        GG_GattlinkProtocol_ConsumeIncomingData(protocol, bytes_to_consume);
                    }
                    ++data;
                    --size;
                }
                break;

            case 5:
                // Advance the timer by some number of milliseconds
                if (size) {
                    now += *data++;
                    GG_TimerScheduler_SetTime(scheduler, now);
                    --size;
                }
                break;
        }
    }

end:
    // Cleanup
    if (protocol) {
        GG_GattlinkProtocol_Destroy(protocol);
    }
    if (scheduler) {
        GG_TimerScheduler_Destroy(scheduler);
    }

    return 0;
}
