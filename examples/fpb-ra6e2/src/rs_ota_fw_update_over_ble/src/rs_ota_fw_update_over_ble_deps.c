/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_deps.c
 * Description  : Dependency / configuration validation for the
 *                ota_fw_update_over_ble RS (§8.3 configuration-check
 *                service). Fails deterministically when the reference staging
 *                assumptions are not internally consistent.
 ******************************************************************************/
#include "rs_ota_fw_update_over_ble_deps.h"

bool rs_ota_fw_update_over_ble_deps_check(void)
{
    const uint32_t addr  = RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_ADDR;
    const uint32_t size  = RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_SIZE;
    const uint32_t block = RS_OTA_FW_UPDATE_OVER_BLE_CFG_ERASE_BLOCK_SIZE;
    const uint32_t unit  = RS_OTA_FW_UPDATE_OVER_BLE_CFG_PROGRAM_UNIT;

    /* Staging window must be erase-block aligned and a whole number of blocks. */
    if ((0U == size) || (0U == block) || (0U == unit))
    {
        return false;
    }
    if ((addr % block) != 0U)
    {
        return false;
    }
    if ((size % block) != 0U)
    {
        return false;
    }
    /* Program buffer sizing in the HAL staging store assumes size is a multiple
     * of the program unit. */
    if ((size % unit) != 0U)
    {
        return false;
    }
    return true;
}
