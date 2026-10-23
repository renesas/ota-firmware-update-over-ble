/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble.c
 * Description  : Orchestrator for the ota_fw_update_over_ble RS.
 *
 *                Target-agnostic use-case logic: it drives the staging store and
 *                the DA14531 SUOTA transport through the RS HAL seam, runs the
 *                image verifier, and maps the internal verify result onto the
 *                public status enum.
 *
 *                Per §9.2 this file depends only on RS-internal helpers and the
 *                HAL interface — it contains NO FSP / BLE-stack references and is
 *                host-unit-testable.
 ******************************************************************************/
#include "rs_ota_fw_update_over_ble.h"
#include "rs_ota_fw_update_over_ble_deps.h"
#include "rs_ota_fw_update_over_ble_verify.h"
#include "rs_ota_fw_update_over_ble_hal.h"
#include "rs_ota_fw_update_over_ble_boot.h"   /* swap_and_reset / confirm_and_reset / last_result */


/* ---- module state (static; no dynamic allocation, §12 rule 5) --------------
 * Transfer events are delivered on the transport's BLE task while the
 * application calls the public API from its own thread, so every access to this
 * state is bracketed by the HAL lock (§8). The lock is never held while a user
 * callback runs. */
static volatile rs_ota_fw_update_over_ble_status_t s_status =
    RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE;
static rs_ota_fw_update_over_ble_cb_t     s_callback;
static void *                             s_context;
static volatile uint32_t                  s_code_size;
/** Latched SUOTA REBOOT request: the peer asked for the staged image to be
 *  installed. Kept separate from s_status because "the image is good" and "the
 *  client wants it activated" are two different facts, and only the second one
 *  justifies resetting the board. */
static volatile bool                      s_install_requested;
/** Latched transfer failure (write error, overflow, failed final flush, abort).
 *  Cleared only when a new transfer starts: once part of the image is lost, a
 *  later END must not re-verify it - a headerless image skips the CRC and would
 *  be reported as SUCCESS. */
static volatile bool                      s_xfer_failed;

/** Full, printable line for each workflow status (ends in CRLF so the app can
 *  emit it verbatim, exactly like the HAL's diagnostic lines). */
static const char * status_summary(rs_ota_fw_update_over_ble_status_t st)
{
    switch (st)
    {
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_SUCCESS:     return "[OTA] transfer complete - image staged and verified\r\n";
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE:        return "[OTA] stopped\r\n";
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_WAITING:     return "[OTA] ready - waiting for a SUOTA client to start a transfer\r\n";
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IN_PROGRESS: return "[OTA] transfer started - receiving image into the secondary slot\r\n";
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_HEADER:  return "[OTA] transfer FAILED - bad SUOTA image header\r\n";
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_CRC:     return "[OTA] transfer FAILED - image CRC mismatch\r\n";
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_NO_MEMORY:   return "[OTA] transfer FAILED - image larger than the secondary slot\r\n";
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT:     return "[OTA] transfer FAILED - stalled, aborted or flash write error\r\n";
        default:                                           return "[OTA] unknown state\r\n";
    }
}

/** Map a workflow status to an RS_STATUS_* severity for the callback. */
static rs_status_t status_severity(rs_ota_fw_update_over_ble_status_t st)
{
    switch (st)
    {
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_SUCCESS:
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE:
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_WAITING:
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IN_PROGRESS:
            return RS_STATUS_SUCCESS;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_NO_MEMORY:
            return RS_STATUS_ERR_OVERFLOW;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT:
            return RS_STATUS_ERR_HARDWARE;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_HEADER:
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_CRC:
        default:
            return RS_STATUS_ERR_ASSERTION;
    }
}

static void set_status(rs_ota_fw_update_over_ble_status_t st)
{
    rs_ota_fw_update_over_ble_hal_lock();
    s_status = st;
    rs_ota_fw_update_over_ble_hal_unlock();

    /* IDLE is never announced. It is only ever entered because the CALLER did
     * something - Stop(), or a Start() that failed and already said so through
     * its return code - so a callback would just tell the application what it
     * already knows (and, on the install path, print noise right before the
     * reset). StatusGet() still reports it. */
    if (RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE == st)
    {
        return;
    }

    /* Notify outside the lock: the application callback is arbitrary code and
     * must never run inside a critical section (§8). */
    if (NULL != s_callback)
    {
        s_callback(status_severity(st), status_summary(st), s_context);
    }
}

/** Snapshot the current workflow state into a caller-supplied result struct.
 *  Taken under the lock so the caller sees a consistent set of fields. */
static void result_fill(rs_ota_fw_update_over_ble_result_t * p_result)
{
    rs_ota_fw_update_over_ble_hal_lock();
    p_result->status            = s_status;
    p_result->bytes_received    = rs_ota_fw_update_over_ble_hal_stage_written();
    p_result->code_size         = s_code_size;
    p_result->install_requested = s_install_requested;
    rs_ota_fw_update_over_ble_hal_unlock();
}

/* ---- transport event handlers (called from BLE/SUOTA context) -------------- */

/** Fail the current transfer for good: @p st is kept until the next on_start(). */
static void xfer_fail(rs_ota_fw_update_over_ble_status_t st)
{
    rs_ota_fw_update_over_ble_hal_lock();
    s_xfer_failed = true;
    rs_ota_fw_update_over_ble_hal_unlock();

    set_status(st);
}

/** A new transfer started: the transport has prepared (erased) the staging
 *  window and is about to let the client stream image bytes. */
static void on_start(void)
{
    s_code_size = 0U;

    /* A previous session's install request must not carry into this one: the
     * image it referred to is being overwritten right now. Neither must its
     * failure: this transfer gets a clean slate. */
    rs_ota_fw_update_over_ble_hal_lock();
    s_install_requested = false;
    s_xfer_failed       = false;
    rs_ota_fw_update_over_ble_hal_unlock();

    set_status(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IN_PROGRESS);
}

static void on_block(const uint8_t * p_data, uint32_t len)
{
    /* Part of the image is already lost: stop writing, so nothing is stored
     * after the gap. */
    if (s_xfer_failed)
    {
        return;
    }

    rs_status_t st = rs_ota_fw_update_over_ble_hal_stage_write(p_data, len);
    if (RS_STATUS_ERR_OVERFLOW == st)
    {
        xfer_fail(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_NO_MEMORY);
    }
    else if (RS_STATUS_SUCCESS != st)
    {
        xfer_fail(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT);
    }
    else
    {
        /* still receiving */
    }
}

static void on_end(void)
{
    /* A failed transfer stays failed: END (or the second END that REBOOT
     * issues) neither flushes nor re-verifies it. */
    if (s_xfer_failed)
    {
        return;
    }

    /* The final (partial) page must be in flash before verifying. A headerless
     * image skips the CRC check, so a failed flush would otherwise be reported
     * as SUCCESS and could be installed with a truncated tail. */
    if (RS_STATUS_SUCCESS != rs_ota_fw_update_over_ble_hal_stage_end())
    {
        xfer_fail(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT);
        return;
    }

    /* Verify into a local: the verifier takes a plain uint32_t *, and the shared
     * copy is only published (under the lock) once the result is known. */
    uint32_t code_size = 0U;
    rs_ota_fw_update_over_ble_verify_t v = rs_ota_fw_update_over_ble_verify(&code_size);

    rs_ota_fw_update_over_ble_hal_lock();
    s_code_size = code_size;
    rs_ota_fw_update_over_ble_hal_unlock();

    switch (v)
    {
        case RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_OK:
            set_status(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_SUCCESS);
            break;
        case RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_CRC:
            set_status(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_CRC);
            break;
        case RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_HEADER:
        default:
            set_status(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_HEADER);
            break;
    }
}

static void on_error(rs_status_t status)
{
    (void) status;
    xfer_fail(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT);
}

/** The peer asked for the staged image to be installed (SUOTA REBOOT).
 *
 *  Only latched when the staged image actually verified OK. A REBOOT that
 *  follows a bad header/CRC asks the board to boot an image that was already
 *  rejected, so it is dropped here; the transport has separately reported the
 *  verify failure to the client, which is the answer it is waiting for.
 *
 *  Latching - rather than resetting here - keeps the decision with the
 *  application: this runs on the BLE task, which still has a notification to
 *  deliver and a link to drop cleanly before anything reboots. */
static void on_install_request(void)
{
    rs_ota_fw_update_over_ble_hal_lock();
    bool staged_ok = (RS_OTA_FW_UPDATE_OVER_BLE_STATUS_SUCCESS == s_status);
    if (staged_ok)
    {
        s_install_requested = true;
    }
    rs_ota_fw_update_over_ble_hal_unlock();
}

static const rs_ota_fw_update_over_ble_hal_callbacks_t s_hal_cb =
{
    .p_on_start           = on_start,
    .p_on_block           = on_block,
    .p_on_end             = on_end,
    .p_on_install_request = on_install_request,
    .p_on_error           = on_error,
};

/* ---- public API ------------------------------------------------------------ */

/** Reject any configuration the RS would otherwise silently ignore.
 *  The staging window is fixed at build time (it is the bootloader's secondary
 *  slot, ..._CFG_STAGE_ADDR/SIZE), so stage_addr/stage_size only accept 0 or the
 *  preset itself, and only the non-blocking mode is implemented. */
static rs_status_t cfg_check(const rs_ota_fw_update_over_ble_cfg_t * cfg)
{
    if (NULL == cfg)
    {
        return RS_STATUS_SUCCESS;   /* NULL => reference-board defaults */
    }

    if ((RS_OTA_FW_UPDATE_OVER_BLE_CFG_VERSION != cfg->version) ||
        (sizeof(rs_ota_fw_update_over_ble_cfg_t) != cfg->size))
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    switch (cfg->mode)
    {
        case RS_OTA_FW_UPDATE_OVER_BLE_EXECUTION_MODE_NON_BLOCKING:
            break;
        case RS_OTA_FW_UPDATE_OVER_BLE_EXECUTION_MODE_BLOCKING:
            return RS_STATUS_ERR_UNSUPPORTED;
        default:
            return RS_STATUS_ERR_ASSERTION;
    }

    if (((0U != cfg->stage_addr) && (RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_ADDR != cfg->stage_addr)) ||
        ((0U != cfg->stage_size) && (RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_SIZE != cfg->stage_size)))
    {
        return RS_STATUS_ERR_UNSUPPORTED;
    }

    return RS_STATUS_SUCCESS;
}

rs_status_t rs_ota_fw_update_over_ble_Start(
    const rs_ota_fw_update_over_ble_cfg_t * cfg,
    rs_ota_fw_update_over_ble_result_t     * result_out)
{
    /* Reject unsupported settings before touching any state, so a caller never
     * believes a setting was applied when it was not. */
    rs_status_t rc = cfg_check(cfg);
    if (RS_STATUS_SUCCESS != rc)
    {
        return rc;
    }

    /* Configuration / dependency check (§8.3). */
    if (!rs_ota_fw_update_over_ble_deps_check())
    {
        return RS_STATUS_ERR_DEPENDENCY;
    }

    s_callback  = (NULL != cfg) ? cfg->p_callback : NULL;
    s_context   = (NULL != cfg) ? cfg->p_context  : NULL;
    s_code_size = 0U;

    /* Nothing has been asked of this session yet. Note this does NOT touch the
     * staging window: an image staged before the reset is still there, and the
     * application may install it (e.g. from its button) without a new download. */
    rs_ota_fw_update_over_ble_hal_lock();
    s_install_requested = false;
    s_xfer_failed       = false;
    rs_ota_fw_update_over_ble_hal_unlock();
    /* The RS has no output device of its own: give the HAL the same single
     * callback so its diagnostic lines reach the application too. */
    rs_ota_fw_update_over_ble_hal_set_cb(s_callback, s_context);

    /* Armed and WAITING for a client; the staging window is NOT erased here. It is
     * prepared by the transport when a download actually starts, so an already
     * staged image survives a reset (the transport then raises on_start, which is
     * what moves the state on to IN_PROGRESS). */
    set_status(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_WAITING);

    /* Bring up the DA14531 SUOTA transport and arm event delivery. The transport
     * owns its own BLE task, so this returns as soon as the link is running and
     * the transfer is driven by BLE events from then on (§12 rule 1). */
    if (RS_STATUS_SUCCESS != rs_ota_fw_update_over_ble_hal_transport_open(&s_hal_cb))
    {
        set_status(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE);
        return RS_STATUS_ERR_HARDWARE;
    }

    if (NULL != result_out)
    {
        result_fill(result_out);
    }

    return RS_STATUS_SUCCESS;
}

rs_status_t rs_ota_fw_update_over_ble_Stop(void)
{
    rs_status_t rc = rs_ota_fw_update_over_ble_hal_transport_close();

    /* Refused (called from the BLE task): the transport is still running, so
     * the state must not claim otherwise. */
    if (RS_STATUS_ERR_ASSERTION == rc)
    {
        return rc;
    }

    /* Clean or forced, the transport is down now. */
    set_status(RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IDLE);
    return rc;
}

rs_status_t rs_ota_fw_update_over_ble_StatusGet(
    rs_ota_fw_update_over_ble_result_t * p_result)
{
    if (NULL == p_result)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    result_fill(p_result);
    return RS_STATUS_SUCCESS;
}

rs_status_t rs_ota_fw_update_over_ble_Swap(
    rs_ota_fw_update_over_ble_swap_mode_t mode)
{
    /* Map the public swap mode onto the bootloader seam
     * (hal/rs_ota_fw_update_over_ble_boot.h). The image has already been written
     * to BOOT_SECONDARY_BASE by the staging store, so activation is just a swap
     * request, plus a reset if the caller wants it applied immediately.
     *
     * NOW resets; the AFTER_RESET modes only ARM the request and return, leaving
     * the application to reset when it suits (that is the whole difference
     * between them). With the placeholder back-end every call reports
     * RS_STATUS_ERR_UNSUPPORTED and nothing is modified. */
    switch (mode)
    {
        case RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_NOW:
            /* Permanent install, applied now; a real back-end does not return. */
            return swap_and_reset(BOOT_SWAP_TYPE_PERM);

        case RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET:
            /* Permanent install, applied by the next reset whenever it comes. */
            return swap_arm(BOOT_SWAP_TYPE_PERM);

        case RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_AFTER_RESET_WITH_TEST:
            /* Trial boot: must be confirmed later, otherwise it is reverted. */
            return swap_arm(BOOT_SWAP_TYPE_TEST);

        case RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_CONFIRM:
            /* Only meaningful for a trial swap. */
            return confirm_and_reset();

        case RS_OTA_FW_UPDATE_OVER_BLE_SWAP_MODE_CANCEL:
            /* Drop a pending/trial request and keep the running image. */
            return swap_arm(BOOT_SWAP_TYPE_NONE);

        default:
            return RS_STATUS_ERR_ASSERTION;
    }
}

rs_status_t rs_ota_fw_update_over_ble_BootResultGet(
    rs_ota_fw_update_over_ble_boot_result_t * p_result)
{
    if (NULL == p_result)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    /* Always leave the caller with a defined struct, even if the seam reports
     * that there is no bootloader to ask. */
    p_result->outcome       = RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_UNKNOWN;
    p_result->trial_pending = false;

    boot_result_t   seam;
    rs_status_t     rc = last_result(&seam);

    if (RS_STATUS_SUCCESS != rc)
    {
        return rc;
    }

    switch (seam.code)
    {
        case BOOT_RESULT_NONE:
            p_result->outcome = RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_NORMAL;
            break;

        case BOOT_RESULT_SWAPPED:
            p_result->outcome = RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_SWAPPED;
            break;

        case BOOT_RESULT_REVERTED:
            p_result->outcome = RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_REVERTED;
            break;

        case BOOT_RESULT_BAD_IMAGE:
            p_result->outcome = RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_BAD_IMAGE;
            break;

        case BOOT_RESULT_UNKNOWN:
        default:
            p_result->outcome = RS_OTA_FW_UPDATE_OVER_BLE_BOOT_OUTCOME_UNKNOWN;
            break;
    }

    /* On trial exactly while a TEST swap has been installed but not yet
     * confirmed. Anything else - including a permanent swap - is already final. */
    p_result->trial_pending = (BOOT_RESULT_SWAPPED == seam.code) &&
                              (BOOT_SWAP_TYPE_TEST == seam.swap_type) &&
                              (!seam.image_confirmed);

    return RS_STATUS_SUCCESS;
}

rs_status_t rs_ota_fw_update_over_ble_BootloaderInfoGet(
    rs_ota_fw_update_over_ble_bootloader_info_t * p_info)
{
    if (NULL == p_info)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    boot_bl_info_t seam;
    rs_status_t    rc = bootloader_info(&seam);

    /* The seam always fills its struct, so the copy is defined even when it
     * reports UNSUPPORTED (stub back-end: everything reads as "none"). */
    p_info->id = (BOOT_BL_SOURCE_BUNDLED == seam.source) ? RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_BUNDLED
               : (BOOT_BL_SOURCE_UNKNOWN == seam.source) ? RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_UNKNOWN
               : RS_OTA_FW_UPDATE_OVER_BLE_BOOTLOADER_ID_NONE;

    p_info->major           = seam.major;
    p_info->minor           = seam.minor;
    p_info->patch           = seam.patch;
    p_info->crc32           = seam.crc32;
    p_info->crc32_length    = seam.crc32_length;
    p_info->record_valid    = seam.record_valid;
    p_info->record_version  = seam.record_version;
    p_info->record_sequence = seam.record_sequence;

    return rc;
}

rs_status_t rs_ota_fw_update_over_ble_SecondarySlotGet(
    rs_ota_fw_update_over_ble_slot_info_t * p_slot)
{
    if (NULL == p_slot)
    {
        return RS_STATUS_ERR_ASSERTION;
    }

    boot_slot_info_t seam;
    rs_status_t      rc = secondary_slot(&seam);

    p_slot->base          = seam.base;
    p_slot->size          = seam.size;
    p_slot->image_present = seam.image_present;

    return rc;
}
