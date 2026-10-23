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
* https://www.renesas.com/disclaimer
*
* Copyright (C) 2024 Renesas Electronics Corporation.
***********************************************************************************************************************/
/***********************************************************************************************************************
* File Name    : qe_ble_profile.c
* Description  : This file includes definitions.
***********************************************************************************************************************/
/***********************************************************************************************************************
* History      : MM/DD/YYYY Version Description
*              : 10/28/2024 1.00    First Release
*              : 17/12/2024 1.01    Support Read Indication
*              : 05/22/2025 1.02    Added write_cmd_cb to attribute structure
***********************************************************************************************************************/

#include "qe_ble_profile.h"

static uint16_t qe_uuid1 = 0x2800;
static uint16_t qe_value1 = 0x1800;
static uint16_t qe_uuid2 = 0x2803;
static uint16_t qe_uuid3 = 0x2A00;
static uint16_t qe_uuid4 = 0x2803;
static uint16_t qe_uuid5 = 0x2A01;
static uint16_t qe_uuid6 = 0x2803;
static uint16_t qe_uuid7 = 0x2A04;
static uint16_t qe_uuid8 = 0x2800;
static uint16_t qe_value8 = 0x1801;
static uint16_t qe_uuid9 = 0x2803;
static uint16_t qe_uuid10 = 0x2A05;
static uint16_t qe_uuid11 = 0x2902;
static uint16_t qe_uuid12 = 0x2800;
static uint16_t qe_value12[] = {0x9A90,0xF1B9,0x6906,0xA3C3,0x4B7A,0x94E4,0x93D7,0x423D};
static uint16_t qe_uuid13 = 0x2803;
static uint16_t qe_uuid15 = 0x2800;
static uint16_t qe_value15[] = {0x2308,0xBF93,0x2D42,0x8915,0x4540,0x651C,0xF173,0x7433};
static uint16_t qe_uuid16 = 0x2803;
static uint16_t qe_uuid18 = 0x2800;
static uint16_t qe_value18[] = {0x34FB,0x5F9B,0x0080,0x8000,0x1000,0x0000,0xFE9E,0x0000};
static uint16_t qe_uuid19 = 0x2803;
static uint16_t qe_uuid21 = 0x2803;
static uint16_t qe_uuid23 = 0x2803;
static uint16_t qe_uuid25 = 0x2803;
static uint16_t qe_uuid27 = 0x2803;
static uint16_t qe_uuid29 = 0x2902;
static uint16_t qe_uuid30 = 0x2803;
static uint16_t qe_uuid32 = 0x2803;
static uint16_t qe_uuid34 = 0x2803;
static uint16_t qe_uuid36 = 0x2803;
static uint16_t qe_uuid38 = 0x2803;

static r_ble_gtl_attr_write_cmd_t qe_write_cmd_cb_array[QE_BLE_PROFILE_NUMBER_OF_ATTRIBUTES];

const attribute_t qe_ble_profile[QE_ATTRIBUTE_HANDLE_PROFILE_END] =
{

	// Profile Declaration
	[0] =
	{
		.handle = 0,
		.encapsulated_attributes = 39,
		.permissions = 0x00, 
		.uuid_length = 0x00,
		.value_length = 0x00,
		.notify_write = 0,
		.notify_read = 0,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[0])
	},
	// Service Declaration: GAP Service
	[1] =
	{
		.handle = 1,
		.encapsulated_attributes = 7,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x02,
		.uuid = (uint8_t *)&qe_uuid1,
		.value = (uint8_t *)&qe_value1,
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[1])
	},
	// Characteristic Declaration: Device Name
	[2] =
	{
		.handle = 2,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x05,
		.uuid = (uint8_t *)&qe_uuid2,
		.value = (uint8_t []){0x0A,0x03,0x00,0x00,0x2A},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[2])
	},
	// Characteristic Value: Device Name
	[3] =
	{
		.handle = 3,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x80,
		.uuid = (uint8_t *)&qe_uuid3,
		.value = (uint8_t []){0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[3])
	},
	// Characteristic Declaration: Appearance
	[4] =
	{
		.handle = 4,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x05,
		.uuid = (uint8_t *)&qe_uuid4,
		.value = (uint8_t []){0x02,0x05,0x00,0x01,0x2A},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[4])
	},
	// Characteristic Value: Appearance
	[5] =
	{
		.handle = 5,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x02,
		.uuid = (uint8_t *)&qe_uuid5,
		.value = (uint8_t []){0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[5])
	},
	// Characteristic Declaration: Peripheral Preferred Connection Parameters
	[6] =
	{
		.handle = 6,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x05,
		.uuid = (uint8_t *)&qe_uuid6,
		.value = (uint8_t []){0x02,0x07,0x00,0x04,0x2A},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[6])
	},
	// Characteristic Value: Peripheral Preferred Connection Parameters
	[7] =
	{
		.handle = 7,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x08,
		.uuid = (uint8_t *)&qe_uuid7,
		.value = (uint8_t []){0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[7])
	},
	// Service Declaration: GATT Service
	[8] =
	{
		.handle = 8,
		.encapsulated_attributes = 4,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x02,
		.uuid = (uint8_t *)&qe_uuid8,
		.value = (uint8_t *)&qe_value8,
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[8])
	},
	// Characteristic Declaration: Service Changed
	[9] =
	{
		.handle = 9,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x05,
		.uuid = (uint8_t *)&qe_uuid9,
		.value = (uint8_t []){0x20,0x0A,0x00,0x05,0x2A},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[9])
	},
	// Characteristic Value: Service Changed
	[10] =
	{
		.handle = 10,
		.encapsulated_attributes = 0,
		.permissions = 0x00,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x04,
		.uuid = (uint8_t *)&qe_uuid10,
		.value = (uint8_t []){0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[10])
	},
	// Descriptor: Client Characteristic Configuration
	[11] =
	{
		.handle = 11,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x02,
		.uuid = (uint8_t *)&qe_uuid11,
		.value = (uint8_t []){0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 1,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[11])
	},
	// Service Declaration: RA6E2 IO Control
	[12] =
	{
		.handle = 12,
		.encapsulated_attributes = 3,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x10,
		.uuid = (uint8_t *)&qe_uuid12,
		.value = (uint8_t *)&qe_value12,
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[12])
	},
	// Characteristic Declaration: Led Control
	[13] =
	{
		.handle = 13,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid13,
		.value = (uint8_t []){0x0E,0x0E,0x00,0x16,0xBF,0x41,0x88,0x04,0xBD,0x6E,0xB2,0x0C,0x44,0xE2,0x05,0x2C,0xE4,0xB8,0x0D},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[13])
	},
	// Characteristic Value: Led Control
	[14] =
	{
		.handle = 14,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x01,
		.uuid = (uint8_t []){0x16,0xBF,0x41,0x88,0x04,0xBD,0x6E,0xB2,0x0C,0x44,0xE2,0x05,0x2C,0xE4,0xB8,0x0D},
		.value = (uint8_t []){0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[14])
	},
	// Service Declaration: RX
	[15] =
	{
		.handle = 15,
		.encapsulated_attributes = 3,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x10,
		.uuid = (uint8_t *)&qe_uuid15,
		.value = (uint8_t *)&qe_value15,
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[15])
	},
	// Characteristic Declaration: RX DATA
	[16] =
	{
		.handle = 16,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid16,
		.value = (uint8_t []){0x0C,0x11,0x00,0xC7,0xAF,0x54,0x1F,0xC9,0x27,0x62,0x8A,0xD0,0x41,0x53,0x95,0x22,0x6B,0xB9,0x1D},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[16])
	},
	// Characteristic Value: RX DATA
	[17] =
	{
		.handle = 17,
		.encapsulated_attributes = 0,
		.permissions = 0x02,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x14,
		.uuid = (uint8_t []){0xC7,0xAF,0x54,0x1F,0xC9,0x27,0x62,0x8A,0xD0,0x41,0x53,0x95,0x22,0x6B,0xB9,0x1D},
		.value = (uint8_t []){0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[17])
	},
	// Service Declaration: SUOTA
	[18] =
	{
		.handle = 18,
		.encapsulated_attributes = 22,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x10,
		.uuid = (uint8_t *)&qe_uuid18,
		.value = (uint8_t *)&qe_value18,
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[18])
	},
	// Characteristic Declaration: Memory Device
	[19] =
	{
		.handle = 19,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid19,
		.value = (uint8_t []){0x0A,0x14,0x00,0x34,0xCC,0x54,0xB9,0xF9,0x56,0xC6,0x91,0x21,0x40,0xA6,0x41,0xA8,0xCA,0x82,0x80},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[19])
	},
	// Characteristic Value: Memory Device
	[20] =
	{
		.handle = 20,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x04,
		.uuid = (uint8_t []){0x34,0xCC,0x54,0xB9,0xF9,0x56,0xC6,0x91,0x21,0x40,0xA6,0x41,0xA8,0xCA,0x82,0x80},
		.value = (uint8_t []){0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[20])
	},
	// Characteristic Declaration: Memory Info
	[21] =
	{
		.handle = 21,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid21,
		.value = (uint8_t []){0x02,0x16,0x00,0xD4,0x4F,0x33,0xFB,0x92,0x7C,0x22,0xA0,0xFE,0x45,0xA1,0x47,0x25,0xDB,0x53,0x6C},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[21])
	},
	// Characteristic Value: Memory Info
	[22] =
	{
		.handle = 22,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x04,
		.uuid = (uint8_t []){0xD4,0x4F,0x33,0xFB,0x92,0x7C,0x22,0xA0,0xFE,0x45,0xA1,0x47,0x25,0xDB,0x53,0x6C},
		.value = (uint8_t []){0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[22])
	},
	// Characteristic Declaration: Patch Length
	[23] =
	{
		.handle = 23,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid23,
		.value = (uint8_t []){0x0A,0x18,0x00,0x31,0xDA,0x3F,0x67,0x5B,0x85,0x83,0x91,0xD8,0x49,0x0C,0x00,0xA3,0xB9,0x84,0x9D},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[23])
	},
	// Characteristic Value: Patch Length
	[24] =
	{
		.handle = 24,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x02,
		.uuid = (uint8_t []){0x31,0xDA,0x3F,0x67,0x5B,0x85,0x83,0x91,0xD8,0x49,0x0C,0x00,0xA3,0xB9,0x84,0x9D},
		.value = (uint8_t []){0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[24])
	},
	// Characteristic Declaration: Pach Data
	[25] =
	{
		.handle = 25,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid25,
		.value = (uint8_t []){0x04,0x1A,0x00,0xB2,0x9C,0x7B,0xB1,0xD0,0x57,0x16,0x91,0xA1,0x4C,0x16,0xD5,0xE8,0x71,0x78,0x45},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[25])
	},
	// Characteristic Value: Pach Data
	[26] =
	{
		.handle = 26,
		.encapsulated_attributes = 0,
		.permissions = 0x02,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x140,
		.uuid = (uint8_t []){0xB2,0x9C,0x7B,0xB1,0xD0,0x57,0x16,0x91,0xA1,0x4C,0x16,0xD5,0xE8,0x71,0x78,0x45},
		.value = (uint8_t []){0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[26])
	},
	// Characteristic Declaration: Service Status
	[27] =
	{
		.handle = 27,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid27,
		.value = (uint8_t []){0x1A,0x1C,0x00,0x88,0x5C,0x06,0x6A,0xEB,0xB3,0x0A,0x99,0xF5,0x46,0x8C,0x79,0x94,0xDF,0x78,0x5F},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[27])
	},
	// Characteristic Value: Service Status
	[28] =
	{
		.handle = 28,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x01,
		.uuid = (uint8_t []){0x88,0x5C,0x06,0x6A,0xEB,0xB3,0x0A,0x99,0xF5,0x46,0x8C,0x79,0x94,0xDF,0x78,0x5F},
		.value = (uint8_t []){0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[28])
	},
	// Descriptor: Client Characteristic Configuration
	[29] =
	{
		.handle = 29,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x02,
		.uuid = (uint8_t *)&qe_uuid29,
		.value = (uint8_t []){0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 1,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[29])
	},
	// Characteristic Declaration: GPIO MAP
	[30] =
	{
		.handle = 30,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid30,
		.value = (uint8_t []){0x0A,0x1F,0x00,0x51,0x86,0xF0,0x5A,0x34,0x42,0x04,0x88,0x5F,0x4B,0xC3,0x5E,0xF0,0x49,0x42,0x72},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[30])
	},
	// Characteristic Value: GPIO MAP
	[31] =
	{
		.handle = 31,
		.encapsulated_attributes = 0,
		.permissions = 0x03,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x04,
		.uuid = (uint8_t []){0x51,0x86,0xF0,0x5A,0x34,0x42,0x04,0x88,0x5F,0x4B,0xC3,0x5E,0xF0,0x49,0x42,0x72},
		.value = (uint8_t []){0x00,0x00,0x00,0x00},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[31])
	},
	// Characteristic Declaration: SUOTA_VERSION
	[32] =
	{
		.handle = 32,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid32,
		.value = (uint8_t []){0x02,0x21,0x00,0x3A,0x91,0x3B,0xDB,0xC8,0xAC,0x1D,0xA2,0x1B,0x40,0xE5,0x0D,0xB5,0xE8,0xB4,0x64},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[32])
	},
	// Characteristic Value: SUOTA_VERSION
	[33] =
	{
		.handle = 33,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x01,
		.uuid = (uint8_t []){0x3A,0x91,0x3B,0xDB,0xC8,0xAC,0x1D,0xA2,0x1B,0x40,0xE5,0x0D,0xB5,0xE8,0xB4,0x64},
		.value = (uint8_t []){0x01},
		.notify_write = 1,
		.notify_read = 0,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[33])
	},
	// Characteristic Declaration: SUOTA_PATCH_DATA_CHAR_SIZE
	[34] =
	{
		.handle = 34,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid34,
		.value = (uint8_t []){0x02,0x23,0x00,0x3B,0xFB,0x67,0x52,0x87,0x8F,0x54,0x84,0x9C,0x4D,0xBE,0x77,0xDD,0xDF,0xC3,0x42},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[34])
	},
	// Characteristic Value: SUOTA_PATCH_DATA_CHAR_SIZE
	[35] =
	{
		.handle = 35,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x02,
		.uuid = (uint8_t []){0x3B,0xFB,0x67,0x52,0x87,0x8F,0x54,0x84,0x9C,0x4D,0xBE,0x77,0xDD,0xDF,0xC3,0x42},
		.value = (uint8_t []){0xF0,0x00},
		.notify_write = 1,
		.notify_read = 0,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[35])
	},
	// Characteristic Declaration: SUOTA_MTU
	[36] =
	{
		.handle = 36,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid36,
		.value = (uint8_t []){0x02,0x25,0x00,0x3C,0xE2,0xFC,0x3D,0x90,0xC4,0xAF,0xA3,0xBB,0x43,0x3D,0x82,0xEA,0x1E,0xDE,0xB7},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[36])
	},
	// Characteristic Value: SUOTA_MTU
	[37] =
	{
		.handle = 37,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x02,
		.uuid = (uint8_t []){0x3C,0xE2,0xFC,0x3D,0x90,0xC4,0xAF,0xA3,0xBB,0x43,0x3D,0x82,0xEA,0x1E,0xDE,0xB7},
		.value = (uint8_t []){0x17,0x00},
		.notify_write = 1,
		.notify_read = 0,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[37])
	},
	// Characteristic Declaration: SUOTA_CAP_L2CAP_PSM
	[38] =
	{
		.handle = 38,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_ADOPTED,
		.value_length = 0x13,
		.uuid = (uint8_t *)&qe_uuid38,
		.value = (uint8_t []){0x02,0x27,0x00,0x2A,0xBB,0xBE,0x19,0x34,0x5C,0x6E,0x94,0x65,0x47,0x39,0xF6,0x9C,0x84,0xC8,0x61},
		.notify_write = 1,
		.notify_read = 1,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[38])
	},
	// Characteristic Value: SUOTA_CAP_L2CAP_PSM
	[39] =
	{
		.handle = 39,
		.encapsulated_attributes = 0,
		.permissions = 0x01,
		.uuid_length = QE_BLE_PROFILE_UUID_SIZE_CUSTOM,
		.value_length = 0x02,
		.uuid = (uint8_t []){0x2A,0xBB,0xBE,0x19,0x34,0x5C,0x6E,0x94,0x65,0x47,0x39,0xF6,0x9C,0x84,0xC8,0x61},
		.value = (uint8_t []){0x00,0x00},
		.notify_write = 1,
		.notify_read = 0,
		.read_indication = 0,
		.p_write_cmd_cb = &(qe_write_cmd_cb_array[39])
	}

};
