# ota_fw_update_over_ble — component build definition
#
# Adds the component as a static library. The integrating project links it and gets the
# public include directory transitively; only rs_ota_fw_update_over_ble.h is meant
# to be included by application code.
#
# Target selection: exactly ONE hal/rs_ota_fw_update_over_ble_hal_<chip>.c is
# compiled. The default is the reference RA6E2 implementation; a port to another
# chip points RS_OTA_FW_UPDATE_OVER_BLE_HAL at its own HAL file.

cmake_minimum_required(VERSION 3.16)

set(RS_OTA_FW_UPDATE_OVER_BLE_HAL
    "${CMAKE_CURRENT_SOURCE_DIR}/src/hal/rs_ota_fw_update_over_ble_hal_ra6e2.c"
    CACHE FILEPATH "HAL implementation compiled into the ota_fw_update_over_ble component")

# Bootloader back-end behind hal/rs_ota_fw_update_over_ble_boot.h
# (swap_arm / swap_and_reset / confirm_and_reset / last_result).
#
# Two back-ends ship with the component:
#   src/hal/rs_ota_fw_update_over_ble_boot_rsboot.c  Renesas RA swap bootloader
#   src/hal/rs_ota_fw_update_over_ble_boot_stub.c    no bootloader (placeholder)
#
# The default here is the stub (download only, no MCU reset). A product build selects the real one, e.g.
#   -DRS_OTA_FW_UPDATE_OVER_BLE_BOOT=src/hal/rs_ota_fw_update_over_ble_boot_rsboot.c
#
# Both files are guarded by RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER, which
# is what lets the e2 studio managed build (which compiles EVERY .c in src/hal)
# end up with exactly one definition of each symbol. CMake compiles only the file
# selected above, so the definition below simply has to agree with that choice.
set(RS_OTA_FW_UPDATE_OVER_BLE_BOOT
    "${CMAKE_CURRENT_SOURCE_DIR}/src/hal/rs_ota_fw_update_over_ble_boot_stub.c"
    CACHE FILEPATH "Bootloader back-end compiled into the ota_fw_update_over_ble component")

if(RS_OTA_FW_UPDATE_OVER_BLE_BOOT MATCHES "boot_stub")
    set(RS_OTA_FW_UPDATE_OVER_BLE_USE_RS_BOOTLOADER 0)
else()
    set(RS_OTA_FW_UPDATE_OVER_BLE_USE_RS_BOOTLOADER 1)
endif()

add_library(rs_ota_fw_update_over_ble STATIC
    src/rs_ota_fw_update_over_ble.c
    src/rs_ota_fw_update_over_ble_deps.c
    src/rs_ota_fw_update_over_ble_verify.c
    ${RS_OTA_FW_UPDATE_OVER_BLE_HAL}
    ${RS_OTA_FW_UPDATE_OVER_BLE_BOOT}
)

target_compile_definitions(rs_ota_fw_update_over_ble
    PRIVATE RS_OTA_FW_UPDATE_OVER_BLE_CFG_USE_RS_BOOTLOADER=${RS_OTA_FW_UPDATE_OVER_BLE_USE_RS_BOOTLOADER}
)

target_include_directories(rs_ota_fw_update_over_ble
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/src/hal
)
