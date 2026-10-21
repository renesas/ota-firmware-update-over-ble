/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_boot_stub.c
 * Description  : PLACEHOLDER bootloader back-end for the ota_fw_update_over_ble
 *                RS (see hal/rs_ota_fw_update_over_ble_boot.h).
 *
 *                The reference build has no bootloader, so the three seam calls
 *                only validate their arguments, trace the intent and report
 *                RS_STATUS_ERR_UNSUPPORTED. Nothing is written to flash and no
 *                reset is issued, so linking this file is always safe.
 *
 *                Replacing this file with a real port (MCUboot / RA bootloader)
 *                is the documented adaptation point:
 *                  - swap_arm()          : write the image-trailer swap request
 *                                          for the slot at BOOT_SECONDARY_BASE.
 *                  - swap_and_reset()    : swap_arm() + NVIC_SystemReset().
 *                  - confirm_and_reset() : set the image-OK flag of the running
 *                                          image (trial boots only).
 *                  - last_result()       : decode the bootloader's shared status
 *                                          record (RAM handoff / flash trailer).
 *
 *                The RA swap bootloader port already exists beside this file, in
 *                rs_ota_fw_update_over_ble_boot_rsboot.c. Exactly one of the two
 *                is compiled, chosen by
 *                RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER — the managed
 *                build compiles every .c in this folder, so the guard below is
 *                what keeps the two from defining the same symbols.
 *
 *                Portable by design: no FSP / BLE / SEGGER headers here — trace
 *                goes through the HAL log sink.
 ******************************************************************************/
#include "rs_ota_fw_update_over_ble_deps.h"

#if !(RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER)

#include "rs_ota_fw_update_over_ble_boot.h"
#include "rs_ota_fw_update_over_ble_hal.h"   /* rs_ota_fw_update_over_ble_hal_log() */

/* Remembers the last request so a debugger / the application can see what the
 * RS *would* have asked the bootloader to do. */
static boot_swap_type_t s_requested_swap = BOOT_SWAP_TYPE_NONE;

rs_status_t swap_arm(boot_swap_type_t swap_type)
{
    switch (swap_type)
    {
        case BOOT_SWAP_TYPE_NONE:
        case BOOT_SWAP_TYPE_TEST:
        case BOOT_SWAP_TYPE_PERM:
        case BOOT_SWAP_TYPE_REVERT:
            break;
        case BOOT_SWAP_TYPE_FAIL:
        default:
            return RS_STATUS_ERR_ASSERTION;
    }

    s_requested_swap = swap_type;

    rs_ota_fw_update_over_ble_hal_log(
        "[BOOT] swap_arm(type=%d) slot@0x%08X (%u KB) -- PLACEHOLDER, no bootloader\r\n",
        (int) swap_type, (unsigned) BOOT_SECONDARY_BASE,
        (unsigned) (BOOT_SECONDARY_SIZE / 1024U));

    /* TODO(bootloader phase): mark the secondary slot pending (trailer/magic). */
    return RS_STATUS_ERR_UNSUPPORTED;
}

rs_status_t swap_and_reset(boot_swap_type_t swap_type)
{
    rs_status_t rc = swap_arm(swap_type);

    if (RS_STATUS_ERR_ASSERTION == rc)
    {
        return rc;
    }

    rs_ota_fw_update_over_ble_hal_log(
        "[BOOT] swap_and_reset(type=%d) -- PLACEHOLDER, no reset issued\r\n",
        (int) swap_type);

    /* TODO(bootloader phase): NVIC_SystemReset(); this call would not return. */
    return RS_STATUS_ERR_UNSUPPORTED;
}

rs_status_t confirm_and_reset(void)
{
    rs_ota_fw_update_over_ble_hal_log(
        "[BOOT] confirm_and_reset() -- PLACEHOLDER, no bootloader\r\n");

    /* TODO(bootloader phase): set the running image's "image OK" flag so a trial
     *                         swap becomes permanent, then reset if required. */
    return RS_STATUS_ERR_UNSUPPORTED;
}

rs_status_t last_result(boot_result_t * p_result)
{
    if (NULL == p_result)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    /* Always leave the caller with a defined struct. */
    p_result->code            = BOOT_RESULT_UNKNOWN;
    p_result->swap_type       = s_requested_swap;
    p_result->image_confirmed = false;
    p_result->image_size      = 0U;

    /* TODO(bootloader phase): decode the bootloader's status handoff (shared RAM
     *                         area or image trailer) into *p_result. */
    return RS_STATUS_ERR_UNSUPPORTED;
}

rs_status_t bootloader_info(boot_bl_info_t * p_info)
{
    if (NULL == p_info)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    /* No bootloader in this build: nothing to identify. */
    p_info->source          = BOOT_BL_SOURCE_NONE;
    p_info->major           = 0U;
    p_info->minor           = 0U;
    p_info->patch           = 0U;
    p_info->crc32           = 0U;
    p_info->crc32_length    = 0U;
    p_info->record_valid    = false;
    p_info->record_version  = 0U;
    p_info->record_sequence = 0U;

    return RS_STATUS_ERR_UNSUPPORTED;
}

rs_status_t secondary_slot(boot_slot_info_t * p_slot)
{
    if (NULL == p_slot)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    /* The staging window exists with or without a bootloader, but without one
     * there is no notion of an installable image, so none is reported. */
    p_slot->base          = (uint32_t) BOOT_SECONDARY_BASE;
    p_slot->size          = (uint32_t) BOOT_SECONDARY_SIZE;
    p_slot->image_present = false;

    return RS_STATUS_ERR_UNSUPPORTED;
}

#endif /* !RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER */
