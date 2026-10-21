/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_deps.h
 * Description  : Internal (RS-private) dependencies, board presets and
 *                constants for the ota_fw_update_over_ble RS. NOT part of the
 *                public API — do not include from application code.
 *
 *                The values below are REFERENCE-BOARD assumptions for the RA6E2
 *                (EK/EVZ) + DA14531 (RS guidelines §8, §13.10). A customer
 *                adapting this RS to another board or memory map edits this file
 *                (or overrides the macros from the build system) — it is the
 *                primary adaptation point for the flash staging window.
 ******************************************************************************/
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_DEPS_H
#define RS_OTA_FW_UPDATE_OVER_BLE_DEPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- staging window (board preset) ---------------------------------------- *
 * Staging area = top 96 KB of the 256 KB code flash: three contiguous 32 KB
 * erase blocks in region 1 (0x00028000..0x0003FFFF inclusive, i.e. end exclusive
 * 0x40000). Region 1 uses 32 KB blocks, so 0x28000 (= 5 x 0x8000) is
 * block-aligned.
 *
 * NOTE: this window is exactly 96 KB. The SUOTA client transfers the firmware
 * plus its 64-byte AN-B-001 image header, so a proper .img must be <= 96 KB
 * TOTAL (header + code) to fit.
 *
 * With RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER this window is the
 * bootloader's SECONDARY slot and the running application occupies the matching
 * 96 KB primary slot at BOOT_PRIMARY_BASE. The two never overlap, so the staging
 * window cannot be eaten by application growth - it can only be overflowed by an
 * image that is itself too big for a slot.
 *
 * Data flash: the bootloader reserves the first 128 bytes (0x08000000..0x0800007F)
 * for its boot record and swap log. Any VEE/flash storage in the application must
 * start above that. */
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_ADDR
 #define RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_ADDR         (0x00028000U)
#endif
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_SIZE
 #define RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_SIZE         (0x00018000U)  /* 96 KB */
#endif

/* RA6E2 region-1 code-flash erase block size (32 KB). */
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_CFG_ERASE_BLOCK_SIZE
 #define RS_OTA_FW_UPDATE_OVER_BLE_CFG_ERASE_BLOCK_SIZE   (0x8000U)
#endif

/* RA6E2 code-flash program unit (minimum write granularity). */
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_CFG_PROGRAM_UNIT
 #define RS_OTA_FW_UPDATE_OVER_BLE_CFG_PROGRAM_UNIT       (128U)
#endif

/* SUOTA single-image header signature (little-endian: byte0=0x70, byte1=0x51). */
#define RS_OTA_FW_UPDATE_OVER_BLE_SUOTA_IMG_SIGNATURE   (0x5170U)

/* ---- bootloader back-end selection ----------------------------------------- *
 * Picks which implementation of the seam in rs_ota_fw_update_over_ble_boot.h is
 * compiled. Both back-ends live in src/hal and the managed build compiles every
 * .c file there, so each one is guarded by this switch and exactly one survives
 * the preprocessor.
 *
 *   1 = Renesas RA swap bootloader (rs_ota_fw_update_over_ble_boot_rsboot.c).
 *       Requires the companion ota_fw_bootloader in the first 16 KB of flash and
 *       this application linked at BOOT_PRIMARY_BASE.
 *   0 = no bootloader (rs_ota_fw_update_over_ble_boot_stub.c): the seam calls
 *       report RS_STATUS_ERR_UNSUPPORTED and nothing is installed.
 *
 * The CMake host-test build compiles only one back-end and defines this to match
 * its RS_OTA_FW_UPDATE_OVER_BLE_BOOT selection. */
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER
 #define RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER  (1)
#endif

/* Compile-time parameter checking (mirrors BSP_CFG_PARAM_CHECKING_ENABLE, §12/11). */
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_CFG_PARAM_CHECKING_ENABLE
 #define RS_OTA_FW_UPDATE_OVER_BLE_CFG_PARAM_CHECKING_ENABLE (1)
#endif

/** Validate that the RS dependencies (flash geometry, staging window) are
 *  self-consistent before a transfer starts. Returns true when the reference
 *  assumptions hold (§8.3 configuration check service). */
bool rs_ota_fw_update_over_ble_deps_check(void);

#ifdef __cplusplus
}
#endif

#endif /* RS_OTA_FW_UPDATE_OVER_BLE_DEPS_H */
