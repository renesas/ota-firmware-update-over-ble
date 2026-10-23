/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_verify.h
 * Description  : Internal helper for the ota_fw_update_over_ble RS.
 *                Parses the SUOTA single-image header at the start of the
 *                staging area and (best effort) verifies the CRC32 of the code
 *                it advertises.
 *
 *                Target-agnostic: staged bytes are read through the RS HAL, so
 *                this module builds and runs on a host.
 *
 *                RS-internal helper — not part of the public RS API. The
 *                public status enum lives in rs_ota_fw_update_over_ble.h;
 *                the orchestrator maps this result onto it.
 ******************************************************************************/
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_H
#define RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Result of rs_ota_fw_update_over_ble_verify(): maps to the SUOTA SERV_STATUS
 *  codes the server must notify at END. */
typedef enum e_rs_ota_fw_update_over_ble_verify
{
    RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_OK         = 0, /* header parsed and CRC matched (or non-SUOTA image) */
    RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_HEADER = 1, /* SUOTA signature present but header inconsistent    */
    RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_BAD_CRC    = 2, /* CRC32 over the code did not match the header value */
} rs_ota_fw_update_over_ble_verify_t;

/** Parse the SUOTA image header at the start of the staging area and (best effort)
 *  verify the CRC32 of the code it advertises. Call after the staging store has
 *  flushed the last page (i.e. when the client issues END, 0xFE000000).
 *
 *  - If the first bytes do NOT carry a SUOTA signature (0x70 0x51) the image is
 *    treated as an opaque staging blob and ..._VERIFY_OK is returned (so plain
 *    test images still complete the handshake).
 *  - If the signature is present, code_size / code_offset / crc are read from the
 *    header, a CRC32 is computed over the code and compared with the header value.
 *
 *  @param[out] p_code_size  Optional; receives the advertised code size (0 if N/A).
 *  @return OK / BAD_HEADER / BAD_CRC. */
rs_ota_fw_update_over_ble_verify_t rs_ota_fw_update_over_ble_verify(uint32_t * p_code_size);

#ifdef __cplusplus
}
#endif

#endif /* RS_OTA_FW_UPDATE_OVER_BLE_VERIFY_H */