/******************************************************************************
 * File Name    : ota_fw_update_over_ble_app.h
 * Description  : Declaration of the reference/demo application entry for the
 *                ota_fw_update_over_ble RS. Called by the FSP-generated
 *                BLE thread entry (src/ble_thread_entry.c).
 ******************************************************************************/
#ifndef OTA_FW_UPDATE_OVER_BLE_APP_H
#define OTA_FW_UPDATE_OVER_BLE_APP_H

#ifdef __cplusplus
extern "C" {
#endif

/** Reference-application entry: configures the debug log and hands over to the
 *  RS public API (rs_ota_fw_update_over_ble_Start), which brings up the
 *  BLE/SUOTA transport and runs its event loop in this thread.
 *
 *  It also owns the INSTALL decision, which the RS deliberately leaves to the
 *  application:
 *    - on start-up it asks BootResultGet() what the bootloader did, and confirms
 *      the running image if it was installed on trial and passes a health gate;
 *    - when a download completes it arms a trial swap and resets.
 *
 *  Never returns. */
void app_ota_fw_update_over_ble_entry(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_FW_UPDATE_OVER_BLE_APP_H */
