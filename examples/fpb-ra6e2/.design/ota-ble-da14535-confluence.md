> Source: https://confluence.eng.renesas.com/spaces/PESKB/pages/457591511/OTA+Firmware+Updates+over+BLE+using+DA14535 — Confluence version 2 (last modified 2026-08-18), downloaded 2026-09-02.
>
> Lines wrapped in `<mark>` (yellow) are the security-related requirements catalogued in [ota-security-requirements.md](ota-security-requirements.md) as MRK-SEC-01 … MRK-SEC-06.

# OTA Firmware Updates over BLE using DA14535

# Executive Summary

MCU Firmware Update Over the Air with Bluetooth® Low Energy (LE) wireless communication Pmod™ Board US159-DA14535EVZ/DA14531EVZ module, utilizing Renesas SUOTA Service of the Bluetooth LE DA14535/DA14531 module.

# Market Opportunity

BLE is widely available on smartphones and tablets, making it a practical local connection for products that do not have Wi-Fi, Ethernet, or cellular connectivity. The lack of requirement for running the networking stack or complex protocols on the MCU for an OTA update makes OTA updates over BLE an ideal solution of resource constrained MCUs.

A BLE-based update method can provide the following customer benefits:

- No requirement for BLE/networking stack running on the MCU.
- No cumbersome protocols to connect to cloud.
- No need for the product to contain cloud credentials.
- No local network configuration.
- No dedicated programming cable or debugger.
- Compatibility with mobile field-service workflows.
- Lower hardware cost than adding a higher-power network interface.

## Targeted Use Cases in End Applications

### Smart Home

- Thermostats
- Sensors
- Security systems
- Smart plugs
- Smart lighting
- Smart locks

### Industrial/Consumer HMI

- Asset tracking tags, sensors
- Condition monitoring sensors
- Building automation controllers
- Gateway devices

### Consumer Appliances

- Coffee makers
- Washers and dryers
- Refrigerators

### Medical Devices

- Wearables
- Patient controls and monitors
- Therapy devices

# Intended Audience

Embedded developers with knowledge of Renesas FSP modules and experience using e2studio IDE.

# Objective

A manual or physical firmware update on IoT device is fraught with many issues

- High maintenance cost
- Slow deployment of corrections
- Poor end-user experience
- Complex wireless integration
- Fragmented updates due to memory constraints

The proposed solution addresses these problems by providing an end-to-end reference architecture for updating the RA6E2 through the DA14531.

The reference design provides a reusable OTA firmware update solution that:

- Updates RA4E2 application firmware through a BLE connection.
- Uses the DA14535/DA14531 as the BLE peripheral and communication bridge.
- Uses an Android or iOS mobile application to initiate and monitor updates.
- Protects the device against invalid, corrupted, <mark>unauthorized</mark>, or interrupted updates.
- Provides a reference implementation that customers can adapt to their own RA MCU products.
- Reduces customer development time and technical risk when adding BLE-based firmware update capability.

# Project Scope

The solution includes

- Firmware update framework for the MCU
- BLE host communication
- Mobile application that supports SUOTA for DA14535/14531
- Firmware packaging for SUOTA compatibility
- Image validation
- Error handling

## Hardware Requirements

- FPB-RA4E2
- DA14535 pmod - US159-DA14535EVZ/DA14531EVZ

## Software Requirements

- e2studio
- FSP v6.5.0, RAFW FSP
- FreeRTOS - This project is considered a small portion of the complete application of MCU wi-fi connectivity to cloud. Hence the scope would be expanded to include FreeRTOS.

## Release Management

- Renesas Explorer GitHub
- Renesas Explorer Landing Page

## Lifecycle Management

Regression tested, updated and published every FSP release

## Functional Specifications

High level functional requirements

### Mobile Application

The mobile application shall:

- Discover compatible devices.
- Distinguish supported products from unrelated BLE peripherals.
- <mark>Establish a secure BLE connection.</mark>
- Read device and firmware metadata.
- <mark>Import firmware from local storage or retrieve it from an approved server.</mark>
- Validate package metadata before transfer.
- Display release version and update status.
- Transfer the firmware using acknowledged blocks or another reliable transport scheme.
- Retry recoverable communication errors.
- Resume an interrupted transfer when supported by the device.
- Cancel an update safely before activation.
- Request image verification.
- Initiate installation or device reboot.
- Reconnect after reboot.
- Confirm the installed version.
- Save a non-sensitive update log.
- Provide user-readable and engineering-level error information.

### DA14531 Firmware

The DA14531 firmware shall:

- Advertise update capability.
- Implement the selected BLE GATT service.
- Provide device and update metadata.
- Receive image data from the mobile application.
- Enforce packet sequencing and flow control.
- Detect missing, duplicate, or invalid blocks.
- Forward data to the MCU through UART.
- Receive status and error information from the MCU.
- Report progress to the mobile application.
- Allow safe reconnection after a BLE interruption.
- <mark>Prevent unrestricted access to host programming commands.</mark>
- Maintain update state across BLE reconnections where feasible.
- Continue normal BLE operation after a successful RA6E2 update.
- Provide a defined update mode that can be entered through the RA6E2 application or a hardware recovery condition.

### MCU Bootloader

The MCU software shall:

- Enter firmware-update mode.
- Receive firmware metadata and payload data from the DA14531.
- Validate destination address and image length.
- Write the image to a staging area.
- Protect bootloader and immutable configuration regions.
- <mark>Verify image integrity and authenticity before activation.</mark>
- Enforce product and hardware compatibility.
- <mark>Enforce the configured version policy.</mark>
- Activate the candidate image only after successful verification.
- Detect a failed first boot.
- Confirm a successful image after startup validation.
- Roll back or enter recovery mode if the new image fails validation or startup.
- Report progress and failure causes to the DA14531.
- Preserve essential product configuration through the update.
- Provide a factory-programming and recovery path independent of BLE.

## Out of Scope

- Firmware updates to secondary device/MCU
- Cloud based OTA updates

# Deliverables: Source Code

**PES Functional Pack**

- **The pack files are intended for internal use only; shall not be distributed.**
- The core functionality of the PES shall remain in a pack.
- No new APIs shall be created.
- No QE tool shall be used.
- The PES shall be designed to be reusable with other MCUs.

**AUC Application Project**

- VCOM printing library shall be used to facilitate CLI prints and user inputs.
- The project shall only use files/folders; the pack file contents shall be included in the ***src*** folder as files or folders.
- The project shall be fully functional upon importing into e2studio, i.e., it shall include the fully configured configuration.xml file.
- The project deliverable shall also include .srec file.

# Deliverables: Documentation

- The documentation shall be delivered in ***.html*** format
- The readme.txt shall provide a link to the specific AUC section in the .html file
- The documentation shall include the following:
  - Description of the project
  - Any design flow diagrams as needed
  - Steps for running the application
  - Directions for MCU bootloader changes
  - Directions for reusing the PES with other MCUs

# References

- <https://www.renesas.com/en/document/apn/rx-family-renesas-firmware-update-over-air-fota-sample-program-bluetooth-low-energy-da14535da14531?r=25565483>
- <https://lpccs-docs.renesas.com/Tutorial_SDK6/suota_overview.html>
