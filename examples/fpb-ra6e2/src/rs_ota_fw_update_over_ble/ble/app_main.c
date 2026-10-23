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
* Copyright (C) 2019-2025 Renesas Electronics Corporation. All rights reserved.
***********************************************************************************************************************/

/******************************************************************************
* File Name    : app_main.c
* Tool-Chain   : e2Studio
* Description  : This is a application file for peripheral role.
*******************************************************************************/

/******************************************************************************
 Includes   <System Includes> , "Project Includes"
*******************************************************************************/
#include <string.h>
#include "r_ble_api.h"
#include "rm_ble_abs.h"
#include "rm_ble_abs_api.h"
#include "gatt_db.h"
#include "profile_cmn/r_ble_servs_if.h"
#include "profile_cmn/r_ble_servc_if.h"
#include "hal_data.h"

/* This code is needed for using FreeRTOS */
#if (BSP_CFG_RTOS == 2 || BSP_CFG_RTOS_USED == 1)
#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"
#define BLE_EVENT_PATTERN   (0x0A0A)
EventGroupHandle_t  g_ble_event_group_handle;
#endif
#include "r_ble_gat2s.h"
#include "r_ble_ia2s.h"
#include "r_ble_RX_SERs.h"
#include "r_ble_SUOTAs.h"

/******************************************************************************
 User file includes
*******************************************************************************/
/* Start user code for file includes. Do not edit comment generated here */
#include "qe_ble_profile.h"
#include "ble_thread.h"
#include "rs_ota_fw_update_over_ble_hal.h"
#include "timers.h"
/* End user code. Do not edit comment generated here */

#define BLE_LOG_TAG "app_main"
#define BLE_GATTS_QUEUE_ELEMENTS_SIZE       (14)
#define BLE_GATTS_QUEUE_BUFFER_LEN          (245)
#define BLE_GATTS_QUEUE_NUM                 (1)

/******************************************************************************
 User macro definitions
*******************************************************************************/
/* Start user code for macro definitions. Do not edit comment generated here */

/* Diagnostic output for this file.
 *
 * This is RS code, so it must NOT reach into src/app and must NOT call SEGGER
 * RTT: it reports through the RS HAL sink, which formats the line and hands it
 * to the single application callback (cfg->p_callback). What that callback does
 * with it - RTT, VCOM, nothing - is the application's decision.
 *
 * The sink must never block: this runs on the BLE task, and a blocking write
 * stalls R_BLE_Execute(), starves the DA14531 GTL/UART link and makes the phone
 * SUOTA app time out (~20 s) and disconnect. Keep the Patch Data hot path silent.
 *
 * NOTE: this macro MUST stay inside this "user code" block. QE for BLE only
 * preserves code between the Start/End user code markers; when it lived just
 * above BLE_LOG_TAG (generated area) a profile regeneration silently deleted it
 * and every RS_LOG() call failed to link. */
#define RS_LOG(...)    rs_ota_fw_update_over_ble_hal_log(__VA_ARGS__)

/* End user code. Do not edit comment generated here */

/******************************************************************************
 Generated function prototype declarations
*******************************************************************************/
/* Internal functions */
void gap_cb(uint16_t type, ble_status_t result, st_ble_evt_data_t *p_data);
void gatts_cb(uint16_t type, ble_status_t result, st_ble_gatts_evt_data_t *p_data);
void gattc_cb(uint16_t type, ble_status_t result, st_ble_gattc_evt_data_t *p_data);
void vs_cb(uint16_t type, ble_status_t result, st_ble_vs_evt_data_t *p_data);
ble_status_t ble_init(void);
void app_main(void);

/******************************************************************************
 User function prototype declarations
*******************************************************************************/
/* Start user code for function prototype declarations. Do not edit comment generated here */

/* SUOTA original-app handshake helpers. Bodies are defined in the "User function
 * definitions" region at the bottom of this file, after all the global state they
 * reference (g_conn_hdl, byte counters, block state) has been declared. */
static void suota_notify_status(uint8_t status_code);
static void suota_update_mem_info(uint32_t total_bytes);
static void suota_handle_patch_data(const uint8_t * p_data, uint16_t len);
static uint8_t suota_status_from_rs(void);

/******************************************************************************
 * Function Name: ota_flush_timer_cb
 * Description  : Periodic FreeRTOS timer callback. It does NOT touch flash itself
 *                (flash must only be programmed from the BLE thread). It simply
 *                wakes the BLE main loop, which then performs the idle-flush check
 *                and the actual flash write in ble_thread context.
 ******************************************************************************/
static void ota_flush_timer_cb(TimerHandle_t xTimer)
{
    FSP_PARAMETER_NOT_USED(xTimer);
#if (BSP_CFG_RTOS == 2 || BSP_CFG_RTOS_USED == 1)
    if (NULL != g_ble_event_group_handle)
    {
        xEventGroupSetBits(g_ble_event_group_handle, (EventBits_t)BLE_EVENT_PATTERN);
    }
#endif
}

/* End user code. Do not edit comment generated here */

/******************************************************************************
 Generated global variables
*******************************************************************************/
/* Advertising Data */
static uint8_t gs_advertising_data[] =
{
    /* Flags */
    0x02, /**< Data Size */
    0x01, /**< Data Type */
    ( 0x06 ), /**< Data Value */

    /* Incomplete List of 128-bit Service Class UUIDs */
    0x11, /**< Data Size */
    0x06, /**< Data Type */
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0x9e, 0xfe, 0x00, 0x00, /**< Data Value */

    /* Shortened Local Name */
    0x09, /**< Data Size */
    0x08, /**< Data Type */
    0x72, 0x61, 0x5f, 0x73, 0x75, 0x6f, 0x74, 0x61, /**< Data Value */
};

ble_abs_legacy_advertising_parameter_t g_ble_advertising_parameter =
{
 .p_peer_address             = NULL,       ///< Peer address.
 .slow_advertising_interval  = 0x000000A0, ///< Slow advertising interval. 100.0(ms)
 .slow_advertising_period    = 0x0000,     ///< Slow advertising period.
 .p_advertising_data         = gs_advertising_data,             ///< Advertising data. If p_advertising_data is specified as NULL, advertising data is not set.
 .advertising_data_length    = ARRAY_SIZE(gs_advertising_data), ///< Advertising data length (in bytes).
 .advertising_filter_policy  = BLE_ABS_ADVERTISING_FILTER_ALLOW_ANY, ///< Advertising Filter Policy.
 .advertising_channel_map    = ( BLE_GAP_ADV_CH_37 | BLE_GAP_ADV_CH_38 | BLE_GAP_ADV_CH_39 ), ///< Channel Map.
 .own_bluetooth_address_type = BLE_GAP_ADDR_RAND, ///< Own Bluetooth address type.
 .own_bluetooth_address      = { 0 },
};

/* GATT server callback parameters */
ble_abs_gatt_server_callback_set_t gs_abs_gatts_cb_param[] =
{
    {
        .gatt_server_callback_function = gatts_cb,
        .gatt_server_callback_priority = 1,
    },
    {
        .gatt_server_callback_function = NULL,
    }
};

/* GATT client callback parameters */
ble_abs_gatt_client_callback_set_t gs_abs_gattc_cb_param[] =
{
    {
        .gatt_client_callback_function = gattc_cb,
        .gatt_client_callback_priority = 1,
    },
    {
        .gatt_client_callback_function = NULL,
    }
};

/* GATT server Prepare Write Queue parameters */
static st_ble_gatt_queue_elm_t  gs_queue_elms[BLE_GATTS_QUEUE_ELEMENTS_SIZE];
static uint8_t gs_buffer[BLE_GATTS_QUEUE_BUFFER_LEN];
static st_ble_gatt_pre_queue_t gs_queue[BLE_GATTS_QUEUE_NUM] = {
    {
        .p_buf_start = gs_buffer,
        .buffer_len  = BLE_GATTS_QUEUE_BUFFER_LEN,
        .p_queue     = gs_queue_elms,
        .queue_size  = BLE_GATTS_QUEUE_ELEMENTS_SIZE,
    }
};

/* Connection handle */
uint16_t g_conn_hdl = BLE_GAP_INVALID_CONN_HDL;

/******************************************************************************
 User global variables
*******************************************************************************/
/* Start user code for global variables. Do not edit comment generated here */
/* records which step of ble_init() failed and the raw error code.
 * Inspect these in the debugger when the __BKPT is hit.
 *   g_dbg_init_step: 1=RM_BLE_ABS_Open, 2=R_BLE_GATTS_SetDbInst,
 *                    3=R_BLE_SERVS_Init, 4=R_BLE_SERVC_Init,
 *                    5=R_BLE_GAT2S_Init, 6=R_BLE_IA2S_Init, 0=success
 *   g_dbg_init_err : raw fsp_err_t / ble_status_t value returned by that step */
volatile int32_t  g_dbg_init_step = 0;
volatile int32_t  g_dbg_init_err  = 0;
/* Actual byte count of the most recent SUOTA Patch Data write. The QE profile always
 * reports a fixed param_len (= sizeof the characteristic struct, 320), so
 * decode_st_ble_suotas_patch_data_t() publishes the true received length here. */
volatile uint16_t g_suota_last_patch_len = 0;
/* Cumulative number of SUOTA Patch Data bytes received in the current transfer.
 * Reset when a new memory device is selected, reported as the transfer progresses,
 * and printed as the final file length when the END/REBOOT command is received. */
volatile uint32_t g_suota_total_bytes = 0;
/* Idle auto-flush support: a periodic timer wakes the BLE loop, which flushes the
 * buffered flash tail if no Patch Data has arrived recently. This makes saving
 * independent of the SUOTA reboot/end command (not sent if the user cancels). */
static TimerHandle_t     s_ota_flush_timer  = NULL;
static volatile uint32_t s_ota_last_rx_tick = 0;

/* --- Staging-erase state machine ---------------------------------------------
 * The staging window is erased once per download, when the client selects a
 * memory device. Slot 2 keeps the previous image for fallback until then, so the
 * erase cannot be moved to a link-down moment (boot / disconnect): it has to run
 * with the phone connected and waiting.
 *
 * A code-flash erase masks every interrupt for up to ~240 ms per 32 KB block, and
 * SCI0 has no FIFO and no flow control, so the DA14531 bytes that arrive in that
 * window used to be lost - which desynced the GTL parser permanently (the next
 * notification failed with BLE_ERR_NOT_FOUND / 0x000D and advertising never came
 * back). That is now handled inside the RS flash HAL, which captures SCI0 RX with
 * the DTC during the blackout and replays it afterwards, so no byte is dropped.
 *
 * The erase is still driven one block per BLE main-loop pass. That is now only
 * about RESPONSIVENESS - R_BLE_Execute() gets to service the link between blocks
 * and the capture buffer stays small - not about correctness. IMAGE_STARTED is
 * notified after the last block, so the client stays parked meanwhile. */
static bool     s_ota_erase_active = false;   /* an erase sequence is running   */
static uint32_t s_ota_erase_index  = 0;       /* next block to erase            */

/* --- Original SUOTA app handshake state ----------------------------------------
 * The stock Renesas/Dialog SUOTA app is a strict state machine: it blocks waiting
 * for SPOTA_SERV_STATUS notifications. The server MUST notify:
 *   0x10 (IMAGE_STARTED) after the MEM_DEV memory-type write,
 *   0x02 (SERVICE_STATUS_OK) after every completed PATCH_LEN-sized block,
 *   0x02 after a good END, or an error code (0x04.. ) on failure.
 * These track the current block so the block-complete notify fires at the right
 * moment. */
#define SUOTA_STATUS_SPOTA_STARTED      (0x01u)
#define SUOTA_STATUS_OK                 (0x02u)  /* block stored / END verified OK  */
#define SUOTA_STATUS_CRC_MISMATCH       (0x04u)
#define SUOTA_STATUS_PATCH_LEN_ERROR    (0x05u)
#define SUOTA_STATUS_INT_MEM_ERROR      (0x07u)
#define SUOTA_STATUS_INVALID_MEM_TYPE   (0x08u)
#define SUOTA_STATUS_APP_ERROR          (0x09u)
#define SUOTA_STATUS_IMAGE_STARTED      (0x10u)  /* SUOTA image transfer started    */
#define SUOTA_STATUS_INVALID_IMG_HDR    (0x12u)

#define SUOTA_MEM_DEV_END               (0xFE000000u)
#define SUOTA_MEM_DEV_REBOOT            (0xFD000000u)
#define SUOTA_MEM_DEV_ABORT             (0xFF000000u)

/* --- SUOTA capability characteristics: fixed values the stock app reads once ------
 * The values the phone actually reads come from the QE GATT table (qe_ble_profile.c);
 * these #defines only mirror that table for reference/trace purposes.
 *
 * WORKING COMBO (verified on-device): SUOTA_MTU = 23, PATCH_DATA_CHAR_SIZE = 240.
 *   - Android auto-negotiates a LARGE real ATT MTU with the DA14531 at connect (the
 *     DA accepts up to max_mtu=247, r_ble_gtl.c), so 240-byte PATCH_DATA writes go
 *     through physically regardless of what we advertise (observed len=240).
 *   - The advertised SUOTA_MTU is only a HINT to the app. Advertising 247 makes the
 *     stock app run an MTU path that TIMES OUT on this phone/DA combo ("file upload
 *     timeout"). Advertising 23 avoids that path; the big real MTU still carries the
 *     240-byte chunks -> full-speed transfer that completes.
 * So do NOT set SUOTA_MTU to 247 here even though char_size is 240; the usual
 * "char_size <= MTU-3" rule does not apply because advertised MTU != real link MTU. */
#define SUOTA_CAP_VERSION        (0x01u)   /* SUOTA protocol version                    */
#define SUOTA_CAP_PATCH_CHAR_SZ  (240u)    /* bytes per PATCH_DATA write (real MTU is big)*/
#define SUOTA_CAP_MTU            (23u)     /* advertise 23: 247 makes the app time out  */
#define SUOTA_CAP_L2CAP_PSM      (0x0000u) /* 0 = no L2CAP CoC fast path                */

/* Expected size (bytes) of the current block, from the last SPOTA_PATCH_LEN write. */
static volatile uint16_t s_suota_block_len      = 0;
/* Bytes received so far towards the current (not yet completed) block. */
static volatile uint32_t s_suota_block_received = 0;
/* End user code. Do not edit comment generated here */

/******************************************************************************
 Generated function definitions
*******************************************************************************/
/******************************************************************************
 * Function Name: gap_cb
 * Description  : Callback function for GAP API.
 * Arguments    : uint16_t type -
 *                  Event type of GAP API.
 *              : ble_status_t result -
 *                  Event result of GAP API.
 *              : st_ble_vs_evt_data_t *p_data - 
 *                  Event parameters of GAP API.
 * Return Value : none
 ******************************************************************************/
void gap_cb(uint16_t type, ble_status_t result, st_ble_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for GAP callback function common process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

    switch(type)
    {
        case BLE_GAP_EVENT_STACK_ON:
        {
            /* Do NOT request the BD address here: rm_ble_abs_gtl.c already calls
             * R_BLE_VS_GetBdAddr() on BLE_GAP_EVENT_STACK_ON before forwarding this
             * event to us. Requesting it again produced a second
             * BLE_VS_EVENT_GET_ADDR_COMP -> a second StartLegacyAdvertising() ->
             * a duplicate BLE_GAP_EVENT_ADV_ON at boot.
             * (Re-apply this after every QE "Generate Code": QE restores its own
             *  R_BLE_VS_GetBdAddr() call here because this is a generated region.) */
        } break;

        case BLE_GAP_EVENT_CONN_IND:
        {
            if (BLE_SUCCESS == result)
            {
                /* Store connection handle */
                st_ble_gap_conn_evt_t *p_gap_conn_evt_param = (st_ble_gap_conn_evt_t *)p_data->p_param;
                g_conn_hdl = p_gap_conn_evt_param->conn_hdl;
            }
            else
            {
                /* Restart advertising when connection failed */
                RM_BLE_ABS_StartLegacyAdvertising(&g_ble_abs0_ctrl, &g_ble_advertising_parameter);
            }
        } break;

        case BLE_GAP_EVENT_DISCONN_IND:
        {
            /* Restart advertising when disconnected */
            g_conn_hdl = BLE_GAP_INVALID_CONN_HDL;
            RM_BLE_ABS_StartLegacyAdvertising(&g_ble_abs0_ctrl, &g_ble_advertising_parameter);
        } break;

        case BLE_GAP_EVENT_CONN_PARAM_UPD_REQ:
        {
            /* Send connection update response with value received on connection update request */
            st_ble_gap_conn_upd_req_evt_t *p_conn_upd_req_evt_param = (st_ble_gap_conn_upd_req_evt_t *)p_data->p_param;

            st_ble_gap_conn_param_t conn_updt_param = {
                .conn_intv_min = p_conn_upd_req_evt_param->conn_intv_min,
                .conn_intv_max = p_conn_upd_req_evt_param->conn_intv_max,
                .conn_latency  = p_conn_upd_req_evt_param->conn_latency,
                .sup_to        = p_conn_upd_req_evt_param->sup_to,
            };

            R_BLE_GAP_UpdConn(p_conn_upd_req_evt_param->conn_hdl,
                              BLE_GAP_CONN_UPD_MODE_RSP,
                              BLE_GAP_CONN_UPD_ACCEPT,
                              &conn_updt_param);
        } break;

/* Hint: Add cases of GAP event macros defined as BLE_GAP_XXX */
/* Start user code for GAP callback function event process. Do not edit comment generated here */
        case BLE_GAP_EVENT_ADV_ON:
        {
            /* Advertising has started */
            static uint32_t s_adv_count = 0;
            s_adv_count++;
            rs_ota_fw_update_over_ble_hal_link_adv_on();
            RS_LOG("[BLE] Advertising started (result=0x%04X, count=%u)\r\n",
                    (unsigned int)result, (unsigned int)s_adv_count);

            /* NOTE: the SUOTA capability values (version=0x01, char_size=240, mtu=23,
             * l2cap=0) are already baked into the QE GATT table (qe_ble_profile.c) as
             * read-only 'RI' attributes, so the DB already returns them. Calling
             * R_BLE_GATTS_SetAttr on those const attributes just fails with 0x3003
             * (WRITE_NOT_PERMITTED) and is not needed. */
        } break;
/* End user code. Do not edit comment generated here */
    }
}

/******************************************************************************
 * Function Name: gatts_cb
 * Description  : Callback function for GATT Server API.
 * Arguments    : uint16_t type -
 *                  Event type of GATT Server API.
 *              : ble_status_t result -
 *                  Event result of GATT Server API.
 *              : st_ble_gatts_evt_data_t *p_data - 
 *                  Event parameters of GATT Server API.
 * Return Value : none
 ******************************************************************************/
void gatts_cb(uint16_t type, ble_status_t result, st_ble_gatts_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for GATT Server callback function common process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

    R_BLE_SERVS_GattsCb(type, result, p_data);
    switch(type)
    {
/* Hint: Add cases of GATT Server event macros defined as BLE_GATTS_XXX */
/* Start user code for GATT Server callback function event process. Do not edit comment generated here */
        case BLE_GATTS_EVENT_EX_MTU_REQ:
        {
            /* The client asked to raise the ATT MTU. On this GTL bridge the actual
             * MTU is decided by the DA14531 (R_BLE_GATTS_RspExMtu is a no-op stub),
             * so this only tells us what the phone WANTS; the real per-write size is
             * what PATCH_DATA actually carries. */
            uint16_t req_mtu = 0;
            if ((NULL != p_data) && (NULL != p_data->p_param))
            {
                req_mtu = ((const st_ble_gatts_ex_mtu_req_evt_t *)p_data->p_param)->mtu;
            }
            (void)req_mtu;
            RS_LOG("[SUOTA] EX_MTU_REQ mtu=%u (advertised cap MTU=%u/char=%u; "
                    "DA controls real MTU)\r\n",
                    (unsigned int)req_mtu,
                    (unsigned int)SUOTA_CAP_MTU,
                    (unsigned int)SUOTA_CAP_PATCH_CHAR_SZ);
        } break;
/* End user code. Do not edit comment generated here */
    }
}

/******************************************************************************
 * Function Name: gattc_cb
 * Description  : Callback function for GATT Client API.
 * Arguments    : uint16_t type -
 *                  Event type of GATT Client API.
 *              : ble_status_t result -
 *                  Event result of GATT Client API.
 *              : st_ble_gattc_evt_data_t *p_data - 
 *                  Event parameters of GATT Client API.
 * Return Value : none
 ******************************************************************************/
void gattc_cb(uint16_t type, ble_status_t result, st_ble_gattc_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for GATT Client callback function common process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

    R_BLE_SERVC_GattcCb(type, result, p_data);
    switch(type)
    {

/* Hint: Add cases of GATT Client event macros defined as BLE_GATTC_XXX */
/* Start user code for GATT Client callback function event process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
    }
}

/******************************************************************************
 * Function Name: vs_cb
 * Description  : Callback function for Vendor Specific API.
 * Arguments    : uint16_t type -
 *                  Event type of Vendor Specific API.
 *              : ble_status_t result -
 *                  Event result of Vendor Specific API.
 *              : st_ble_vs_evt_data_t *p_data - 
 *                  Event parameters of Vendor Specific API.
 * Return Value : none
 ******************************************************************************/
void vs_cb(uint16_t type, ble_status_t result, st_ble_vs_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for vender specific callback function common process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
    
    R_BLE_SERVS_VsCb(type, result, p_data);
    switch(type)
    {
        case BLE_VS_EVENT_GET_ADDR_COMP:
        {
            /* Start advertising when BD address is ready */
            st_ble_vs_get_bd_addr_comp_evt_t * get_address = (st_ble_vs_get_bd_addr_comp_evt_t *)p_data->p_param;
            memcpy(g_ble_advertising_parameter.own_bluetooth_address, get_address->addr.addr, BLE_BD_ADDR_LEN);
            RM_BLE_ABS_StartLegacyAdvertising(&g_ble_abs0_ctrl, &g_ble_advertising_parameter);
        } break;

/* Hint: Add cases of vender specific event macros defined as BLE_VS_XXX */
/* Start user code for vender specific callback function event process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
    }
}

/******************************************************************************
 * Function Name: gat2s_cb
 * Description  : Callback function for GATT Service server feature.
 * Arguments    : uint16_t type -
 *                  Event type of GATT Service server feature.
 *              : ble_status_t result -
 *                  Event result of GATT Service server feature.
 *              : st_ble_servs_evt_data_t *p_data - 
 *                  Event parameters of GATT Service server feature.
 * Return Value : none
 ******************************************************************************/
static void gat2s_cb(uint16_t type, ble_status_t result, st_ble_servs_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for GATT Service Server callback function common process. Do not edit comment generated here */
    FSP_PARAMETER_NOT_USED(result);
    FSP_PARAMETER_NOT_USED(p_data);
/* End user code. Do not edit comment generated here */

    switch(type)
    {
/* Hint: Add cases of GATT Service server events defined in e_ble_gat2s_event_t */
/* Start user code for GATT Service Server callback function event process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
    }    
}

/******************************************************************************
 * Function Name: ia2s_cb
 * Description  : Callback function for RA6E2 IO Control server feature.
 * Arguments    : uint16_t type -
 *                  Event type of RA6E2 IO Control server feature.
 *              : ble_status_t result -
 *                  Event result of RA6E2 IO Control server feature.
 *              : st_ble_servs_evt_data_t *p_data - 
 *                  Event parameters of RA6E2 IO Control server feature.
 * Return Value : none
 ******************************************************************************/
static void ia2s_cb(uint16_t type, ble_status_t result, st_ble_servs_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for RA6E2 IO Control Server callback function common process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */

    switch(type)
    {
/* Hint: Add cases of RA6E2 IO Control server events defined in e_ble_ia2s_event_t */
/* Start user code for RA6E2 IO Control Server callback function event process. Do not edit comment generated here */
        case BLE_IA2S_EVENT_LEDCTRL_WRITE_REQ:
               {
                   if (BLE_SUCCESS == result)
                   {
                       uint8_t Level = *(uint8_t *)p_data->p_param;
                       if(Level == 0x00)
                       {
                           R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_06,BSP_IO_LEVEL_LOW);
                       }
                       else
                       {
                           R_IOPORT_PinWrite(&g_ioport_ctrl, BSP_IO_PORT_02_PIN_06,BSP_IO_LEVEL_HIGH);
                       }
                   }
               }
               break;

               default:
                   break;
/* End user code. Do not edit comment generated here */
    }    
}

/******************************************************************************
 * Function Name: RX_SERs_cb
 * Description  : Callback function for RX server feature.
 * Arguments    : uint16_t type -
 *                  Event type of RX server feature.
 *              : ble_status_t result -
 *                  Event result of RX server feature.
 *              : st_ble_servs_evt_data_t *p_data - 
 *                  Event parameters of RX server feature.
 * Return Value : none
 ******************************************************************************/
static void RX_SERs_cb(uint16_t type, ble_status_t result, st_ble_servs_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for RX Server callback function common process. Do not edit comment generated here */
    FSP_PARAMETER_NOT_USED(result);
    FSP_PARAMETER_NOT_USED(p_data);
/* End user code. Do not edit comment generated here */

    switch(type)
    {
/* Hint: Add cases of RX server events defined in e_ble_RX_SERs_event_t */
/* Start user code for RX Server callback function event process. Do not edit comment generated here */
/* End user code. Do not edit comment generated here */
    }    
}

/******************************************************************************
 * Function Name: SUOTAs_cb
 * Description  : Callback function for SUOTA server feature.
 * Arguments    : uint16_t type -
 *                  Event type of SUOTA server feature.
 *              : ble_status_t result -
 *                  Event result of SUOTA server feature.
 *              : st_ble_servs_evt_data_t *p_data - 
 *                  Event parameters of SUOTA server feature.
 * Return Value : none
 ******************************************************************************/
static void SUOTAs_cb(uint16_t type, ble_status_t result, st_ble_servs_evt_data_t *p_data)
{
/* Hint: Input common process of callback function such as variable definitions */
/* Start user code for SUOTA Server callback function common process. Do not edit comment generated here */
    /* 'result' was only consumed by the removed trace output. */
    FSP_PARAMETER_NOT_USED(result);
/* End user code. Do not edit comment generated here */

    switch(type)
    {
/* Hint: Add cases of SUOTA server events defined in e_ble_SUOTAs_event_t */
/* Start user code for SUOTA Server callback function event process. Do not edit comment generated here */

        /* ---- Memory Device (client writes memory type / base address here) ---- */
        case BLE_SUOTAS_EVENT_MEM_DEV_WRITE_REQ:
        {
            /* The SUOTA client uses the Memory Device characteristic both to SELECT a
             * memory device (start of transfer) and to issue control commands, encoded
             * in the most-significant byte:
             *   0x13xxxxxx / 0x12xxxxxx = start SUOTA to external SPI/I2C (bank in LSBs)
             *   0xFExxxxxx = END OF IMAGE (finish, verify CRC, commit)
             *   0xFDxxxxxx = REBOOT (apply the new image)
             *   0xFFxxxxxx = ABORT (cancel the running session)
             * The stock app blocks on a SERV_STATUS notification after each of these,
             * so we must respond with the matching status code (see guide §6). */
            if ((NULL != p_data) && (p_data->param_len >= sizeof(uint32_t)) && (NULL != p_data->p_param))
            {
                uint32_t mem_dev_cmd = *(const uint32_t *)p_data->p_param;
                switch (mem_dev_cmd)
                {
                    case SUOTA_MEM_DEV_REBOOT:
                        RS_LOG("\r\n---- REBOOT ---- (cmd 0x%08X)"
                                "  File length received = %u bytes\r\n\r\n",
                                (unsigned int)mem_dev_cmd,
                                (unsigned int)g_suota_total_bytes);
                        /* This - NOT END - is the client asking for the staged
                         * image to be activated. Flush/verify first (feed_end is
                         * idempotent: a transfer that already ended has nothing
                         * buffered and simply re-verifies), answer the client
                         * while the link is still up, THEN latch the install
                         * request. The RS ignores the request if the image did
                         * not verify, and the application owns the actual reset,
                         * so the notification below always gets out first. */
                        rs_ota_fw_update_over_ble_hal_feed_end();
                        suota_update_mem_info(g_suota_total_bytes);
                        suota_notify_status(suota_status_from_rs());
                        rs_ota_fw_update_over_ble_hal_feed_install_request();
                        break;

                    case SUOTA_MEM_DEV_END:
                    {
                        RS_LOG("\r\n---- END OF IMAGE ---- (cmd 0x%08X)"
                                "  File length received = %u bytes\r\n\r\n",
                                (unsigned int)mem_dev_cmd,
                                (unsigned int)g_suota_total_bytes);
                        /* The RS commits the final (partial) page and verifies the
                         * header + CRC32; we only translate its verdict for the client.
                         *
                         * END means "the image is complete", nothing more: the client
                         * may follow it with REBOOT, or leave the image staged and
                         * disconnect. Do NOT trigger an install from here - resetting
                         * now would drop a client that is still mid-conversation. */
                        rs_ota_fw_update_over_ble_hal_feed_end();
                        suota_update_mem_info(g_suota_total_bytes);
                        suota_notify_status(suota_status_from_rs());
                    } break;

                    case SUOTA_MEM_DEV_ABORT:
                        RS_LOG("\r\n---- ABORT ---- (cmd 0x%08X)\r\n\r\n",
                                (unsigned int)mem_dev_cmd);
                        /* Discard the in-flight session state; do NOT touch running FW.
                         * Also drop a staging erase that has not finished yet. */
                        s_suota_block_len      = 0;
                        s_suota_block_received = 0;
                        g_suota_total_bytes    = 0;
                        s_ota_erase_active     = false;
                        rs_ota_fw_update_over_ble_hal_feed_error(RS_STATUS_ERR_HARDWARE);
                        break;

                    default:
                        /* Any non-command value selects a memory device, i.e. the start of a
                         * new transfer. Reset all per-session counters and ARM the staging
                         * erase; the blocks are erased from the main loop (one per pass) and
                         * IMAGE_STARTED (0x10) is notified once the last one is done.
                         *
                         * Erasing here - once per download - instead of at every boot keeps an
                         * already staged image across resets and spares the flash. Do NOT erase
                         * inline in this callback: the resulting interrupt-off window breaks
                         * the GTL link (see the s_ota_erase_* comment block above). */
                        g_suota_total_bytes    = 0;
                        s_suota_block_len      = 0;
                        s_suota_block_received = 0;
                        suota_update_mem_info(0);
                        RS_LOG("[SUOTA] Transfer start (mem_dev=0x%08X) - erasing staging\r\n",
                                (unsigned int)mem_dev_cmd);

                        s_ota_erase_index  = 0;
                        s_ota_erase_active = true;
                        break;
                }
            }
        } break;
        case BLE_SUOTAS_EVENT_MEM_DEV_WRITE_COMP:     break;
        case BLE_SUOTAS_EVENT_MEM_DEV_READ_REQ:       break;

        /* ---- Memory Info ---- */
        case BLE_SUOTAS_EVENT_MEM_INFO_READ_REQ:      break;

        /* ---- Patch Length (block size the client will use) ----
         * The app writes the number of bytes in the NEXT block here (a full block, or
         * the smaller remainder for the final block). Latch it and start counting the
         * block from zero so we know exactly when the block is complete. */
        case BLE_SUOTAS_EVENT_PATCH_LEN_WRITE_REQ:
        {
            if ((NULL != p_data) && (p_data->param_len >= sizeof(uint16_t)) && (NULL != p_data->p_param))
            {
                s_suota_block_len      = *(const uint16_t *)p_data->p_param;
                s_suota_block_received = 0;

                if (!rs_ota_fw_update_over_ble_hal_link_block_len_ok(s_suota_block_len))
                {
                    s_suota_block_len = 0;   /* the HAL drops the data; never complete a block */
                    suota_notify_status(SUOTA_STATUS_PATCH_LEN_ERROR);
                    break;
                }

                RS_LOG("[SUOTA] Block length set to %u bytes\r\n",
                        (unsigned int)s_suota_block_len);
            }
        } break;
        case BLE_SUOTAS_EVENT_PATCH_LEN_WRITE_COMP:   break;
        case BLE_SUOTAS_EVENT_PATCH_LEN_READ_REQ:     break;

        /* ---- Patch Data (raw image bytes streamed by the client) ----
         * HOT PATH: keep this silent. Any per-chunk trace floods the log transport and,
         * with a blocking back-end, stalls the BLE thread mid-transfer. If a VCOM
         * back-end is added later, log progress at most once per block, never per chunk.
         *
         * IMPORTANT: the DA14531 GTL bridge reports EVERY peer write as PEER_WRITE_REQ
         * (db_op is hardcoded in r_ble_gtl.c's write_req_ind_handler). So after making
         * PATCH_DATA 'Write Without Response', the runtime event is still WRITE_REQ
         * (id 0x0C00), NOT WRITE_CMD (0x0C01). The regenerated enum only names
         * ..._WRITE_CMD, so we ALSO match the computed WRITE_REQ id below - otherwise
         * every patch chunk falls through to 'default' and is dropped, the block never
         * completes, and the app times out at 0%. */
        case BLE_SUOTAS_EVENT_PATCH_DATA_WRITE_CMD:
        case (BLE_SERVS_ATTR_EVENT(BLE_SUOTAS_PATCH_DATA_IDX, BLE_SERVS_WRITE_REQ)):
            suota_handle_patch_data((const uint8_t *)(p_data ? p_data->p_param : NULL),
                                    g_suota_last_patch_len);
            break;

        /* ---- Service Status ---- */
        case BLE_SUOTAS_EVENT_SERV_STATUS_READ_REQ:        break;
        case BLE_SUOTAS_EVENT_SERV_STATUS_CCCD_WRITE_REQ:  break;
        case BLE_SUOTAS_EVENT_SERV_STATUS_CCCD_WRITE_COMP: break;
        case BLE_SUOTAS_EVENT_SERV_STATUS_CCCD_READ_REQ:   break;

        /* ---- GPIO Map ---- */
        case BLE_SUOTAS_EVENT_GPIO_MAP_WRITE_REQ:     break;
        case BLE_SUOTAS_EVENT_GPIO_MAP_WRITE_COMP:    break;
        case BLE_SUOTAS_EVENT_GPIO_MAP_READ_REQ:      break;

        /* ---- Capability characteristics (read once by the app before onDeviceReady) ----
         * Auto-answered from the QE GATT table; nothing to do here. */
        case BLE_SUOTAS_EVENT_SUOTA_VERSION_READ_REQ:                break;
        case BLE_SUOTAS_EVENT_SUOTA_PATCH_DATA_CHAR_SIZE_READ_REQ:   break;
        case BLE_SUOTAS_EVENT_SUOTA_MTU_READ_REQ:                    break;
        case BLE_SUOTAS_EVENT_SUOTA_CAP_L2CAP_PSM_READ_REQ:          break;

        default:
            break;

/* End user code. Do not edit comment generated here */
    }    
}
/******************************************************************************
 * Function Name: ble_init
 * Description  : Initialize BLE and profiles.
 * Arguments    : none
 * Return Value : BLE_SUCCESS - SUCCESS
 *                BLE_ERR_INVALID_OPERATION -
 *                    Failed to initialize BLE or profiles.
 ******************************************************************************/
ble_status_t ble_init(void)
{
    ble_status_t status;
    fsp_err_t err;

    /* Initialize BLE */
    err = RM_BLE_ABS_Open(&g_ble_abs0_ctrl, &g_ble_abs0_cfg);
    if (FSP_SUCCESS != err)
    {
        /* fsp_err_t values fit in 16 bits; explicit cast instead of an implicit
         * narrowing (QE generates "return err;" - re-apply after "Generate Code"). */
        return (ble_status_t) err;
    }

    /* Initialize GATT Database */
    status = R_BLE_GATTS_SetDbInst(NULL);
    if (BLE_SUCCESS != status)
    {
        return BLE_ERR_INVALID_OPERATION;
    }

    /* Initialize GATT server */
    status = R_BLE_SERVS_Init();
    if (BLE_SUCCESS != status)
    {
        return BLE_ERR_INVALID_OPERATION;
    }

    /*Initialize GATT client */
    status = R_BLE_SERVC_Init();
    if (BLE_SUCCESS != status)
    {
        return BLE_ERR_INVALID_OPERATION;
    }
    
    /* Set Prepare Write Queue */
    R_BLE_GATTS_SetPrepareQueue(gs_queue, BLE_GATTS_QUEUE_NUM);

    /* Initialize GATT Service server API */
    status = R_BLE_GAT2S_Init(gat2s_cb);
    if (BLE_SUCCESS != status)
    {
        return BLE_ERR_INVALID_OPERATION;
    }

    /* Initialize RA6E2 IO Control server API */
    status = R_BLE_IA2S_Init(ia2s_cb);
    if (BLE_SUCCESS != status)
    {
        return BLE_ERR_INVALID_OPERATION;
    }

    /* Initialize RX server API */
    status = R_BLE_RX_SERS_Init(RX_SERs_cb);
    if (BLE_SUCCESS != status)
    {
        return BLE_ERR_INVALID_OPERATION;
    }

    /* Initialize SUOTA server API */
    status = R_BLE_SUOTAS_Init(SUOTAs_cb);
    if (BLE_SUCCESS != status)
    {
        return BLE_ERR_INVALID_OPERATION;
    }

    return status;
}

/******************************************************************************
 * Function Name: app_main
 * Description  : Application main function with main loop
 * Arguments    : none
 * Return Value : none
 ******************************************************************************/
void app_main(void)
{
#if (BSP_CFG_RTOS == 2 || BSP_CFG_RTOS_USED == 1)
    /* Create Event Group */
    g_ble_event_group_handle = xEventGroupCreate();
    assert(g_ble_event_group_handle);
#endif

    /* Initialize BLE and profiles */
    ble_init();

/* Hint: Input process that should be done before main loop such as calling initial function or variable definitions */
/* Start user code for process before main loop. Do not edit comment generated here */

    /* NOTE: trace output goes through RS_LOG() (see the macro at the top of this
     * file), which reports to the application via the RS callback. Whatever the
     * application writes it to MUST be non-blocking - a blocking transport stalls
     * the BLE thread and breaks SUOTA. Lines emitted before the application
     * registers its callback are parked by the HAL and replayed on registration.
     *
     * NOTE: the SUOTA capability values are served from the QE GATT table, so nothing
     * has to be seeded at runtime.
     *
     * NOTE: the OTA staging block is NOT erased at start-up. It is erased once per
     * download, from the SUOTA memory-device-select handler (SUOTAs_cb, transfer
     * start), so a staged image survives a reset and the flash is only cycled when
     * a new image is actually about to be downloaded. */

    /* Start a periodic timer that wakes the BLE loop so it can auto-flush a buffered
     * OTA tail if the transfer went idle (see the idle-flush check in the main loop). */
    s_ota_flush_timer = xTimerCreate("ota_flush", pdMS_TO_TICKS(250), pdTRUE, NULL, ota_flush_timer_cb);
    if ((NULL == s_ota_flush_timer) || (pdPASS != xTimerStart(s_ota_flush_timer, 0)))
    {
        /* Without the periodic wake the loop only runs on BLE events: no idle
         * flush, no link supervision while silent, and Stop() falls back to its
         * forced path after the timeout. */
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                "[SUOTA] flush timer %s FAILED - idle flush / link supervision degraded\r\n",
                (NULL == s_ota_flush_timer) ? "create" : "start");
    }

/* End user code. Do not edit comment generated here */

    /* main loop */
    while (1)
    {
        /* Process BLE Event */
        R_BLE_Execute();

/* When this BLE application works on the FreeRTOS */
#if (BSP_CFG_RTOS == 2 || BSP_CFG_RTOS_USED == 1)
        /* The (false == s_ota_erase_active) term keeps the loop spinning while the
         * staging erase is in progress, so the one-block-per-pass erase below can
         * make progress. QE regenerates this line without it - re-apply after every
         * "Generate Code". */
        if((0 != R_BLE_IsTaskFree()) && (false == s_ota_erase_active))
        {
            /* If the BLE Task has no operation to be processed, it transits block state until the event from RF transciever occurs. */
            xEventGroupWaitBits(g_ble_event_group_handle,
                                (EventBits_t)BLE_EVENT_PATTERN,
                                pdTRUE,
                                pdFALSE,
                                portMAX_DELAY);
        }
#endif

/* Hint: Input process that should be done during main loop such as calling processing functions */
/* Start user code for process during main loop. Do not edit comment generated here */
        /* Staging erase, one block per loop pass, so R_BLE_Execute() above keeps
         * servicing the GTL link between blocks and the client is not left
         * waiting longer than necessary. The erase itself is made safe by the
         * DTC capture/replay guard in the RS flash HAL, not by this split. */
        if (s_ota_erase_active)
        {
            rs_status_t est = rs_ota_fw_update_over_ble_hal_stage_erase_block(s_ota_erase_index);

            if (RS_STATUS_SUCCESS != est)
            {
                /* Staging area unusable -> tell the app instead of letting it push an
                 * image that can never be stored. */
                rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                        "[SUOTA] staging erase FAILED at block %u - aborting transfer\r\n",
                        (unsigned int)s_ota_erase_index);
                s_ota_erase_active = false;
                suota_notify_status(SUOTA_STATUS_INT_MEM_ERROR);
            }
            else
            {
                s_ota_erase_index++;
                if (s_ota_erase_index >= rs_ota_fw_update_over_ble_hal_stage_block_count())
                {
                    /* Window is clean: tell the RS a transfer is starting, then
                     * release the client. */
                    s_ota_erase_active = false;
                    rs_ota_fw_update_over_ble_hal_feed_start();
                    RS_LOG("[SUOTA] staging erased - image transfer may start\r\n");
                    suota_notify_status(SUOTA_STATUS_IMAGE_STARTED);
                }
            }
        }

        /* Idle auto-flush: if image bytes are buffered but no new Patch Data has
         * arrived recently, commit the tail to flash. Kept long (15 s) so a normal
         * mid-transfer pause does NOT trigger a premature stage_end() (which would
         * advance the flash offset and desync the rest of the image). The real END is
         * driven by the SUOTA MEM_DEV END/REBOOT command. */
        if (rs_ota_fw_update_over_ble_hal_stage_has_pending() &&
            (((uint32_t)xTaskGetTickCount() - s_ota_last_rx_tick) > pdMS_TO_TICKS(15000)))
        {
            if (RS_STATUS_SUCCESS != rs_ota_fw_update_over_ble_hal_stage_end())
            {
                /* The image tail is not in flash: fail the transfer (RS leaves
                 * IN_PROGRESS, staging is invalidated, later blocks are dropped)
                 * and tell the client instead of letting it carry on. */
                rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                        "[SUOTA] idle flush FAILED - aborting transfer\r\n");
                rs_ota_fw_update_over_ble_hal_feed_error(RS_STATUS_ERR_HARDWARE);
                suota_notify_status(suota_status_from_rs());

                /* If the tail is still buffered (e.g. the staging erase that
                 * discards it failed too), retry once per idle period, not on
                 * every loop pass. */
                s_ota_last_rx_tick = (uint32_t)xTaskGetTickCount();
            }
        }

        /* Never leave the board disconnected and silent (see the RS HAL). */
        rs_ota_fw_update_over_ble_hal_link_poll();

        /* Stop() asked the transport to shut down: leave the loop so BLE is
         * closed cleanly below (the RS HAL then retires this task). */
        if (rs_ota_fw_update_over_ble_hal_transport_stop_requested())
        {
            break;
        }
/* End user code. Do not edit comment generated here */
    }

/* Hint: Input process that should be done after main loop such as calling closing functions */
/* Start user code for process after main loop. Do not edit comment generated here */
    /* Disconnect the peer and stop advertising before the module is closed. */
    rs_ota_fw_update_over_ble_hal_transport_shutdown();

    /* Created at every app_main() run: free it, or each Stop()/Start() cycle
     * leaks FreeRTOS heap. Block until the timer task has deleted it, so its
     * callback can no longer touch the event group freed after this function. */
    if (NULL != s_ota_flush_timer)
    {
        (void) xTimerDelete(s_ota_flush_timer, portMAX_DELAY);
        s_ota_flush_timer = NULL;
    }
    s_ota_erase_active = false;
/* End user code. Do not edit comment generated here */

    /* Terminate BLE */
    RM_BLE_ABS_Close(&g_ble_abs0_ctrl);
}

/******************************************************************************
 User function definitions
*******************************************************************************/
/* Start user code for function definitions. Do not edit comment generated here */

/******************************************************************************
 * Function Name: suota_notify_status
 * Description  : Send a single SPOTA_SERV_STATUS notification to the connected
 *                SUOTA client. This is the ONLY channel the stock app uses to learn
 *                that a step succeeded/failed, so it must be called at the right
 *                moments (see the handshake notes above the SUOTA_STATUS_*
 *                defines). No-op if not connected.
 ******************************************************************************/
static void suota_notify_status(uint8_t status_code)
{
    ble_status_t st = BLE_SUCCESS;

    if (BLE_GAP_INVALID_CONN_HDL == g_conn_hdl)
    {
        return;
    }

    /* Keep the local GATT value in sync (so a client read returns the last status)
     * and push the notification. */
    (void)R_BLE_SUOTAS_SetServ_status(&status_code);
    st = R_BLE_SUOTAS_NotifyServ_status(g_conn_hdl, &status_code);

    if (st != BLE_SUCCESS)
    {
        rs_ota_fw_update_over_ble_hal_logs(RS_STATUS_ERR_HARDWARE,
                "[SUOTA] notify SERV_STATUS (0x%02X) failed (0x%04X)\r\n",
                (unsigned int)status_code, (unsigned int)st);
    }
}

/******************************************************************************
 * Function Name: suota_update_mem_info
 * Description  : Publish the running total of stored image bytes into the
 *                SPOTA_MEM_INFO characteristic so the app's read returns real
 *                progress.
 ******************************************************************************/
static void suota_update_mem_info(uint32_t total_bytes)
{
    (void)R_BLE_SUOTAS_SetMem_info(&total_bytes);
}

/******************************************************************************
 * Function Name: suota_status_from_rs
 * Description  : Translate the RS workflow status (public API) into the SUOTA
 *                SPOTA_SERV_STATUS code the client expects. This is the only
 *                place the transport glue interprets RS state.
 ******************************************************************************/
static uint8_t suota_status_from_rs(void)
{
    rs_ota_fw_update_over_ble_result_t res = {0};

    if (RS_STATUS_SUCCESS != rs_ota_fw_update_over_ble_StatusGet(&res))
    {
        return SUOTA_STATUS_APP_ERROR;
    }

    switch (res.status)
    {
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_SUCCESS:     return SUOTA_STATUS_OK;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_WAITING:     return SUOTA_STATUS_OK;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_IN_PROGRESS: return SUOTA_STATUS_OK;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_CRC:     return SUOTA_STATUS_CRC_MISMATCH;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_BAD_HEADER:  return SUOTA_STATUS_INVALID_IMG_HDR;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_NO_MEMORY:   return SUOTA_STATUS_INT_MEM_ERROR;
        case RS_OTA_FW_UPDATE_OVER_BLE_STATUS_TIMEOUT:     return SUOTA_STATUS_INT_MEM_ERROR;
        default:                                           return SUOTA_STATUS_APP_ERROR;
    }
}

/******************************************************************************
 * Function Name: suota_handle_patch_data
 * Description  : Hand one received PATCH_DATA chunk to the RS and drive the
 *                block-complete handshake. The RS stages the bytes; this glue
 *                only counts them towards the current block (size = last
 *                SPOTA_PATCH_LEN write). When a full block has been received,
 *                SPOTA_MEM_INFO is refreshed and a SERV_STATUS = 0x02 (OK)
 *                notification is sent so the app releases the next block.
 ******************************************************************************/
static void suota_handle_patch_data(const uint8_t * p_data, uint16_t len)
{
    if ((NULL == p_data) || (0U == len))
    {
        return;
    }

    /* Into the RS: the orchestrator stages the bytes and updates its status. */
    rs_ota_fw_update_over_ble_hal_feed_block(p_data, len);

    g_suota_total_bytes    += len;
    s_suota_block_received += len;
    s_ota_last_rx_tick      = (uint32_t)xTaskGetTickCount();

    uint8_t rs_status = suota_status_from_rs();
    if (SUOTA_STATUS_OK != rs_status)
    {
        /* Staging failed (e.g. window overflow) -> report and stop the block. */
        suota_notify_status(rs_status);
        return;
    }

    /* Block complete? Notify OK so the app sends the next PATCH_LEN / block. Use a
     * >= test and carry any surplus so an off-by-a-chunk never wedges the loop. */
    if ((s_suota_block_len > 0U) && (s_suota_block_received >= s_suota_block_len))
    {
        RS_LOG("[SUOTA] BLOCK COMPLETE (%u bytes, total %u)\r\n",
                (unsigned int)s_suota_block_len, (unsigned int)g_suota_total_bytes);
        suota_update_mem_info(g_suota_total_bytes);
        s_suota_block_received -= s_suota_block_len;
        suota_notify_status(SUOTA_STATUS_OK);
    }
}
/* End user code. Do not edit comment generated here */