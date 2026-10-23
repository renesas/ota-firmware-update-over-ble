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
 * File Name: r_ble_RX_SERs.h
 * Version : 1.0
 * Description : The header file for RX service.
 **********************************************************************************************************************/
/***********************************************************************************************************************
 * History : DD.MM.YYYY Version Description
 *         : 31.12.2999 1.00 First Release
 ***********************************************************************************************************************/

/*******************************************************************************************************************//**
 * @file
 * @defgroup RX_SERs RX Service
 * @{
 * @ingroup profile
 * @brief   
 **********************************************************************************************************************/
#include "profile_cmn/r_ble_servs_if.h"
#include "gatt_db.h"

#ifndef R_BLE_RX_SERS_H
#define R_BLE_RX_SERS_H

/*----------------------------------------------------------------------------------------------------------------------
    RX DATA Characteristic
----------------------------------------------------------------------------------------------------------------------*/
/***************************************************************************//**
 * @brief RX DATA value structure.
*******************************************************************************/
typedef struct {
    uint8_t rx_data[20]; /**< rx_data */
} st_ble_rx_sers_rx_data_t;

/*----------------------------------------------------------------------------------------------------------------------
    RX Service
----------------------------------------------------------------------------------------------------------------------*/

/***************************************************************************//**
 * @brief RX characteristic Index.
*******************************************************************************/
typedef enum {
    BLE_RX_SERS_RX_DATA_IDX,
} e_ble_rx_sers_char_idx_t;

/***************************************************************************//**
 * @brief RX event type.
*******************************************************************************/
typedef enum {
    /* RX DATA */
    BLE_RX_SERS_EVENT_RX_DATA_WRITE_REQ = BLE_SERVS_ATTR_EVENT(BLE_RX_SERS_RX_DATA_IDX, BLE_SERVS_WRITE_REQ),
    BLE_RX_SERS_EVENT_RX_DATA_WRITE_COMP = BLE_SERVS_ATTR_EVENT(BLE_RX_SERS_RX_DATA_IDX, BLE_SERVS_WRITE_COMP),
    BLE_RX_SERS_EVENT_RX_DATA_WRITE_CMD = BLE_SERVS_ATTR_EVENT(BLE_RX_SERS_RX_DATA_IDX, BLE_SERVS_WRITE_CMD),
} e_ble_rx_sers_event_t;

/***************************************************************************//**
 * @brief     Initialize RX service.
 * @param[in] cb Service callback.
 * @return    @ref ble_status_t
*******************************************************************************/
ble_status_t R_BLE_RX_SERS_Init(ble_servs_app_cb_t cb);

#endif /* R_BLE_RX_SERS_H */

/** @} */
