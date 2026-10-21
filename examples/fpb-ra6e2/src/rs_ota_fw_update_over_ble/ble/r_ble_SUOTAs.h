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
 * File Name: r_ble_SUOTAs.h
 * Version : 1.0
 * Description : The header file for SUOTA service.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * History : DD.MM.YYYY Version Description
 *         : 31.12.2999 1.00 First Release
 ***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @file
 * @defgroup SUOTAs SUOTA Service
 * @{
 * @ingroup profile
 * @brief   
 **********************************************************************************************************************/
#include "profile_cmn/r_ble_servs_if.h"
#include "gatt_db.h"

#ifndef R_BLE_SUOTAS_H
#define R_BLE_SUOTAS_H

/*----------------------------------------------------------------------------------------------------------------------
    Memory Device Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set Memory Device characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetMem_dev(const uint32_t *p_value);

/***************************************************************************//**
 * @brief     Get Memory Device characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetMem_dev(uint32_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    Memory Info Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set Memory Info characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetMem_info(const uint32_t *p_value);

/***************************************************************************//**
 * @brief     Get Memory Info characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetMem_info(uint32_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    Patch Length Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set Patch Length characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetPatch_len(const uint16_t *p_value);

/***************************************************************************//**
 * @brief     Get Patch Length characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetPatch_len(uint16_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    Pach Data Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief Pach Data value structure.
*******************************************************************************/
typedef struct {
    uint32_t patch_data[80]; /**< patch_data */
} st_ble_suotas_patch_data_t;

/*----------------------------------------------------------------------------------------------------------------------
    Service Status Characteristic
        Client Characteristic Configuration descriptor : Client Characteristic Configuration Descriptor
----------------------------------------------------------------------------------------------------------------------*/

/***************************************************************************//**
 * @brief     Set Service Status characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetServ_status(const uint8_t *p_value);

/***************************************************************************//**
 * @brief     Get Service Status characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetServ_status(uint8_t *p_value);

/***************************************************************************//**
 * @brief     Send notification of  Service Status characteristic value to the remote device.
 * @param[in] conn_hdl Connection handle.
 * @param[in] p_value  Characteristic value to send.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_NotifyServ_status(uint16_t conn_hdl, const uint8_t *p_value);

/***************************************************************************//**
 * @brief     Set Service Status CCCD descriptor value to the local GATT database.
 * @param[in] conn_hdl Connection handle.
 * @param[in] p_value  Descriptor value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetServ_statusCccd(uint16_t conn_hdl, const uint16_t *p_value);

/***************************************************************************//**
 * @brief     Get Service Status CCCD descriptor value from the local GATT database.
 * @param[in] conn_hdl Connection handle.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetServ_statusCccd(uint16_t conn_hdl, uint16_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    GPIO MAP Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set GPIO MAP characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetGpio_map(const uint32_t *p_value);

/***************************************************************************//**
 * @brief     Get GPIO MAP characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetGpio_map(uint32_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_VERSION Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set SUOTA_VERSION characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetSuota_version(const uint8_t *p_value);

/***************************************************************************//**
 * @brief     Get SUOTA_VERSION characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetSuota_version(uint8_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_PATCH_DATA_CHAR_SIZE Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set SUOTA_PATCH_DATA_CHAR_SIZE characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetSuota_patch_data_char_size(const uint16_t *p_value);

/***************************************************************************//**
 * @brief     Get SUOTA_PATCH_DATA_CHAR_SIZE characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetSuota_patch_data_char_size(uint16_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_MTU Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set SUOTA_MTU characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetSuota_mtu(const uint16_t *p_value);

/***************************************************************************//**
 * @brief     Get SUOTA_MTU characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetSuota_mtu(uint16_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA_CAP_L2CAP_PSM Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief     Set SUOTA_CAP_L2CAP_PSM characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_SetSuota_cap_l2cap_psm(const uint16_t *p_value);

/***************************************************************************//**
 * @brief     Get SUOTA_CAP_L2CAP_PSM characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_GetSuota_cap_l2cap_psm(uint16_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    SUOTA Service
----------------------------------------------------------------------------------------------------------------------*/

/***************************************************************************//**
 * @brief SUOTA characteristic Index.
*******************************************************************************/
typedef enum {
    BLE_SUOTAS_MEM_DEV_IDX,
    BLE_SUOTAS_MEM_INFO_IDX,
    BLE_SUOTAS_PATCH_LEN_IDX,
    BLE_SUOTAS_PATCH_DATA_IDX,
    BLE_SUOTAS_SERV_STATUS_IDX,
    BLE_SUOTAS_SERV_STATUS_CCCD_IDX,
    BLE_SUOTAS_GPIO_MAP_IDX,
    BLE_SUOTAS_SUOTA_VERSION_IDX,
    BLE_SUOTAS_SUOTA_PATCH_DATA_CHAR_SIZE_IDX,
    BLE_SUOTAS_SUOTA_MTU_IDX,
    BLE_SUOTAS_SUOTA_CAP_L2CAP_PSM_IDX,
} e_ble_suotas_char_idx_t;

/***************************************************************************//**
 * @brief SUOTA event type.
*******************************************************************************/
typedef enum {
    /* Memory Device */
    BLE_SUOTAS_EVENT_MEM_DEV_WRITE_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_MEM_DEV_IDX, BLE_SERVS_WRITE_REQ),
    BLE_SUOTAS_EVENT_MEM_DEV_WRITE_COMP = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_MEM_DEV_IDX, BLE_SERVS_WRITE_COMP),
    BLE_SUOTAS_EVENT_MEM_DEV_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_MEM_DEV_IDX, BLE_SERVS_READ_REQ),
    /* Memory Info */
    BLE_SUOTAS_EVENT_MEM_INFO_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_MEM_INFO_IDX, BLE_SERVS_READ_REQ),
    /* Patch Length */
    BLE_SUOTAS_EVENT_PATCH_LEN_WRITE_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_PATCH_LEN_IDX, BLE_SERVS_WRITE_REQ),
    BLE_SUOTAS_EVENT_PATCH_LEN_WRITE_COMP = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_PATCH_LEN_IDX, BLE_SERVS_WRITE_COMP),
    BLE_SUOTAS_EVENT_PATCH_LEN_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_PATCH_LEN_IDX, BLE_SERVS_READ_REQ),
    /* Pach Data */
    BLE_SUOTAS_EVENT_PATCH_DATA_WRITE_CMD = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_PATCH_DATA_IDX, BLE_SERVS_WRITE_CMD),
    /* Service Status */
    BLE_SUOTAS_EVENT_SERV_STATUS_WRITE_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SERV_STATUS_IDX, BLE_SERVS_WRITE_REQ),
    BLE_SUOTAS_EVENT_SERV_STATUS_WRITE_COMP = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SERV_STATUS_IDX, BLE_SERVS_WRITE_COMP),
    BLE_SUOTAS_EVENT_SERV_STATUS_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SERV_STATUS_IDX, BLE_SERVS_READ_REQ),
    BLE_SUOTAS_EVENT_SERV_STATUS_CCCD_WRITE_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SERV_STATUS_CCCD_IDX, BLE_SERVS_WRITE_REQ),
    BLE_SUOTAS_EVENT_SERV_STATUS_CCCD_WRITE_COMP = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SERV_STATUS_CCCD_IDX, BLE_SERVS_WRITE_COMP),
    BLE_SUOTAS_EVENT_SERV_STATUS_CCCD_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SERV_STATUS_CCCD_IDX, BLE_SERVS_READ_REQ),
    /* GPIO MAP */
    BLE_SUOTAS_EVENT_GPIO_MAP_WRITE_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_GPIO_MAP_IDX, BLE_SERVS_WRITE_REQ),
    BLE_SUOTAS_EVENT_GPIO_MAP_WRITE_COMP = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_GPIO_MAP_IDX, BLE_SERVS_WRITE_COMP),
    BLE_SUOTAS_EVENT_GPIO_MAP_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_GPIO_MAP_IDX, BLE_SERVS_READ_REQ),
    /* SUOTA_VERSION */
    BLE_SUOTAS_EVENT_SUOTA_VERSION_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SUOTA_VERSION_IDX, BLE_SERVS_READ_REQ),
    /* SUOTA_PATCH_DATA_CHAR_SIZE */
    BLE_SUOTAS_EVENT_SUOTA_PATCH_DATA_CHAR_SIZE_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SUOTA_PATCH_DATA_CHAR_SIZE_IDX, BLE_SERVS_READ_REQ),
    /* SUOTA_MTU */
    BLE_SUOTAS_EVENT_SUOTA_MTU_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SUOTA_MTU_IDX, BLE_SERVS_READ_REQ),
    /* SUOTA_CAP_L2CAP_PSM */
    BLE_SUOTAS_EVENT_SUOTA_CAP_L2CAP_PSM_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_SUOTA_CAP_L2CAP_PSM_IDX, BLE_SERVS_READ_REQ),
} e_ble_suotas_event_t;

/***************************************************************************//**
 * @brief     Initialize SUOTA service.
 * @param[in] cb Service callback.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_SUOTAS_Init(ble_servs_app_cb_t cb);

#endif /* R_BLE_SUOTAS_H */

/** @} */
