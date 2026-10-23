/******************************************************************************
 * File Name    : rs_ota_fw_update_over_ble_board_ra6e2.h
 * Description  : Integrator-supplied board hooks for the DA14531 transport of
 *                the ota_fw_update_over_ble RS.
 *
 *                These are the board/wiring-specific callbacks the HAL needs but
 *                that depend on the customer's schematic (UART instance for the
 *                GTL link, DA14531 hardware reset line, power enable). The
 *                reference implementation for the RA6E2 EVZ wires these to the
 *                FSP-generated instances in the demo application.
 *
 *                This is a documented adaptation point (§8, §9.2): a customer
 *                porting to another board reimplements these.
 ******************************************************************************/
#ifndef RS_OTA_FW_UPDATE_OVER_BLE_BOARD_RA6E2_H
#define RS_OTA_FW_UPDATE_OVER_BLE_BOARD_RA6E2_H

#include "bsp_api.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Assert/deassert the DA14531 hardware reset line.
 *  @param[in] asserted  true = hold in reset, false = release. */
void rs_ota_fw_update_over_ble_board_reset(bool asserted);

/** Enable/disable power to the DA14531 module (no-op if always powered). */
void rs_ota_fw_update_over_ble_board_power(bool on);

#ifdef __cplusplus
}
#endif

#endif /* RS_OTA_FW_UPDATE_OVER_BLE_BOARD_RA6E2_H */