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
 * File Name: r_ble_SUOTAs.c
 * Version : 1.0
 * Description : The source file for SUOTA service.
 **********************************************************************************************************************/

#include "r_ble_SUOTAs.h"
#include "profile_cmn/r_ble_servs_if.h"
#include "gatt_db.h"

static st_ble_servs_info_t gs_servs_info;

/*----------------------------------------------------------------------------------------------------------------------
    Memory Device characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* Memory Device characteristic definition */
static const st_ble_servs_char_info_t gs_mem_dev_char = {
    .start_hdl    = BLE_SUOTAS_MEM_DEV_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_MEM_DEV_VAL_HDL,
    .char_idx     = BLE_SUOTAS_MEM_DEV_IDX,
    .app_size     = sizeof(uint32_t),
    .db_size      = BLE_SUOTAS_MEM_DEV_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint32_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint32_t,
};

ble_status_t R_BLE_SUOTAS_SetMem_dev(const uint32_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_mem_dev_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetMem_dev(uint32_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_mem_dev_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    Memory Info characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* Memory Info characteristic definition */
static const st_ble_servs_char_info_t gs_mem_info_char = {
    .start_hdl    = BLE_SUOTAS_MEM_INFO_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_MEM_INFO_VAL_HDL,
    .char_idx     = BLE_SUOTAS_MEM_INFO_IDX,
    .app_size     = sizeof(uint32_t),
    .db_size      = BLE_SUOTAS_MEM_INFO_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint32_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint32_t,
};

ble_status_t R_BLE_SUOTAS_SetMem_info(const uint32_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_mem_info_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetMem_info(uint32_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_mem_info_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    Patch Length characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* Patch Length characteristic definition */
static const st_ble_servs_char_info_t gs_patch_len_char = {
    .start_hdl    = BLE_SUOTAS_PATCH_LEN_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_PATCH_LEN_VAL_HDL,
    .char_idx     = BLE_SUOTAS_PATCH_LEN_IDX,
    .app_size     = sizeof(uint16_t),
    .db_size      = BLE_SUOTAS_PATCH_LEN_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint16_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint16_t,
};

ble_status_t R_BLE_SUOTAS_SetPatch_len(const uint16_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_patch_len_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetPatch_len(uint16_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_patch_len_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    Pach Data characteristic
----------------------------------------------------------------------------------------------------------------------*/

static ble_status_t decode_st_ble_suotas_patch_data_t(st_ble_suotas_patch_data_t *p_app_value, const st_ble_gatt_value_t *p_gatt_value)
{
    /* Start user code for Pach Data characteristic value decode function. Do not edit comment generated here */
    /* The QE framework allocates a fixed-size (sizeof struct) app buffer and relies on
     * this decode function to copy the received GATT write payload into it. The default
     * generated stub did nothing, so the application callback received uninitialised
     * heap memory ("junk") together with a misleading fixed length (sizeof struct).
     *
     * Copy the actual received bytes, zero-pad the remainder, and publish the true
     * received length so the application can log/process exactly what the client sent. */
    extern volatile uint16_t g_suota_last_patch_len;

    uint16_t len = (NULL != p_gatt_value) ? p_gatt_value->value_len : 0U;
    if (len > sizeof(p_app_value->patch_data))
    {
        len = (uint16_t)sizeof(p_app_value->patch_data);
    }

    uint8_t *p_dst = (uint8_t *)p_app_value->patch_data;
    for (uint16_t i = 0; i < (uint16_t)sizeof(p_app_value->patch_data); i++)
    {
        p_dst[i] = ((i < len) && (NULL != p_gatt_value->p_value)) ? p_gatt_value->p_value[i] : 0U;
    }

    g_suota_last_patch_len = len;
    /* End user code. Do not edit comment generated here */
    return BLE_SUCCESS;
}

static ble_status_t encode_st_ble_suotas_patch_data_t(const st_ble_suotas_patch_data_t *p_app_value, st_ble_gatt_value_t *p_gatt_value)
{
    /* Start user code for Pach Data characteristic value encode function. Do not edit comment generated here */
    FSP_PARAMETER_NOT_USED(p_app_value);
    FSP_PARAMETER_NOT_USED(p_gatt_value);
    /* End user code. Do not edit comment generated here */
    return BLE_SUCCESS;
}

/* Pach Data characteristic definition */
static const st_ble_servs_char_info_t gs_patch_data_char = {
    .start_hdl    = BLE_SUOTAS_PATCH_DATA_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_PATCH_DATA_VAL_HDL,
    .char_idx     = BLE_SUOTAS_PATCH_DATA_IDX,
    .app_size     = sizeof(st_ble_suotas_patch_data_t),
    .db_size      = BLE_SUOTAS_PATCH_DATA_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_st_ble_suotas_patch_data_t,
    .encode       = (ble_servs_attr_encode_t)encode_st_ble_suotas_patch_data_t,
};

/*----------------------------------------------------------------------------------------------------------------------
    Service Status Client Characteristic Configuration descriptor : Client Characteristic Configuration Descriptor
----------------------------------------------------------------------------------------------------------------------*/

static const st_ble_servs_desc_info_t gs_serv_status_cccd = {
    .attr_hdl = BLE_SUOTAS_SERV_STATUS_CCCD_DESC_HDL,
    .app_size = sizeof(uint16_t),
    .desc_idx = BLE_SUOTAS_SERV_STATUS_CCCD_IDX,
    .db_size  = BLE_SUOTAS_SERV_STATUS_CCCD_LEN,
    .decode   = (ble_servs_attr_decode_t)decode_uint16_t,
    .encode   = (ble_servs_attr_encode_t)encode_uint16_t,
};

ble_status_t R_BLE_SUOTAS_SetServ_statusCccd(uint16_t conn_hdl, const uint16_t *p_value)
{
    return R_BLE_SERVS_SetDesc(&gs_serv_status_cccd, conn_hdl, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetServ_statusCccd(uint16_t conn_hdl, uint16_t *p_value)
{
    return R_BLE_SERVS_GetDesc(&gs_serv_status_cccd, conn_hdl, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    Service Status characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* Service Status characteristic descriptor definition */
static const st_ble_servs_desc_info_t *gspp_serv_status_descs[] = { 
    &gs_serv_status_cccd,
};

/* Service Status characteristic definition */
static const st_ble_servs_char_info_t gs_serv_status_char = {
    .start_hdl    = BLE_SUOTAS_SERV_STATUS_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_SERV_STATUS_CCCD_DESC_HDL,
    .char_idx     = BLE_SUOTAS_SERV_STATUS_IDX,
    .app_size     = sizeof(uint8_t),
    .db_size      = BLE_SUOTAS_SERV_STATUS_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint8_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint8_t,
    .pp_descs     = gspp_serv_status_descs,
    .num_of_descs = ARRAY_SIZE(gspp_serv_status_descs),
};

ble_status_t R_BLE_SUOTAS_SetServ_status(const uint8_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_serv_status_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetServ_status(uint8_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_serv_status_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

ble_status_t R_BLE_SUOTAS_NotifyServ_status(uint16_t conn_hdl, const uint8_t *p_value)
{
    return R_BLE_SERVS_SendHdlVal(&gs_serv_status_char, conn_hdl, (const void *)p_value, true);
}

/*----------------------------------------------------------------------------------------------------------------------
    GPIO MAP characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* GPIO MAP characteristic definition */
static const st_ble_servs_char_info_t gs_gpio_map_char = {
    .start_hdl    = BLE_SUOTAS_GPIO_MAP_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_GPIO_MAP_VAL_HDL,
    .char_idx     = BLE_SUOTAS_GPIO_MAP_IDX,
    .app_size     = sizeof(uint32_t),
    .db_size      = BLE_SUOTAS_GPIO_MAP_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint32_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint32_t,
};

ble_status_t R_BLE_SUOTAS_SetGpio_map(const uint32_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_gpio_map_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetGpio_map(uint32_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_gpio_map_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_VERSION characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* SUOTA_VERSION characteristic definition */
static const st_ble_servs_char_info_t gs_suota_version_char = {
    .start_hdl    = BLE_SUOTAS_SUOTA_VERSION_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_SUOTA_VERSION_VAL_HDL,
    .char_idx     = BLE_SUOTAS_SUOTA_VERSION_IDX,
    .app_size     = sizeof(uint8_t),
    .db_size      = BLE_SUOTAS_SUOTA_VERSION_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint8_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint8_t,
};

ble_status_t R_BLE_SUOTAS_SetSuota_version(const uint8_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_suota_version_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetSuota_version(uint8_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_suota_version_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_PATCH_DATA_CHAR_SIZE characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* SUOTA_PATCH_DATA_CHAR_SIZE characteristic definition */
static const st_ble_servs_char_info_t gs_suota_patch_data_char_size_char = {
    .start_hdl    = BLE_SUOTAS_SUOTA_PATCH_DATA_CHAR_SIZE_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_SUOTA_PATCH_DATA_CHAR_SIZE_VAL_HDL,
    .char_idx     = BLE_SUOTAS_SUOTA_PATCH_DATA_CHAR_SIZE_IDX,
    .app_size     = sizeof(uint16_t),
    .db_size      = BLE_SUOTAS_SUOTA_PATCH_DATA_CHAR_SIZE_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint16_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint16_t,
};

ble_status_t R_BLE_SUOTAS_SetSuota_patch_data_char_size(const uint16_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_suota_patch_data_char_size_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetSuota_patch_data_char_size(uint16_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_suota_patch_data_char_size_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_MTU characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* SUOTA_MTU characteristic definition */
static const st_ble_servs_char_info_t gs_suota_mtu_char = {
    .start_hdl    = BLE_SUOTAS_SUOTA_MTU_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_SUOTA_MTU_VAL_HDL,
    .char_idx     = BLE_SUOTAS_SUOTA_MTU_IDX,
    .app_size     = sizeof(uint16_t),
    .db_size      = BLE_SUOTAS_SUOTA_MTU_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint16_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint16_t,
};

ble_status_t R_BLE_SUOTAS_SetSuota_mtu(const uint16_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_suota_mtu_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetSuota_mtu(uint16_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_suota_mtu_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_CAP_L2CAP_PSM characteristic
----------------------------------------------------------------------------------------------------------------------*/

/* SUOTA_CAP_L2CAP_PSM characteristic definition */
static const st_ble_servs_char_info_t gs_suota_cap_l2cap_psm_char = {
    .start_hdl    = BLE_SUOTAS_SUOTA_CAP_L2CAP_PSM_DECL_HDL,
    .end_hdl      = BLE_SUOTAS_SUOTA_CAP_L2CAP_PSM_VAL_HDL,
    .char_idx     = BLE_SUOTAS_SUOTA_CAP_L2CAP_PSM_IDX,
    .app_size     = sizeof(uint16_t),
    .db_size      = BLE_SUOTAS_SUOTA_CAP_L2CAP_PSM_LEN,
    .decode       = (ble_servs_attr_decode_t)decode_uint16_t,
    .encode       = (ble_servs_attr_encode_t)encode_uint16_t,
};

ble_status_t R_BLE_SUOTAS_SetSuota_cap_l2cap_psm(const uint16_t *p_value)
{
    return R_BLE_SERVS_SetChar(&gs_suota_cap_l2cap_psm_char, BLE_GAP_INVALID_CONN_HDL, (const void *)p_value);
}

ble_status_t R_BLE_SUOTAS_GetSuota_cap_l2cap_psm(uint16_t *p_value)
{
    return R_BLE_SERVS_GetChar(&gs_suota_cap_l2cap_psm_char, BLE_GAP_INVALID_CONN_HDL, (void *)p_value);
}

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA server
----------------------------------------------------------------------------------------------------------------------*/

/* SUOTA characteristics definition */
static const st_ble_servs_char_info_t *gspp_chars[] = {
    &gs_mem_dev_char,
    &gs_mem_info_char,
    &gs_patch_len_char,
    &gs_patch_data_char,
    &gs_serv_status_char,
    &gs_gpio_map_char,
    &gs_suota_version_char,
    &gs_suota_patch_data_char_size_char,
    &gs_suota_mtu_char,
    &gs_suota_cap_l2cap_psm_char,
};

/* SUOTA service definition */
static st_ble_servs_info_t gs_servs_info = {
    .pp_chars     = gspp_chars,
    .num_of_chars = ARRAY_SIZE(gspp_chars),
};

ble_status_t R_BLE_SUOTAS_Init(ble_servs_app_cb_t cb)
{
    if (NULL == cb)
    {
        return BLE_ERR_INVALID_PTR;
    }

    gs_servs_info.cb = cb;

    return R_BLE_SERVS_RegisterServer(&gs_servs_info);
}
