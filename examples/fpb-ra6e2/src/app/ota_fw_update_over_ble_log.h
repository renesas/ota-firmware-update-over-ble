/******************************************************************************
 * File Name    : ota_fw_update_over_ble_log.h
 * Description  : Thin logging shim for the ota_fw_update_over_ble demo app,
 *                plus the application version, reset cause and boot banner.
 *
 *                All application trace output goes through OTA_LOG() /
 *                OTA_LOG_HEX(). The back-end is SEGGER RTT (channel 0),
 *                implemented in ota_fw_update_over_ble_log.c.
 *
 *                Logging is ENABLED BY DEFAULT. To silence every call site at
 *                compile time, define OTA_LOG_DISABLE in the project symbols.
 *                To swap in a different back-end (e.g. VCOM later), only
 *                ota_fw_update_over_ble_log.c has to change - every call site
 *                already goes through these macros.
 *
 *                WHERE DEBUG OUTPUT COMES FROM - the whole list:
 *                  OTA_LOG() / OTA_LOG_HEX()            this application only
 *                                                       (src/app)
 *                  the RS callback (cfg->p_callback)   every line the RS
 *                                                       produces - staging, verify,
 *                                                       boot seam, BLE/SUOTA server
 *                                                       - arrives as a ready-made
 *                                                       string and is forwarded to
 *                                                       OTA_LOG() by ota_cb()
 *                THE RS ITSELF NEVER CALLS SEGGER. src/app is the only place
 *                allowed to, which is what lets a customer drop the RS into an
 *                application with a different console. There is NO UART/CLI console
 *                either: SCI0 is wired to the DA14531 on PMOD2 and carries the
 *                GTL link, so a serial console would collide with the BLE
 *                transport. RTT rides the SWD/J-Link connection that is already
 *                attached for debugging and costs no pins. Output only - nothing
 *                reads the RTT down-buffer.
 *
 *                IMPORTANT DESIGN CONSTRAINT for whatever back-end is added:
 *                the SUOTA path logs from the BLE thread. The transport MUST
 *                NOT block when the host side is absent or not draining. A
 *                blocking write stalls R_BLE_Execute(), the DA14531 GTL/UART
 *                link stops being serviced, and the phone SUOTA app times out
 *                (~20 s) and disconnects. Use a non-blocking / drop-on-full
 *                write, and keep the SUOTA Patch Data hot path silent.
 ******************************************************************************/
#ifndef OTA_FW_UPDATE_OVER_BLE_LOG_H
#define OTA_FW_UPDATE_OVER_BLE_LOG_H

#include <stdint.h>
#include <stdbool.h>
#include "rs_ota_fw_update_over_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- application version - THE place to bump it ---------------------------
 * Semantic versioning, set by hand before EVERY image that goes out over the
 * air: MAJOR = incompatible (flash map, bootloader contract), MINOR = new
 * functionality, PATCH = fixes only. The boot banner compares it against the
 * image in the secondary slot; two builds with the same number are still told
 * apart by their build date/time. The version is embedded in the image as a
 * 64-byte 'APVR' record (ota_fw_update_over_ble_log.c), so it can also be found
 * in a raw .bin without symbols. */
#define APP_VERSION_MAJOR       (1U)
#define APP_VERSION_MINOR       (0U)
#define APP_VERSION_PATCH       (0U)
#define APP_VERSION_NAME        "ra6e2_da14531evz"      /* max 23 characters */

/* ---- reset cause (always compiled, independent of logging) ----------------
 * RSTSR0/1/2 survive a reset and only clear when written, so they are
 * snapshotted + cleared once from a constructor in ota_fw_update_over_ble_log.c
 * (runs from __init_array, before main() - no call site needed). */
/** Human-readable cause of the last reset, e.g. "WATCHDOG", "RESET PIN". */
const char * ota_log_reset_name(void);
/** Watchdog, low voltage or an error-detection reset: the last run failed. */
bool         ota_log_reset_abnormal(void);

/* Logging is on unless explicitly disabled by the project. */
#if !defined(OTA_LOG_DISABLE) && !defined(OTA_LOG_ENABLE)
 #define OTA_LOG_ENABLE
#endif

#ifdef OTA_LOG_ENABLE

/* Back-end: SEGGER RTT channel 0 (see ota_fw_update_over_ble_log.c). */
void ota_log_printf(const char * p_format, ...);
void ota_log_hex(const char * p_label, const unsigned char * p_data, unsigned int len);

/** Optional explicit init (configures RTT up-buffer 0 as non-blocking).
 *  Safe to skip - the first OTA_LOG() initialises the back-end lazily. */
void ota_log_init(void);

/** Boot banner: app + bootloader version, boot record, reset cause, boot
 *  outcome, secondary slot. Call BEFORE Start() (reads the flash directly). */
void ota_log_banner(const rs_ota_fw_update_over_ble_boot_result_t * p_boot);

/** Compact banner used INSTEAD of ota_log_banner() on a trial boot. */
void ota_log_banner_trial(uint32_t gate_ms);

/** One line describing the image in the secondary slot, versus this one.
 *  @p p_label is the left column, padded to 13 characters. */
void ota_log_slot_image(const char * p_label);

#define OTA_LOG(...)                  ota_log_printf(__VA_ARGS__)
#define OTA_LOG_HEX(label, p, len)    ota_log_hex((label), (const unsigned char *)(p), (unsigned int)(len))

#else /* logging disabled: compile every call site away */

#define ota_log_init()                ((void)0)
#define ota_log_banner(p_boot)        ((void)0)
#define ota_log_banner_trial(gate_ms) ((void)0)
#define ota_log_slot_image(p_label)   ((void)0)
#define OTA_LOG(...)                  ((void)0)
#define OTA_LOG_HEX(label, p, len)    ((void)0)

#endif /* OTA_LOG_ENABLE */

#ifdef __cplusplus
}
#endif

#endif /* OTA_FW_UPDATE_OVER_BLE_LOG_H */
