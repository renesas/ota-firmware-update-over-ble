#include "ble_thread.h"
/* BLE Thread entry function */
/* pvParameters contains TaskHandle_t */

/* Reference-application entry lives in src/app/, separate from the files
 * FSP/QE regenerate. This FSP-generated stub only delegates so the authored
 * demo logic survives QE/FSP regeneration. */
extern void app_ota_fw_update_over_ble_entry(void);

void ble_thread_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    app_ota_fw_update_over_ble_entry();
}
