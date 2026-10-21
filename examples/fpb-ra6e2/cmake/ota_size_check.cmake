# Fails the build if the OTA payload does not fit the bootloader's primary slot.
#   cmake -DBIN=<file> -DSLOT_SIZE=<bytes, decimal or 0x hex> -P cmake/ota_size_check.cmake
# CMake port of the PowerShell size guard in makefile.targets (e2studio build).

file(SIZE "${BIN}" _size)
math(EXPR _slot "${SLOT_SIZE}")
math(EXPR _pct_x10 "(${_size} * 1000) / ${_slot}")
math(EXPR _pct_int "${_pct_x10} / 10")
math(EXPR _pct_frac "${_pct_x10} % 10")

get_filename_component(_name "${BIN}" NAME)
if(_size GREATER _slot)
    message(FATAL_ERROR "${_name} is ${_size} bytes and does not fit its ${_slot} byte slot. "
                        "Check the FLASH_LENGTH override in script/ota.lld / script/ota.ld.")
endif()
message("${_name}: ${_size} of ${_slot} bytes used (${_pct_int}.${_pct_frac} percent)")
