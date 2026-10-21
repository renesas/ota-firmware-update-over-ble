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
 * File Name: r_ble_ia2s.h
 * Version : 1.0
 * Description : The header file for RA6E2 IO Control service.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * History : DD.MM.YYYY Version Description
 *         : 31.12.2999 1.00 First Release
 ***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @file
 * @defgroup ia2s RA6E2 IO Control Service
 * @{
 * @ingroup profile
 * @brief   This service exposes a control point to allow a peer device to cause the device to immediately alert.
 **********************************************************************************************************************/
#include "profile_cmn/r_ble_servs_if.h"
#include "gatt_db.h"

#ifndef R_BLE_IA2S_H
#define R_BLE_IA2S_H

/*----------------------------------------------------------------------------------------------------------------------
    Led Control Characteristic : The level of an alert a device is to sound. If this level is changed while the alert is being sounded,the new level should take effect.
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief Led Control Level enumeration.
*******************************************************************************/
typedef enum {
    BLE_IA2S_LEDCTRL_LEVEL_NO_ALERT = 0, /**< No Alert */
    BLE_IA2S_LEDCTRL_LEVEL_MILD_ALERT = 1, /**< Mild Alert */
    BLE_IA2S_LEDCTRL_LEVEL_HIGH_ALERT = 2, /**< High Alert */
} e_ble_ia2s_ledctrl_level_t;

/***************************************************************************//**
 * @brief     Set Led Control characteristic value to the local GATT database.
 * @param[in] p_value  Characteristic value to set.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_IA2S_SetLedctrl(const uint8_t *p_value);

/***************************************************************************//**
 * @brief     Get Led Control characteristic value from the local GATT database.
 * @param[out] p_value  Output location for the acquired descriptor value.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_IA2S_GetLedctrl(uint8_t *p_value);

/*----------------------------------------------------------------------------------------------------------------------
    RA6E2 IO Control Service
----------------------------------------------------------------------------------------------------------------------*/

/***************************************************************************//**
 * @brief RA6E2 IO Control characteristic Index.
*******************************************************************************/
typedef enum {
    BLE_IA2S_LEDCTRL_IDX,
} e_ble_ia2s_char_idx_t;

/***************************************************************************//**
 * @brief RA6E2 IO Control event type.
*******************************************************************************/
typedef enum {
    /* Led Control */
    BLE_IA2S_EVENT_LEDCTRL_WRITE_REQ = BLE_SERVS_ATTR_EVENT(BLE_IA2S_LEDCTRL_IDX, BLE_SERVS_WRITE_REQ),
    BLE_IA2S_EVENT_LEDCTRL_WRITE_COMP = BLE_SERVS_ATTR_EVENT(BLE_IA2S_LEDCTRL_IDX, BLE_SERVS_WRITE_COMP),
    BLE_IA2S_EVENT_LEDCTRL_WRITE_CMD = BLE_SERVS_ATTR_EVENT(BLE_IA2S_LEDCTRL_IDX, BLE_SERVS_WRITE_CMD),
    BLE_IA2S_EVENT_LEDCTRL_READ_REQ = BLE_SERVS_ATTR_EVENT(BLE_IA2S_LEDCTRL_IDX, BLE_SERVS_READ_REQ),
} e_ble_ia2s_event_t;

/***************************************************************************//**
 * @brief     Initialize RA6E2 IO Control service.
 * @param[in] cb Service callback.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_IA2S_Init(ble_servs_app_cb_t cb);

#endif /* R_BLE_IA2S_H */

/** @} */
