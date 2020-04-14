/**
 * @file
 * @brief Non-Volatile Memory Access API
 *
 * @copyright
 * Copyright 2018 by Fitbit, Inc. All Rights Reserved
 *
 * @author Bogdan Davidoaia
 *
 * @date 2018-03-07
 *
 */

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#if defined(NRF52)
#include "nrf52.h"
#include "nrf52_bitfields.h"
#elif defined(NRF52840_XXAA)
#include "nrf52840.h"
#include "nrf52840_bitfields.h"
#else
#error "Unsupported BSP!"
#endif

#include "nvm.h"

/*----------------------------------------------------------------------
|   defines
+---------------------------------------------------------------------*/
#define NVM_VALUE_UNSED     (0xFFFFFFFF)
#define NVM_UICR_SIZE       (sizeof(NRF_UICR_Type) / sizeof(uint32_t *))
#define NVM_BASE            ((uint32_t *)NRF_UICR->CUSTOMER - (uint32_t *)NRF_UICR)

#define ADV_NAME_IDX        (0)
#define ADV_NAME_SIZE       (8)

#define PEER_ADDR_IDX       (8)
#define PEER_ADDR_SIZE      (2)

#define LOG_CONFIG_IDX      (10)
#define LOG_CONFIG_SIZE     (16)

// Some limit checks
#if (ADV_NAME_MAX_LEN + 1) > (4 * ADV_NAME_SIZE)
#error "(ADV_NAME_MAX_LEN + 1) > (4 * ADV_NAME_SIZE)"
#endif

#if (LOG_CONFIG_MAX_LEN + 1) > (4 * LOG_CONFIG_SIZE)
#error "(LOG_CONFIG_MAX_LEN + 1) > (4 * LOG_CONFIG_SIZE)"
#endif

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

//----------------------------------------------------------------------
static void nvm_set_access_mode(int mode)
{
    NRF_NVMC->CONFIG = (NRF_NVMC->CONFIG & ~NVMC_CONFIG_WEN_Msk) |
                       (mode << NVMC_CONFIG_WEN_Pos);
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}
}

//----------------------------------------------------------------------
static void nvm_write_u32(uint32_t *buf, int count, int idx)
{
    /**
     * The NVM module is based on using the CUSTOMER part of the nRF52 UICR
     * (User Information Configuration Registers) which has a perticular
     * behaviour.
     *
     * By default, all the bits in the registers are set to 1. Writing to the
     * registers only switches 1 to 0, but doesn't switch 0 to 1. As such,
     * an erase is needed beforehand to make sure all the bits can be written
     * (by resetting them to 1).
     *
     * However, the erase operation erases all the UICR (not just the CUSTOMER
     * part), so the UICR needs to be completely saved, modified by the NVM
     * write operation and finally restored.
     */
    uint32_t uicr[NVM_UICR_SIZE];
    int i;

    // Store UICR
    for (i = 0; i < NVM_UICR_SIZE; i++) {
        uicr[i] = ((__IO uint32_t *)NRF_UICR)[i];
    }

    // Erase UICR
    nvm_set_access_mode(NVMC_CONFIG_WEN_Een);

    NRF_NVMC->ERASEUICR =
        NVMC_ERASEUICR_ERASEUICR_Erase << NVMC_ERASEUICR_ERASEUICR_Pos;
    while (NRF_NVMC->READY == NVMC_READY_READY_Busy) {}

    // Update UICR data
    for (i = 0; i < count; i++) {
        uicr[NVM_BASE + idx + i] = buf[i];
    }

    // Restore UICR
    nvm_set_access_mode(NVMC_CONFIG_WEN_Wen);

    for (i = 0; i < NVM_UICR_SIZE; i++) {
        ((__IO uint32_t *)NRF_UICR)[i] = uicr[i];
    }

    // Set access mode to read enable
    nvm_set_access_mode(NVMC_CONFIG_WEN_Ren);
}

//----------------------------------------------------------------------
static void nvm_read_u32(uint32_t *buf, int count, int idx)
{
    int i;

    for (i = 0; i < count; i++) {
        buf[i] = NRF_UICR->CUSTOMER[idx + i];
    }
}

//----------------------------------------------------------------------
static nvm_error_t nvm_write_string(const char *str, int max_len,
                                    int nvm_size, int idx)
{
    uint32_t *buf;
    int len;

    len = strlen(str);

    if (len > max_len) {
        return NVM_EINVAL;
    }

    buf = (uint32_t *)calloc(nvm_size, sizeof(uint32_t));

    memcpy(buf, str, len);
    nvm_write_u32(buf, nvm_size, idx);

    free(buf);

    return NVM_OK;
}

//----------------------------------------------------------------------
static nvm_error_t nvm_read_string(char *name, size_t len,
                                   int nvm_size, int idx)
{
    uint32_t *buf;
    char *str_buf;
    nvm_error_t rc;

    buf = (uint32_t *)calloc(nvm_size, sizeof(uint32_t));
    str_buf = (char *)buf;

    nvm_read_u32(buf, nvm_size, idx);

    if (buf[0] == NVM_VALUE_UNSED) {
        rc = NVM_NOT_SET;
    } else if (strlen(str_buf) + 1 > len) {
        rc = NVM_EINVAL;
    } else {
        strcpy(name, str_buf);
        rc = NVM_OK;
    }

    free(buf);

    return rc;
}

//----------------------------------------------------------------------
nvm_error_t nvm_get_adv_name(char *name, size_t len)
{
    return nvm_read_string(name, len, ADV_NAME_SIZE, ADV_NAME_IDX);
}

//----------------------------------------------------------------------
nvm_error_t nvm_set_adv_name(const char *name)
{
    return nvm_write_string(name, ADV_NAME_MAX_LEN, ADV_NAME_SIZE, ADV_NAME_IDX);
}

//----------------------------------------------------------------------
nvm_error_t nvm_get_peer_addr(ble_addr_t *addr)
{
    uint32_t buf[PEER_ADDR_SIZE];

    nvm_read_u32(buf, PEER_ADDR_SIZE, PEER_ADDR_IDX);

    if (buf[0] == NVM_VALUE_UNSED) {
        return NVM_NOT_SET;
    }

    memcpy(addr, buf, sizeof(ble_addr_t));

    return NVM_OK;
}

//----------------------------------------------------------------------
nvm_error_t nvm_set_peer_addr(const ble_addr_t *addr)
{
    uint32_t buf[PEER_ADDR_SIZE];

    if (addr == NULL) {
        buf[0] = NVM_VALUE_UNSED;
    } else {
        memcpy(buf, addr, sizeof(ble_addr_t));
    }

    nvm_write_u32(buf, PEER_ADDR_SIZE, PEER_ADDR_IDX);

    return NVM_OK;
}

//----------------------------------------------------------------------
nvm_error_t nvm_get_log_config(char *config, size_t len)
{
    return nvm_read_string(config, len, LOG_CONFIG_SIZE, LOG_CONFIG_IDX);
}

//----------------------------------------------------------------------
nvm_error_t nvm_set_log_config(const char *config)
{
    return nvm_write_string(config, LOG_CONFIG_MAX_LEN, LOG_CONFIG_SIZE,
                            LOG_CONFIG_IDX);
}
