/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_boot.h
 * Description  : Bootloader interface (RS-internal HAL seam) for the
 *                ota_fw_update_over_ble RS.
 *
 *                The RS only STAGES an image into the secondary slot
 *                (BOOT_SECONDARY_BASE). Installing/activating it is the
 *                bootloader's job. This header is the single seam the RS uses to
 *                talk to that bootloader:
 *
 *                  swap_arm(BOOT_SWAP_TYPE_PERM);      // request, reset later
 *                  swap_and_reset(BOOT_SWAP_TYPE_PERM);// request and reset now
 *                  confirm_and_reset();                // only for trial swaps
 *                  last_result(&result);               // what happened on this boot
 *
 *                ARMING IS NOT TRIGGERING. swap_arm() records the request and
 *                returns; the RESET is what makes the bootloader act on it. That
 *                lets the application finish what it is doing first — drop the BLE
 *                link, park hardware, flush a log — and reset when it suits.
 *                swap_and_reset() is simply the two back to back.
 *
 *                A request armed this way is held in RAM, so it survives a warm
 *                reset but NOT a power cycle. An application that needs the
 *                request to outlive a power loss must keep its own non-volatile
 *                note and re-arm after it comes back up.
 *
 *                BACK-ENDS: two implementations live beside this header, selected
 *                by RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER:
 *                  ..._boot_rsboot.c  Renesas RA swap bootloader (default)
 *                  ..._boot_stub.c    no bootloader — argument-checking no-ops
 *                                     returning RS_STATUS_ERR_UNSUPPORTED
 *                A different bootloader (e.g. MCUboot) is ported by adding a third
 *                file that keeps these signatures.
 *
 *                Internal implementation boundary — NOT a public API contract.
 ******************************************************************************/
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_BOOT_H
#define RS_OTA_FW_UPDATE_OVER_BLE_BOOT_H

#include <stdint.h>
#include <stdbool.h>
#include "rs_ota_fw_update_over_ble.h"        /* rs_status_t + RS_STATUS_* codes */
#include "rs_ota_fw_update_over_ble_deps.h"   /* staging window board presets    */

#ifdef __cplusplus
extern "C" {
#endif

/* ---- slot geometry --------------------------------------------------------- *
 * The secondary (download/staging) slot is the window this RS erases and writes
 * during the SUOTA transfer. The primary slot is where the bootloader runs the
 * active image from — and where THIS application is linked (see script/ota.ld).
 * Both are adaptation points (override from the build system or edit
 * rs_ota_fw_update_over_ble_deps.h).
 *
 * The staged image is an ORDINARY application binary linked for the primary slot,
 * identical to the one already running there: no header, no signature, no per-slot
 * build. The bootloader physically moves it into the primary slot before it ever
 * executes, so it only ever runs from the address it was linked for. */
#ifndef BOOT_SECONDARY_BASE
 #define BOOT_SECONDARY_BASE   (RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_ADDR)
#endif
#ifndef BOOT_SECONDARY_SIZE
 #define BOOT_SECONDARY_SIZE   (RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_SIZE)
#endif
#ifndef BOOT_PRIMARY_BASE
 #define BOOT_PRIMARY_BASE     (0x00010000U)   /* running application slot */
#endif
#ifndef BOOT_PRIMARY_SIZE
 #define BOOT_PRIMARY_SIZE     (0x00018000U)   /* 96 KB, same as secondary */
#endif

/** Requested swap behaviour, mirroring the MCUboot swap types so a real
 *  bootloader port maps 1:1.
 *
 *  NOTE: these values are this RS's own numbering. A back-end whose bootloader
 *  numbers them differently (the RA swap bootloader does) must TRANSLATE, never
 *  cast. */
typedef enum e_boot_swap_type
{
    /** No swap pending / clear a previously requested swap. */
    BOOT_SWAP_TYPE_NONE   = 0,
    /** Trial (revert-able) swap: boot the staged image once; if it does not call
     *  confirm_and_reset(), the bootloader reverts on the next reset. */
    BOOT_SWAP_TYPE_TEST   = 1,
    /** Permanent swap: the staged image becomes the active one for good. */
    BOOT_SWAP_TYPE_PERM   = 2,
    /** Revert a trial image back to the previous one. */
    BOOT_SWAP_TYPE_REVERT = 3,
    /** Swap not possible (bad/unusable image in the secondary slot). */
    BOOT_SWAP_TYPE_FAIL   = 4,
} boot_swap_type_t;

/** Outcome the bootloader recorded for the CURRENT boot. */
typedef enum e_boot_result_code
{
    BOOT_RESULT_NONE = 0,   /* normal boot, nothing was swapped        */
    BOOT_RESULT_SWAPPED,    /* a staged image was installed this boot  */
    BOOT_RESULT_REVERTED,   /* trial image failed -> previous restored */
    BOOT_RESULT_BAD_IMAGE,  /* staged image rejected (header/CRC/sig)  */
    BOOT_RESULT_UNKNOWN,    /* no bootloader / no result available     */
} boot_result_code_t;

/** What the bootloader did on this boot, filled by last_result(). */
typedef struct st_boot_result
{
    boot_result_code_t code;             /* outcome of this boot                */
    boot_swap_type_t   swap_type;        /* swap that was performed/attempted   */
    bool               image_confirmed;  /* true once confirm_and_reset() stuck */
    uint32_t           image_size;       /* size of the active image, 0 if n/a  */
} boot_result_t;

/**
 * Record a request to install the image staged at BOOT_SECONDARY_BASE, WITHOUT
 * resetting. Returns immediately.
 *
 * Nothing happens until the next reset, whatever causes it — swap_and_reset(), a
 * watchdog, a button or a power cycle that the application handles itself. Use
 * this when there is work to finish first (close the BLE link, park hardware,
 * flush a log) and call the reset when convenient.
 *
 * @param[in] swap_type  BOOT_SWAP_TYPE_PERM for a normal update,
 *                       BOOT_SWAP_TYPE_TEST for a trial boot,
 *                       BOOT_SWAP_TYPE_NONE to discard a request armed earlier.
 * @retval RS_STATUS_SUCCESS         Request armed (or discarded).
 * @retval RS_STATUS_ERR_ASSERTION   Unknown @p swap_type.
 * @retval RS_STATUS_ERR_UNSUPPORTED BOOT_SWAP_TYPE_REVERT / _FAIL — the
 *                                   bootloader decides those itself; or no
 *                                   bootloader support in this build (stub).
 */
rs_status_t swap_arm(boot_swap_type_t swap_type);

/**
 * Request the bootloader to install the image staged at BOOT_SECONDARY_BASE and
 * reset so it takes effect. Call AFTER the transfer has been fully written and
 * verified. Equivalent to swap_arm() followed by a reset.
 *
 * Does not return on success with a real bootloader (the MCU resets). The one
 * exception is BOOT_SWAP_TYPE_NONE, which cancels and returns — resetting would
 * be the opposite of what was asked for.
 *
 * The staged image is validated by the BOOTLOADER, not here. If the secondary
 * slot holds nothing usable the request is refused, the board comes back up on
 * the image it was already running, and last_result() reports
 * BOOT_RESULT_BAD_IMAGE — so a bad request costs a reboot and nothing else.
 *
 * @param[in] swap_type  BOOT_SWAP_TYPE_PERM for a normal update,
 *                       BOOT_SWAP_TYPE_TEST for a trial boot (must be confirmed
 *                       later with confirm_and_reset()),
 *                       BOOT_SWAP_TYPE_NONE to cancel a pending request.
 * @retval RS_STATUS_ERR_ASSERTION   Unknown @p swap_type.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build (stub).
 */
rs_status_t swap_and_reset(boot_swap_type_t swap_type);

/**
 * Confirm the currently running image so the bootloader stops treating it as a
 * trial and never reverts it, then reset. ONLY meaningful after a
 * BOOT_SWAP_TYPE_TEST swap; on a permanent swap it is a no-op.
 *
 * Does not return with a real bootloader (the MCU resets) — the confirmation is
 * applied by the bootloader on the way back up.
 *
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build (stub).
 */
rs_status_t confirm_and_reset(void);

/**
 * Report what the bootloader did on this boot (installed / reverted / rejected
 * an image), so the application can notify the user or roll back its own state.
 *
 * Also the only way to learn that the running image is ON TRIAL: a result of
 * BOOT_RESULT_SWAPPED with swap_type == BOOT_SWAP_TYPE_TEST and
 * image_confirmed == false means the next reset reverts it unless
 * confirm_and_reset() is called first.
 *
 * "No report available" is not an error — it is the normal outcome of a cold
 * power-on, and @p p_result is filled with BOOT_RESULT_UNKNOWN.
 *
 * @param[out] p_result  Receives the boot outcome; always written when non-NULL.
 * @retval RS_STATUS_SUCCESS         @p p_result written (possibly UNKNOWN).
 * @retval RS_STATUS_ERR_ASSERTION   @p p_result was NULL.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build (stub);
 *                                   @p p_result is filled with BOOT_RESULT_UNKNOWN.
 */
rs_status_t last_result(boot_result_t * p_result);

/** How the bootloader in flash was identified by bootloader_info(). */
typedef enum e_boot_bl_source
{
    BOOT_BL_SOURCE_NONE = 0,    /* nothing at the bootloader address (erased)      */
    BOOT_BL_SOURCE_BUNDLED,     /* CRC32 matches the bootloader this app ships     */
    BOOT_BL_SOURCE_UNKNOWN,     /* something is there, but it is not recognised    */
} boot_bl_source_t;

/** Identity of the bootloader in flash, filled by bootloader_info(). */
typedef struct st_boot_bl_info
{
    boot_bl_source_t source;
    uint8_t          major;           /* valid for BUNDLED                         */
    uint8_t          minor;
    uint8_t          patch;
    uint32_t         crc32;           /* CRC32 of the first crc32_length bytes     */
    uint32_t         crc32_length;    /* 0 when no CRC was taken                   */
    bool             record_valid;    /* boot record in data flash checks out      */
    uint8_t          record_version;  /* its layout version, when record_valid     */
    uint32_t         record_sequence; /* times the record was rewritten, ditto     */
} boot_bl_info_t;

/**
 * Identify the bootloader currently in flash. Read-only; safe at any time the
 * flash is not being programmed (call it before a transfer starts).
 *
 * @param[out] p_info  Always written when non-NULL.
 * @retval RS_STATUS_SUCCESS         @p p_info written.
 * @retval RS_STATUS_ERR_ASSERTION   @p p_info was NULL.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build (stub).
 */
rs_status_t bootloader_info(boot_bl_info_t * p_info);

/** The secondary (staging) slot, filled by secondary_slot(). */
typedef struct st_boot_slot_info
{
    uint32_t base;
    uint32_t size;
    bool     image_present;   /* vector table looks like a primary-linked image */
} boot_slot_info_t;

/**
 * Describe the secondary slot and whether it currently holds something that
 * looks like an application image (the same vector-table check the bootloader
 * applies before a swap). Read-only.
 *
 * @param[out] p_slot  Always written when non-NULL.
 * @retval RS_STATUS_SUCCESS         @p p_slot written.
 * @retval RS_STATUS_ERR_ASSERTION   @p p_slot was NULL.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build (stub).
 */
rs_status_t secondary_slot(boot_slot_info_t * p_slot);

#ifdef __cplusplus
}
#endif

#endif /* RS_OTA_FW_UPDATE_OVER_BLE_BOOT_H */
