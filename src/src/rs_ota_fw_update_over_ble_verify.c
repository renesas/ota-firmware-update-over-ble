/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_verify.c
 * Description  : SUOTA image-header parsing + CRC32 verification for the
 *                ota_fw_update_over_ble RS (see
 *                rs_ota_fw_update_over_ble_verify.h).
 *
 *                Target-agnostic: staged bytes are pulled through the RS HAL in
 *                small chunks, so no memory-mapped-flash assumption leaks in here.
 ******************************************************************************/
#include "rs_ota_fw_update_over_ble_verify.h"
#include "rs_ota_fw_update_over_ble_hal.h"
#include "rs_ota_fw_update_over_ble_deps.h"
#include <string.h>

#define STAGE_SIZE      RS_OTA_FW_UPDATE_OVER_BLE_CFG_STAGE_SIZE
#define SUOTA_SIGNATURE RS_OTA_FW_UPDATE_OVER_BLE_SUOTA_IMG_SIGNATURE

/* Read granularity for the CRC pass (stack buffer, no dynamic allocation). */
#define VERIFY_CHUNK    (64U)

/******************************************************************************
 * SUOTA single-image header (DA1453x, 64 bytes, little-endian). Only the fields
 * used for size/CRC verification are named; the rest are reserved.
 ******************************************************************************/
typedef struct
{
    uint16_t signature;    /* 0x00: 0x70,0x51 for an image header          */
    uint16_t flags;        /* 0x02: bit0 = valid/force                     */
    uint32_t code_size;    /* 0x04: firmware size in bytes (excl. header)  */
    uint32_t crc;          /* 0x08: CRC32 over code_size bytes of code     */
    uint8_t  version[16];  /* 0x0C: ASCII version string                   */
    uint32_t timestamp;    /* 0x1C: build epoch seconds                    */
    uint32_t code_offset;  /* 0x20: offset from header start to the code   */
} rs_ota_fw_update_over_ble_suota_hdr_t;

/******************************************************************************
 * Standard IEEE 802.3 (reflected) CRC32, computed on the fly (no lookup table,
 * to keep the footprint small). Call repeatedly to chain chunks; seed with
 * 0xFFFFFFFF and invert the final value.
 ******************************************************************************/
static uint32_t rs_ota_fw_update_over_ble_crc32_update(uint32_t crc, const uint8_t * p, uint32_t len)
{
    for (uint32_t i = 0U; i < len; i++)
    {
        crc ^= p[i];
        for (int b = 0; b < 8; b++)
        {
            uint32_t mask = (uint32_t) -(int32_t) (crc & 1U);
            crc = (crc >> 1) ^ (0xEDB88320U & mask);
        }
    }
    return crc;
}

rs_ota_fw_update_over_ble_verify_t rs_ota_fw_update_over_ble_verify(uint32_t * p_code_size)
{
    rs_ota_fw_update_over_ble_suota_hdr_t hdr;

    if (NULL != p_code_size)
    {
        *p_code_size = 0U;
    }

    uint32_t stored = rs_ota_fw_update_over_ble_hal_stage_written();

    if (RS_STATUS_SUCCESS != rs_ota_fw_update_over_ble_hal_stage_read(0U, &hdr, sizeof(hdr)))
    {
        return RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_HEADER;
    }

    /* No SUOTA header: this is the normal case for the flat, headerless *_ota.bin
     * images produced by the build (the bootloader expects the vector table at
     * offset 0). There is no stored size/CRC to check against, so skip
     * verification and report OK. Integrity then relies on the bootloader
     * (vector-table plausibility check + swap CRC, trial boot with rollback). */
    if (SUOTA_SIGNATURE != hdr.signature)
    {
        rs_ota_fw_update_over_ble_hal_log(
            "[FWACQ] verify: no SUOTA header (sig=0x%04X) - skipping CRC\r\n",
            (unsigned) hdr.signature);
        return RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_OK;
    }

    uint32_t code_off  = hdr.code_offset;
    uint32_t code_size = hdr.code_size;

    if (NULL != p_code_size)
    {
        *p_code_size = code_size;
    }

    /* Sanity-check the advertised geometry against what we actually stored. */
    if ((0U == code_size) ||
        ((code_off + code_size) > stored) ||
        ((code_off + code_size) > STAGE_SIZE))
    {
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_ASSERTION,
            "[FWACQ] verify: bad header (off=%u size=%u stored=%u)\r\n",
            (unsigned) code_off, (unsigned) code_size, (unsigned) stored);
        return RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_HEADER;
    }

    uint32_t crc       = 0xFFFFFFFFU;
    uint32_t remaining = code_size;
    uint32_t offset    = code_off;
    uint8_t  chunk[VERIFY_CHUNK];

    while (remaining > 0U)
    {
        uint32_t n = (remaining < VERIFY_CHUNK) ? remaining : VERIFY_CHUNK;

        if (RS_STATUS_SUCCESS != rs_ota_fw_update_over_ble_hal_stage_read(offset, chunk, n))
        {
            return RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_HEADER;
        }

        crc        = rs_ota_fw_update_over_ble_crc32_update(crc, chunk, n);
        offset    += n;
        remaining -= n;
    }
    crc = ~crc;

    rs_ota_fw_update_over_ble_hal_logs(
        (crc == hdr.crc) ? RS_STATUS_SUCCESS : RS_STATUS_ERR_ASSERTION,
        "[FWACQ] verify: code=%u bytes crc_hdr=0x%08X crc_calc=0x%08X -> %s\r\n",
        (unsigned) code_size, (unsigned) hdr.crc, (unsigned) crc,
        (crc == hdr.crc) ? "OK" : "MISMATCH");

    return (crc == hdr.crc) ? RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_OK
                            : RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_CRC;
}