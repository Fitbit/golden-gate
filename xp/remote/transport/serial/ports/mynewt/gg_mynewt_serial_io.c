/*
    Platform specific SerialIO implementation for Mynewt.
*/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "hal/hal_uart.h"
#include "os/os_cputime.h"
#include "os/os_dev.h"
#include "os/os_sem.h"
#include "os/os_time.h"

#include "xp/common/gg_buffer.h"
#include "xp/common/gg_memory.h"
#include "xp/common/gg_ring_buffer.h"
#include "xp/remote/gg_remote.h"
#include "xp/remote/transport/serial/gg_remote_serial_io.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define RING_BUFFER_CAPACITY    (128)

#define UART_PORT               (0)

/*----------------------------------------------------------------------
|   globals
+---------------------------------------------------------------------*/
static GG_RingBuffer rx_buffer;
static uint8_t data_buffer[RING_BUFFER_CAPACITY];
static struct os_sem rx_sem;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static int tx_cb(void *arg)
{
    return 0;
}

//----------------------------------------------------------------------
static void tx_done_cb(void *arg)
{
}

//----------------------------------------------------------------------
static int rx_cb(void *arg, uint8_t byte)
{
    if (GG_RingBuffer_GetSpace(&rx_buffer) >= 1) {
        GG_RingBuffer_Write(&rx_buffer, &byte, 1);
        os_sem_release(&rx_sem);
    }

    return 0;
}

//----------------------------------------------------------------------
static GG_Result SerialIO_ReadFrame(GG_SerialIO* _self, GG_Buffer **buffer) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    GG_Result gg_ret;
    uint8_t byte = 0;
    os_error_t ret;
    os_sr_t sr;

    while (true) {
        ret = os_sem_pend(&rx_sem, OS_TIMEOUT_NEVER);

        if (ret != OS_OK) {
            return GG_FAILURE;
        }

        OS_ENTER_CRITICAL(sr);
        if (GG_RingBuffer_GetAvailable(&rx_buffer) >= 1) {
            byte = GG_RingBuffer_ReadByte(&rx_buffer);
        }
        OS_EXIT_CRITICAL(sr);

        GG_SerialRemoteParser_PutData(self->parser, (char)byte);
        if (GG_SerialRemoteParser_IsFrameReceived(self->parser)) {
            gg_ret = GG_SerialRemoteParser_GetFramePayload(self->parser,
                                                           buffer);

            if (gg_ret != GG_SUCCESS) {
                return GG_FAILURE;
            }

            break;
        }
    }


    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result SerialIO_ReadAck(GG_SerialIO* _self) {
    SerialIO* self = GG_SELF(SerialIO, GG_SerialIO);
    uint32_t timeout_ticks;
    uint32_t timeout_ms;
    uint32_t cputime;
    uint8_t byte = 0;
    os_error_t ret;
    os_sr_t sr;

    timeout_ms = GG_ACK_FRAME_TIMEOUT;

    while (true) {
        ret = os_time_ms_to_ticks(timeout_ms, &timeout_ticks);

        if (ret != OS_OK) {
            return GG_FAILURE;
        }

        cputime = os_cputime_get32();
        ret = os_sem_pend(&rx_sem, timeout_ticks);
        cputime = os_cputime_get32() - cputime;

        if (ret != OS_OK) {
            return (ret == OS_TIMEOUT) ? GG_ERROR_TIMEOUT : GG_FAILURE;
        }

        if (timeout_ms > os_cputime_ticks_to_usecs(cputime) / 1000) {
            timeout_ms -= os_cputime_ticks_to_usecs(cputime) / 1000;
        } else {
            timeout_ms = 0;
        }

        OS_ENTER_CRITICAL(sr);
        if (GG_RingBuffer_GetAvailable(&rx_buffer) >= 1) {
            byte = GG_RingBuffer_ReadByte(&rx_buffer);
        }
        OS_EXIT_CRITICAL(sr);

        GG_SerialRemoteParser_PutData(self->parser, (char)byte);
        if (GG_SerialRemoteParser_IsAckReceived(self->parser)) {
            break;
        }
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
static GG_Result SerialIO_Write(GG_SerialIO* _self, GG_Buffer* buffer) {
    const uint8_t *data;
    size_t len;
    size_t i;

    GG_COMPILER_UNUSED(_self);

    data = GG_Buffer_GetData(buffer);
    len = GG_Buffer_GetDataSize(buffer);

    for (i = 0; i < len; i++) {
        hal_uart_blocking_tx(UART_PORT, data[i]);
    }

    return GG_SUCCESS;
}

//----------------------------------------------------------------------
GG_IMPLEMENT_INTERFACE(SerialIO, GG_SerialIO) {
    SerialIO_ReadFrame,
    SerialIO_ReadAck,
    SerialIO_Write
};

//----------------------------------------------------------------------
void SerialIO_Init(SerialIO* serial, GG_SerialRemoteParser* parser) {
    // init all the fields to 0
    memset(serial, 0, sizeof(*serial));

    serial->parser = parser;

    // Setup interfaces
    GG_SET_INTERFACE(serial, SerialIO, GG_SerialIO);

    GG_RingBuffer_Init(&rx_buffer, data_buffer, RING_BUFFER_CAPACITY);

    os_sem_init(&rx_sem, 0);

    /* Configure UART_PORT for gg-shell */
    hal_uart_close(UART_PORT);
    hal_uart_init_cbs(UART_PORT, tx_cb, tx_done_cb, rx_cb, NULL);
    hal_uart_config(UART_PORT, MYNEWT_VAL(GG_REMOTE_SHELL_BAUD), 8, 1,
                    HAL_UART_PARITY_NONE, HAL_UART_FLOW_CTL_NONE);
}
