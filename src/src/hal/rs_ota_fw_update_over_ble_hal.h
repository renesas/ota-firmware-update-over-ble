/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_hal.h
 * Description  : Internal HAL interface of the ota_fw_update_over_ble RS
 *                (RS guidelines §9.2).
 *
 *                Everything target-specific lives behind this seam: the code
 *                flash staging driver, the DA14531 BLE/SUOTA transport and the
 *                debug log sink. The orchestrator, the verifier and the
 *                dependency check are written against THIS header only, so they
 *                contain no FSP/BLE-stack references and can be built on a host.
 *
 *                The reference implementation is
 *                rs_ota_fw_update_over_ble_hal_ra6e2.c — the only translation
 *                unit in the RS allowed to include FSP / BLE stack headers.
 *                A host test links tests/mocks/mock_fsp_ota_fw_update_over_ble.c
 *                instead.
 *
 *                Internal implementation boundary — NOT a public API contract.
 ******************************************************************************/
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_HAL_H
#define RS_OTA_FW_UPDATE_OVER_BLE_HAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "rs_ota_fw_update_over_ble.h"   /* rs_status_t + RS_STATUS_* codes */

#ifdef __cplusplus
extern "C" {
#endif

/** Events the transport raises towards the RS orchestrator. The orchestrator
 *  supplies these; the HAL invokes them from BLE/callback context (§8). */
typedef struct st_rs_ota_fw_update_over_ble_hal_callbacks
{
    /** A new transfer has begun and the staging window is prepared (erased).
     *  Raised by the transport before it lets the client stream image bytes. */
    void (*p_on_start)(void);
    /** A block of image bytes arrived (SUOTA patch data). */
    void (*p_on_block)(const uint8_t * p_data, uint32_t len);
    /** Transfer END received — flush and verify. This does NOT mean "install":
     *  it only means the image is complete in the staging window. */
    void (*p_on_end)(void);
    /** The peer explicitly asked for the staged image to be installed (SUOTA
     *  REBOOT). Raised after p_on_end, and only acted upon if the staged image
     *  verified OK. */
    void (*p_on_install_request)(void);
    /** Transport error / abort. */
    void (*p_on_error)(rs_status_t status);
} rs_ota_fw_update_over_ble_hal_callbacks_t;

/* ---- staging store (internal code flash) ---------------------------------- */

/** Prepare the staging window for a new download: erase ALL of it and reset the
 *  write offset.
 *
 *  Call it ONCE PER DOWNLOAD, not at boot: erasing at every start-up would
 *  destroy an already staged image.
 *
 *  WARNING: this keeps interrupts off for the sum of all block erase times
 *  (~0.7 s for a 96 KB window), which starves the DA14531 GTL/UART link and
 *  desyncs it. Only use it while the BLE link is DOWN. With a live connection,
 *  drive ..._stage_erase_block() one block per BLE main-loop pass instead.
 *  @retval RS_STATUS_SUCCESS / RS_STATUS_ERR_HARDWARE */
rs_status_t rs_ota_fw_update_over_ble_hal_stage_open(void);

/** Erase a single erase block of the staging window.
 *
 *  Interrupts are off only for that one block (tens to ~240 ms), so the caller
 *  can interleave block erases with R_BLE_Execute() and keep the BLE link alive.
 *  Block 0 also resets the write offset and page buffer.
 *  @param[in] index  0 .. ..._stage_block_count()-1.
 *  @retval RS_STATUS_SUCCESS / RS_STATUS_ERR_ASSERTION / RS_STATUS_ERR_HARDWARE */
rs_status_t rs_ota_fw_update_over_ble_hal_stage_erase_block(uint32_t index);

/** @return Number of erase blocks that make up the staging window. */
uint32_t rs_ota_fw_update_over_ble_hal_stage_block_count(void);

/** Append a received chunk to the staging window (buffered into program units).
 *  @retval RS_STATUS_SUCCESS / RS_STATUS_ERR_OVERFLOW / RS_STATUS_ERR_HARDWARE */
rs_status_t rs_ota_fw_update_over_ble_hal_stage_write(const uint8_t * p_data, uint32_t len);

/** Flush a partial final program unit (padded with 0xFF).
 *  @retval RS_STATUS_SUCCESS / RS_STATUS_ERR_HARDWARE */
rs_status_t rs_ota_fw_update_over_ble_hal_stage_end(void);

/** @return Number of bytes committed to the staging window so far. */
uint32_t rs_ota_fw_update_over_ble_hal_stage_written(void);

/** @return true while bytes are buffered but not yet committed to flash. */
bool rs_ota_fw_update_over_ble_hal_stage_has_pending(void);

/** Read back staged bytes (used by the verifier).
 *  @param[in]  offset  Byte offset from the start of the staging window.
 *  @param[out] p_dst   Destination buffer.
 *  @param[in]  len     Number of bytes to copy.
 *  @retval RS_STATUS_SUCCESS / RS_STATUS_ERR_ASSERTION / RS_STATUS_ERR_OVERFLOW */
rs_status_t rs_ota_fw_update_over_ble_hal_stage_read(uint32_t offset, void * p_dst, uint32_t len);

/* ---- transport (DA14531 BLE / SUOTA) -------------------------------------- */

/** Bring up the DA14531 GTL/BLE link and SUOTA server, registering @p p_cb for
 *  transfer events.
 *
 *  Returns as soon as the transport is running: the reference implementation
 *  starts a dedicated BLE task that owns the event loop, so transfer events are
 *  delivered on THAT task while the caller continues. Callers must therefore
 *  treat the orchestrator state as shared (see ..._hal_lock()).
 *  @retval RS_STATUS_SUCCESS / RS_STATUS_ERR_ASSERTION / RS_STATUS_ERR_HARDWARE */
rs_status_t rs_ota_fw_update_over_ble_hal_transport_open(
    const rs_ota_fw_update_over_ble_hal_callbacks_t * p_cb);

/** Tear down the SUOTA server / BLE link. Safe to call when already closed.
 *
 *  Cooperative: asks the transport's BLE loop to exit, waits for it to
 *  disconnect the peer, stop advertising and close the BLE module, then retires
 *  its task. Blocks the caller for up to a few seconds.
 *  @retval RS_STATUS_SUCCESS       Closed cleanly (or already closed).
 *  @retval RS_STATUS_ERR_HARDWARE  The loop did not stop in time; its task was
 *                                  deleted and the module closed forcibly.
 *  @retval RS_STATUS_ERR_ASSERTION Called from the transport's own task (e.g.
 *                                  from the RS callback); nothing was done. */
rs_status_t rs_ota_fw_update_over_ble_hal_transport_close(void);

/** true once ..._transport_close() has asked the BLE loop to exit. The loop
 *  checks it on every pass and breaks out. */
bool rs_ota_fw_update_over_ble_hal_transport_stop_requested(void);

/** Called by the BLE loop after it exited and BEFORE it closes the BLE module:
 *  aborts an unfinished transfer, disconnects the peer and stops advertising. */
void rs_ota_fw_update_over_ble_hal_transport_shutdown(void);

/* ---- ingress from the transport glue (SUOTA server) ----------------------- *
 * The DA14531 SUOTA server code (currently the QE-generated app_main SUOTA state
 * machine) forwards transfer events here; the HAL relays them to the
 * orchestrator-supplied callbacks. This keeps transport-specific glue out of the
 * orchestrator while giving the generated code one stable seam to call. */

/** A new transfer started and the staging window is ready to receive bytes.
 *  The transport raises this once it has prepared (erased) the staging window,
 *  just before it tells the client to start streaming. */
void rs_ota_fw_update_over_ble_hal_feed_start(void);
void rs_ota_fw_update_over_ble_hal_feed_block(const uint8_t * p_data, uint32_t len);
void rs_ota_fw_update_over_ble_hal_feed_end(void);

/** The peer asked for the staged image to be installed (SUOTA MEM_DEV REBOOT).
 *
 *  Deliberately separate from ..._hal_feed_end(): END only means the transfer is
 *  complete, and a client may end a transfer without ever asking for a reboot.
 *  The orchestrator latches the request (and ignores it unless the staged image
 *  verified OK); the APPLICATION decides when to reset. */
void rs_ota_fw_update_over_ble_hal_feed_install_request(void);

void rs_ota_fw_update_over_ble_hal_feed_error(rs_status_t status);

/* ---- link supervision ----------------------------------------------------- *
 * Guarantees the board always returns to advertising: after a disconnect, a
 * stalled or aborted transfer, or a lost DA14531 link. All the logic lives in the
 * HAL, so the (QE-generated) transport glue only has to call these three. */

/** BLE_GAP_EVENT_ADV_ON received: the DA14531 confirmed advertising is running. */
void rs_ota_fw_update_over_ble_hal_link_adv_on(void);

/** SPOTA_PATCH_LEN written by the client. Returns false when the block size is
 *  not supported: the caller answers SUOTA PATCH_LEN_ERROR, and every byte the
 *  client still streams in this transfer is dropped by ..._hal_feed_block(). */
bool rs_ota_fw_update_over_ble_hal_link_block_len_ok(uint16_t block_len);

/** Call on every BLE main-loop pass (the loop is woken at least every 250 ms).
 *  Never from a BLE callback: it may erase flash or re-open the BLE stack. */
void rs_ota_fw_update_over_ble_hal_link_poll(void);

/* ---- state protection ----------------------------------------------------- *
 * The transport delivers transfer events on its own BLE task while the
 * application calls the public API from another one, so the orchestrator's state
 * is shared. These bracket the (very short) critical sections around it. The
 * reference implementation uses an RTOS critical section; a bare-metal port can
 * mask interrupts, and a single-threaded host build makes them no-ops.
 *
 * Must be cheap and non-blocking: no user callback is ever invoked while held. */
void rs_ota_fw_update_over_ble_hal_lock(void);
void rs_ota_fw_update_over_ble_hal_unlock(void);

/* ---- diagnostics ---------------------------------------------------------- */

/** Give the HAL the application's single callback (cfg->p_callback) and its
 *  context (cfg->p_context) so the HAL's diagnostic helpers reach the app too.
 *  Pass NULL to disable reporting. */
void rs_ota_fw_update_over_ble_hal_set_cb(
    rs_ota_fw_update_over_ble_cb_t cb,
    void *                         p_context);

/** printf-style diagnostic helper. The reference implementation formats into a
 *  static buffer and forwards the line (tagged RS_STATUS_SUCCESS) to the
 *  registered callback; a host build discards it. Lines produced before a
 *  callback is registered are parked and replayed on registration.
 *
 *  This is the ONLY way RS code emits diagnostics: no part of the RS may call
 *  a debug transport (SEGGER RTT, UART, ...) itself - that choice belongs to the
 *  application. */
void rs_ota_fw_update_over_ble_hal_log(const char * p_fmt, ...);

/** Like ..._hal_log() but tags the line with an explicit @p status (e.g.
 *  RS_STATUS_ERR_HARDWARE) so the callback can filter/highlight errors. */
void rs_ota_fw_update_over_ble_hal_logs(rs_status_t status, const char * p_fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* RS_OTA_FW_UPDATE_OVER_BLE_HAL_H */
