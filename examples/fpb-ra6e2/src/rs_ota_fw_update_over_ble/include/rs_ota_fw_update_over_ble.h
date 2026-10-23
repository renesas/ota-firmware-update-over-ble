/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble.h
 * Description  : Public API for the "ota_fw_update_over_ble" RS.
 *
 *                Use case: the RA6E2 host acquires a firmware image over BLE
 *                from/through a DA14531 (SUOTA transfer) and stages it into the
 *                RA6E2 internal code flash for a later install/boot phase.
 *
 *                This is Product Enabling Software (reference code). Public
 *                functions are intuitive to the use case, not a formal/versioned
 *                API contract. See docs/ for adaptation guidance.
 *
 *  Reference board : RA6E2 EK/EVZ + DA14531
 *                    (presets in src/hal/rs_ota_fw_update_over_ble_deps.h).
 *  Dependencies    : FSP r_flash_hp, BLE (rm_ble_abs) + SUOTA server, FreeRTOS.
 *  Memory model    : no dynamic allocation (§12 rule 5); staging buffer is static.
 ******************************************************************************/
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_H
#define RS_OTA_FW_UPDATE_OVER_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>   /* NULL */

#ifdef __cplusplus
extern "C" {
#endif

/** RS-common status/return code. Success is zero (§12 rule 2). */
typedef int32_t rs_status_t;
#define RS_STATUS_SUCCESS        (0)
#define RS_STATUS_ERR_ASSERTION  (-1)   /* bad argument / null pointer            */
#define RS_STATUS_ERR_DEPENDENCY (-2)   /* dependency/config check failed (§8.3)  */
#define RS_STATUS_ERR_HARDWARE   (-3)   /* flash or BLE bring-up failed           */
#define RS_STATUS_ERR_OVERFLOW   (-4)   /* staging window would overflow          */
#define RS_STATUS_ERR_UNSUPPORTED (-5)  /* requested operation not implemented yet */

/** Execution mode. Non-blocking is the default (§12 rule 1): Start() arms the
 *  transfer and returns; progress is reported via StatusGet() and the optional
 *  event callback. */
typedef enum e_rs_ota_fw_update_over_ble_execution_mode
{
    RS_OTA_FW_UPDATE_OVER_BLE_EXECUTION_MODE_NON_BLOCKING = 0,
    RS_OTA_FW_UPDATE_OVER_BLE_EXECUTION_MODE_BLOCKING,
} rs_ota_fw_update_over_ble_execution_mode_t;

/** Progress/verification status of the acquisition workflow. Success is zero.
 *
 *  Lifecycle:  IDLE --Start()--> WAITING --client starts--> IN_PROGRESS
 *              IN_PROGRESS --> SUCCESS | BAD_HEADER | BAD_CRC | NO_MEMORY | TIMEOUT
 *              any --Stop()--> IDLE
 *
 *  NOTE: SUCCESS describes the IMAGE (complete, staged, verified), not a request
 *  to activate it. Use result::install_requested for that. */
typedef enum e_rs_ota_fw_update_over_ble_status
{
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_SUCCESS     = 0, /* image acquired + verified   */
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE,            /* not started / stopped       */
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IN_PROGRESS,     /* a transfer is running       */
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_HEADER,      /* SUOTA header inconsistent   */
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_CRC,         /* CRC mismatch                */
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_NO_MEMORY,       /* staging window overflow     */
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT,         /* transfer stalled/aborted    */
    /** Armed by Start(): the BLE link is up and advertising, but no client has
     *  started a transfer yet. Nothing has been erased or written. (Appended so
     *  the values above keep their numbers.) */
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_WAITING,
} rs_ota_fw_update_over_ble_status_t;

/** When the staged image should replace the running one. Consumed by
 *  rs_ota_fw_update_over_ble_Swap(); reserved for the bootloader/install phase. */
typedef enum e_rs_ota_fw_update_over_ble_swap_mode
{
    /** Swap now: install the staged image and reset immediately. */
    RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_NOW = 0,
    /** Mark the staged image as pending; the bootloader installs it on the next
     *  reset, which the application triggers when convenient. */
    RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET,
    /** Like AFTER_RESET but as a trial boot: unless the new image confirms
     *  itself, the bootloader reverts to the previous one. */
    RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET_WITH_TEST,
    /** Confirm a trial boot, making the running image permanent. */
    RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_CONFIRM,
    /** Clear a pending/trial request; keep running the current image. */
    RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_CANCEL,
} rs_ota_fw_update_over_ble_swap_mode_t;

/** Single application callback. The RS has NO output device of its own: it
 *  reports everything through this one sink — workflow status changes AND
 *  diagnostic lines. The application decides what to do (print over RTT/VCOM,
 *  react to status, drop it). Invoked from BLE/callback context; must be
 *  non-blocking. May be NULL.
 *
 *  @param[in] status     RS_STATUS_SUCCESS for informational/good states, an
 *                        RS_STATUS_ERR_* code for failures.
 *  @param[in] summary    NUL-terminated, human-readable line (ends in CRLF where
 *                        appropriate, so it can be emitted verbatim).
 *  @param[in] p_context  Opaque pointer supplied via cfg->p_context. */
typedef void (*rs_ota_fw_update_over_ble_cb_t)(
    rs_status_t   status,
    const char  * summary,
    void        * p_context);

/** Configuration supplied by the integrating application. A zeroed struct with
 *  a valid version/size uses the reference-board defaults.
 *
 *  Start() rejects (rather than ignores) any value it cannot honour:
 *  - version/size must be RS_OTA_FW_UPDATE_OVER_BLE_CFG_VERSION and
 *    sizeof(rs_ota_fw_update_over_ble_cfg_t)           -> else ERR_ASSERTION
 *  - mode: only NON_BLOCKING is implemented             -> BLOCKING: ERR_UNSUPPORTED
 *  - stage_addr/stage_size: the staging window is the bootloader's secondary
 *    slot and is fixed at build time (..._CFG_STAGE_ADDR/SIZE). Only 0 or the
 *    preset value itself is accepted                    -> else ERR_UNSUPPORTED
 *    To move the window, change the build-time defines AND the bootloader. */
#define RS_OTA_FW_UPDATE_OVER_BLE_CFG_VERSION  (1U)

typedef struct st_rs_ota_fw_update_over_ble_cfg
{
    uint16_t                                   version;    /* struct version (=1) */
    uint16_t                                   size;       /* sizeof(this struct) */
    rs_ota_fw_update_over_ble_execution_mode_t mode;       /* NON_BLOCKING only   */
    uint32_t                                   stage_addr; /* 0 or board preset   */
    uint32_t                                   stage_size; /* 0 or board preset   */
    rs_ota_fw_update_over_ble_cb_t             p_callback; /* optional, may be NULL */
    void *                                     p_context;  /* passed back to p_callback */
} rs_ota_fw_update_over_ble_cfg_t;

/** Result/state, inspectable by the application (§12 rule 7). */
typedef struct st_rs_ota_fw_update_over_ble_result
{
    rs_ota_fw_update_over_ble_status_t status;         /* final/current status */
    uint32_t                                bytes_received; /* bytes staged so far  */
    uint32_t                                code_size;      /* advertised code size */
    /** true once the BLE peer has explicitly ASKED for the staged image to be
     *  installed (SUOTA MEM_DEV REBOOT, 0xFD000000).
     *
     *  STATUS_SUCCESS on its own only means "a complete image is staged and
     *  verified" — the transfer may well end there, with the client deciding
     *  later (or never) to activate it. Installing on SUCCESS alone resets the
     *  board out from under a client that was not finished, so an application
     *  that follows the SUOTA flow must wait for THIS flag (or its own trigger,
     *  e.g. a button) before calling Swap().
     *
     *  Set only when the staged image verified OK: a REBOOT that arrives after a
     *  bad header/CRC is reported to the client and otherwise ignored. Cleared
     *  when a new transfer starts. Survives a disconnect, so a client that drops
     *  the link after asking for the reboot still gets its install. */
    bool                                    install_requested;
} rs_ota_fw_update_over_ble_result_t;

/** What the bootloader did on the boot that is currently running, as reported by
 *  rs_ota_fw_update_over_ble_BootResultGet(). */
typedef enum e_rs_ota_fw_update_over_ble_boot_outcome
{
    /** No report available: cold power-on, or no bootloader in this build. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_UNKNOWN = 0,
    /** Ordinary boot — nothing was installed or reverted. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_NORMAL,
    /** A staged image was installed and is what is running now. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_SWAPPED,
    /** A trial image did not confirm itself, so the previous one was restored. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_REVERTED,
    /** The staged image was rejected, or the install did not produce it. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_BAD_IMAGE,
} rs_ota_fw_update_over_ble_boot_outcome_t;

/** Outcome of the bootloader's work for the current boot. */
typedef struct st_rs_ota_fw_update_over_ble_boot_result
{
    rs_ota_fw_update_over_ble_boot_outcome_t outcome;
    /** true while the running image is ON TRIAL: it was installed with
     *  SWAP_MODE_AFTER_RESET_WITH_TEST and has not been confirmed, so the NEXT
     *  reset puts the previous image back unless Swap(CONFIRM) is called first. */
    bool                                     trial_pending;
} rs_ota_fw_update_over_ble_boot_result_t;

/**
 * Arm firmware acquisition: validate dependencies and bring up the DA14531
 * BLE/SUOTA link so the host can receive an image.
 *
 * This is the application's single entry point — it owns BLE bring-up. The
 * staging window is NOT erased here: it is erased once per download, when a
 * client actually starts a transfer, so an image staged earlier survives a reset.
 *
 * Non-blocking (§12 rule 1): the transport runs its own BLE task, so Start()
 * returns as soon as the link is up and the transfer is driven by BLE events.
 * Poll StatusGet() or supply cfg->p_callback to follow progress.
 *
 * @note The event callback and the staging updates run on the transport's BLE
 *       task, not the caller's. StatusGet() is safe to call from either.
 *
 * @param[in]  cfg         Configuration; NULL uses reference-board defaults.
 * @param[out] result_out  Optional; receives the initial state (may be NULL).
 * @retval RS_STATUS_SUCCESS         Armed successfully.
 * @retval RS_STATUS_ERR_ASSERTION   cfg has a wrong version/size or an unknown mode.
 * @retval RS_STATUS_ERR_UNSUPPORTED cfg asks for BLOCKING mode or a non-preset
 *                                   staging window (see rs_ota_fw_update_over_ble_cfg_t).
 * @retval RS_STATUS_ERR_DEPENDENCY  Dependency/config check failed.
 * @retval RS_STATUS_ERR_HARDWARE    BLE bring-up failed.
 */
rs_status_t rs_ota_fw_update_over_ble_Start(
    const rs_ota_fw_update_over_ble_cfg_t * cfg,
    rs_ota_fw_update_over_ble_result_t     * result_out);

/**
 * Stop acquisition and release the BLE/SUOTA link. Safe to call repeatedly.
 *
 * Shuts the transport down cleanly: an unfinished transfer is aborted (its
 * partial image invalidated), a connected peer is disconnected, advertising is
 * stopped and the BLE module is closed before the transport task is retired.
 * Blocks the caller until that is done (typically well under a second, bounded
 * at a few seconds).
 *
 * Must NOT be called from the event callback: that runs on the transport's own
 * task, which cannot wait for itself to stop.
 *
 * The state returns to IDLE silently: no callback is raised for it, because the
 * caller asked for it (StatusGet() reports IDLE).
 * @retval RS_STATUS_SUCCESS         Stopped cleanly (or already stopped).
 * @retval RS_STATUS_ERR_HARDWARE    The transport did not stop in time and was
 *                                   forced down; the state is still IDLE.
 * @retval RS_STATUS_ERR_ASSERTION   Called from the event callback; nothing was
 *                                   stopped and the state is unchanged.
 */
rs_status_t rs_ota_fw_update_over_ble_Stop(void);

/**
 * Read the current workflow state (poll for progress in non-blocking mode).
 *
 * Returns rs_status_t like the rest of the API; the workflow status itself is
 * reported through @p p_result, as with Start().
 *
 * @param[out] p_result  Receives status, bytes staged so far and code size.
 * @retval RS_STATUS_SUCCESS        State written to @p p_result.
 * @retval RS_STATUS_ERR_ASSERTION  @p p_result was NULL.
 */
rs_status_t rs_ota_fw_update_over_ble_StatusGet(
    rs_ota_fw_update_over_ble_result_t * p_result);

/**
 * Activate (swap to) the staged image.
 *
 * Routes to the bootloader seam (src/hal/rs_ota_fw_update_over_ble_boot.h):
 *   NOW                    -> swap_and_reset(BOOT_SWAP_TYPE_PERM)  [resets]
 *   AFTER_RESET            -> swap_arm(BOOT_SWAP_TYPE_PERM)        [returns]
 *   AFTER_RESET_WITH_TEST  -> swap_arm(BOOT_SWAP_TYPE_TEST)        [returns]
 *   CONFIRM                -> confirm_and_reset()                  [resets]
 *   CANCEL                 -> swap_arm(BOOT_SWAP_TYPE_NONE)        [returns]
 *
 * NOW installs immediately and does not return. The AFTER_RESET modes only
 * RECORD the request and return, leaving the application to reset when it suits —
 * after dropping the BLE link, parking hardware or flushing a log. Any reset then
 * applies it.
 *
 * @note An armed request lives in RAM: it survives a warm reset but NOT a power
 *       cycle. If the board loses power before resetting, the request is silently
 *       lost and the current image simply boots again.
 *
 * @warning AFTER_RESET_WITH_TEST installs the image ON TRIAL. Unless
 *          Swap(CONFIRM) is called once the new image has judged itself healthy,
 *          the next reset reverts to the previous image. Use
 *          BootResultGet()::trial_pending to detect that state after the swap.
 *
 * With the stub back-end (no bootloader in the build) every call validates its
 * argument and reports RS_STATUS_ERR_UNSUPPORTED without touching flash or reset
 * state.
 *
 * @param[in] mode  When the staged image should become the active one.
 * @retval RS_STATUS_SUCCESS         Swap performed/armed.
 * @retval RS_STATUS_ERR_ASSERTION   Unknown @p mode.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader/swap support in this build.
 */
rs_status_t rs_ota_fw_update_over_ble_Swap(
    rs_ota_fw_update_over_ble_swap_mode_t mode);

/**
 * Report what the bootloader did on this boot.
 *
 * Call it early in start-up to find out whether a previously staged image was
 * installed, reverted or rejected — and, critically, whether the running image is
 * still on trial. A trial image that never calls Swap(CONFIRM) is rolled back by
 * the next reset, so an application that uses AFTER_RESET_WITH_TEST must check
 * this and confirm once it is satisfied it is healthy.
 *
 * "No report" is not an error: after a cold power-on there is nothing to report
 * and @p p_result reads back as OUTCOME_UNKNOWN.
 *
 * @param[out] p_result  Receives the boot outcome; always written when non-NULL.
 * @retval RS_STATUS_SUCCESS         @p p_result written.
 * @retval RS_STATUS_ERR_ASSERTION   @p p_result was NULL.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build.
 */
rs_status_t rs_ota_fw_update_over_ble_BootResultGet(
    rs_ota_fw_update_over_ble_boot_result_t * p_result);

/** How BootloaderInfoGet() identified the bootloader in flash. */
typedef enum e_rs_ota_fw_update_over_ble_bootloader_id
{
    /** Nothing at the bootloader address, or no bootloader support in this build. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_NONE = 0,
    /** Byte-for-byte the bootloader this application ships with (CRC32 match);
     *  the version is the label that bundle was given. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_BUNDLED,
    /** A bootloader is present but not recognised - e.g. the board was updated
     *  over the air only and still runs an older one. See crc32. */
    RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_UNKNOWN,
} rs_ota_fw_update_over_ble_bootloader_id_t;

/** Identity of the bootloader in flash, and of the boot record it maintains. */
typedef struct st_rs_ota_fw_update_over_ble_bootloader_info
{
    rs_ota_fw_update_over_ble_bootloader_id_t id;
    uint8_t      major;            /**< valid for BUNDLED                              */
    uint8_t      minor;
    uint8_t      patch;
    uint32_t     crc32;            /**< fingerprint over crc32_length bytes            */
    uint32_t     crc32_length;     /**< 0 when no fingerprint was taken                */
    bool         record_valid;     /**< the boot record in data flash checks out       */
    uint8_t      record_version;   /**< its layout version, when record_valid          */
    uint32_t     record_sequence;  /**< how often it was rewritten, when record_valid  */
} rs_ota_fw_update_over_ble_bootloader_info_t;

/** The secondary (staging) slot. */
typedef struct st_rs_ota_fw_update_over_ble_slot_info
{
    uint32_t base;                 /**< first byte of the slot (memory mapped)         */
    uint32_t size;                 /**< slot size in bytes                             */
    /** true when the slot holds something that looks like an application image -
     *  either a download waiting to be installed, or the previous image that a
     *  swap moved out of the primary slot. */
    bool     image_present;
} rs_ota_fw_update_over_ble_slot_info_t;

/**
 * Identify the bootloader currently in flash.
 *
 * Read-only. Call it before Start() (or while no transfer is in progress): it
 * reads code and data flash directly and must not overlap a flash write.
 *
 * @param[out] p_info  Always written when non-NULL.
 * @retval RS_STATUS_SUCCESS         @p p_info written.
 * @retval RS_STATUS_ERR_ASSERTION   @p p_info was NULL.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build.
 */
rs_status_t rs_ota_fw_update_over_ble_BootloaderInfoGet(
    rs_ota_fw_update_over_ble_bootloader_info_t * p_info);

/**
 * Describe the secondary slot, so the application can look inside the image it
 * holds (for instance to read that image's version record).
 *
 * Read-only; same timing rule as BootloaderInfoGet(). The slot is memory mapped,
 * so [base, base + size) can be read directly while no transfer is running.
 *
 * @param[out] p_slot  Always written when non-NULL.
 * @retval RS_STATUS_SUCCESS         @p p_slot written.
 * @retval RS_STATUS_ERR_ASSERTION   @p p_slot was NULL.
 * @retval RS_STATUS_ERR_UNSUPPORTED No bootloader support in this build.
 */
rs_status_t rs_ota_fw_update_over_ble_SecondarySlotGet(
    rs_ota_fw_update_over_ble_slot_info_t * p_slot);

#ifdef __cplusplus
}
#endif

#endif /* RS_OTA_FW_UPDATE_OVER_BLE_H */
