/***********************************************************************************************************************
* DISCLAIMER
* This software is supplied by Renesas Electronics Corporation and is only intended for use with Renesas products. No
* other uses are authorized. This software is owned by Renesas Electronics Corporation and is protected under all
* applicable laws, including copyright laws.
* THIS SOFTWARE IS PROVIDED "AS IS" AND RENESAS MAKES NO WARRANTIES REGARDING
* THIS SOFTWARE, WHETHER EXPRESS, IMPLIED OR STATUTORY, INCLUDING BUT NOT LIMITED TO WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. ALL SUCH WARRANTIES ARE EXPRESSLY DISCLAIMED. TO THE MAXIMUM
* EXTENT PERMITTED NOT PROHIBITED BY LAW, NEITHER RENESAS ELECTRONICS CORPORATION NOR ANY OF ITS AFFILIATED COMPANIES
* SHALL BE LIABLE FOR ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES FOR ANY REASON RELATED TO THIS
* SOFTWARE, EVEN IF RENESAS OR ITS AFFILIATES HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
* Renesas reserves the right, without notice, to make changes to this software and to discontinue the availability of
* this software. By using this software, you agree to the additional terms and conditions found by accessing the
* following link:
* http://www.renesas.com/disclaimer
*
* Copyright (C) 2019-2020 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/
/***********************************************************************************************************************
 * File Name: r_ble_RX_SERs.c
 * Version : 1.0
 * Description : The source file for RX service.
 **********************************************************************************************************************/

#include "r_ble_RX_SERs.h"
#include "profile_cmn/r_ble_servs_if.h"
#include "gatt_db.h"

static st_ble_servs_info_t gs_servs_info;

/*----------------------------------------------------------------------------------------------------------------------
    RX DATA characteristic
----------------------------------------------------------------------------------------------------------------------*/

static ble_status_t decode_st_ble_rx_sers_rx_data_t(st_ble_rx_sers_rx_data_t *p_app_value, const st_ble_gatt_value_t *p_gatt_value)
{
    /* Start user code for RX DATA characteristic value decode function. Do not edit comment generated here */
    /* Deliberately silent.
     *
     * This used to dump every received byte straight to the debug channel. That
     * was removed: it ran per byte on the BLE thread, and it wrote the transport
     * directly instead of reporting through the RS sink - so it could neither be
     * compiled out nor inherit the sink's non-blocking guarantee. All RS trace
     * output goes through rs_ota_fw_update_over_ble_hal_log(), which hands the
     * finished line to the application callback; the application alone decides
     * where it is written. Nothing here writes a UART either (SCI0 carries the
     * DA14531 GTL link).
     *
     * This service is not part of the SUOTA transfer path - the image arrives
     * through r_ble_SUOTAs.c - so there is nothing to decode here. */
    (void) p_app_value;
    (void) p_gatt_value;
    /* End user code. Do not edit comment generated here */
    return BLE_SUCCESS;
}

static ble_status_t encode_st_ble_rx_sers_rx_data_t(const st_ble_rx_sers_rx_data_t *p_app_value, st_ble_gatt_value_t *p_gatt_value)
{
    /* Start user code for RX DATA characteristic value encode function. Do not edit comment generated here */
    (void) p_app_value;
    (void) p_gatt_value;
    /* End user code. Do not edit comment generated here */
    return BLE_SUCCESS;
}

/* RX DATA characteristic definition */
static const st_ble_servs_char_info_t gs_rx_data_char = {
    .start_hdl    = BLE_RX_SERS_RX_DATA_DECL_HDL,
    .end_hdl      = BLE_RX_SERS_RX_DATA_VAL_HDL,
    .char_idx     = BLE_RX_SERS_RX_DATA_IDX,
    .app_size     = sizeof(st_ble_rx_sers_rx_data_t),
    .db_size      = BLE_RX_SERS_RX_DATA_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_st_ble_rx_sers_rx_data_t,
    .encode       = (ble_servs_attr_encode_t)encode_st_ble_rx_sers_rx_data_t,
};

/*----------------------------------------------------------------------------------------------------------------------
    RX server
----------------------------------------------------------------------------------------------------------------------*/

/* RX characteristics definition */
static const st_ble_servs_char_info_t *gspp_chars[] = {
    &gs_rx_data_char,
};

/* RX service definition */
static st_ble_servs_info_t gs_servs_info = {
    .pp_chars     = gspp_chars,
    .num_of_chars = ARRAY_SIZE(gspp_chars),
};

ble_status_t R_BLE_RX_SERS_Init(ble_servs_app_cb_t cb)
{
    if (NULL == cb)
    {
        return BLE_ERR_INVALID_PTR;
    }

    gs_servs_info.cb = cb;

    return R_BLE_SERVS_RegisterServer(&gs_servs_info);
}
