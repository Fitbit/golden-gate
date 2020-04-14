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

#ifndef __NVM_H__
#define __NVM_H__

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "nimble/ble.h"

/*----------------------------------------------------------------------
|   defines
+---------------------------------------------------------------------*/
// Value is max allowed by BLE stack
#define ADV_NAME_MAX_LEN    (29)
#define LOG_CONFIG_MAX_LEN  (63)

/*----------------------------------------------------------------------
|   types
+---------------------------------------------------------------------*/

/**
 * Enum for NVM related error codes
 */
typedef enum  {
    NVM_OK = 0,
    NVM_EINVAL = 1,
    NVM_NOT_SET = 2,
}  nvm_error_t;

/*----------------------------------------------------------------------
|   functions
+---------------------------------------------------------------------*/

/**
 * Get device name used in BLE advertisments from Non-Volatile Memory
 * @note Returned name will be null-terminated, so the buffer should also have
 *       space for the terminator.
 *
 * @param name Pointer to buffer in which to return name
 * @param len Length of buffer in which to return name
 * @retrun NVM_OK on success, or negative number on failure
 */
nvm_error_t nvm_get_adv_name(char *name, size_t len);

/**
 * Set device name used in BLE advertisments in Non-Volatile Memory
 *
 * @param name Pointer to buffer that contains a null-terminated string with
 *             the name to be set
 * @retrun NVM_OK on success, or negative number on failure
 */
nvm_error_t nvm_set_adv_name(const char *name);

/**
 * Get peer BLE address from Non-Volatile Memory
 *
 * @param addr Pointer to ble_addr_t structure in which to return address.
 * @retrun NVM_OK on success, or negative number on failure
 */
nvm_error_t nvm_get_peer_addr(ble_addr_t *addr);

/**
 * Set peer BLE address in Non-Volatile Memory
 *
 * @param addr Pointer to ble_addr_t structure with the address to be set. If
 *             this parameter is NULL, then unset the address in NVM.
 * @retrun NVM_OK on success, or negative number on failure
 */
nvm_error_t nvm_set_peer_addr(const ble_addr_t *addr);

/**
 * Get log config string from Non-Volatile Memory
 * @note Returned string will be null-terminated, so the buffer should also
 *       have space for the terminator.
 *
 * @param name Pointer to buffer in which to return log config string
 * @param len Length of buffer in which to return log config string
 * @retrun NVM_OK on success, or negative number on failure
 */
nvm_error_t nvm_get_log_config(char *config, size_t len);

/**
 * Set log config string in Non-Volatile Memory
 *
 * @param name Pointer to buffer that contains a null-terminated string with
 *             the log config to be set
 * @retrun NVM_OK on success, or negative number on failure
 */
nvm_error_t nvm_set_log_config(const char *config);

#endif
