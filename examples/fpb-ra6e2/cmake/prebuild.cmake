# Runs the FSP Smart Configurator (RASC) to (re)generate ra/, ra_gen/, ra_cfg/ and the
# generated linker scripts from configuration.xml. Script mode:
#
#   cmake [-DRASC_COMPILER=LLVMARM|GCC] [-DRASC_TOOLCHAIN_VERSION=x.y.z]
#         [-DRASC_BUILD_CONFIG=Debug] [-DRASC_EXE_PATH=...] -P cmake/prebuild.cmake
#
# Called automatically by CMakeLists.txt on every configure. e2studio does the same
# job through "Generate Project Content" and does not use this script.

cmake_minimum_required(VERSION 3.20)

get_filename_component(_proj_dir "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(RASC_CONFIG_FILE "${_proj_dir}/configuration.xml")

include("${_proj_dir}/Config.cmake")

if(NOT RASC_COMPILER)
    if(DEFINED ENV{RASC_COMPILER} AND NOT "$ENV{RASC_COMPILER}" STREQUAL "")
        set(RASC_COMPILER "$ENV{RASC_COMPILER}")
    else()
        set(RASC_COMPILER "LLVMARM")
    endif()
endif()

if(NOT RASC_TOOLCHAIN_VERSION)
    if(DEFINED ENV{RASC_TOOLCHAIN_VERSION} AND NOT "$ENV{RASC_TOOLCHAIN_VERSION}" STREQUAL "")
        set(RASC_TOOLCHAIN_VERSION "$ENV{RASC_TOOLCHAIN_VERSION}")
    elseif(RASC_COMPILER STREQUAL "LLVMARM")
        set(RASC_TOOLCHAIN_VERSION "21.1.1")
    else()
        set(RASC_TOOLCHAIN_VERSION "13.3.1")
    endif()
endif()

if(NOT RASC_BUILD_CONFIG)
    set(RASC_BUILD_CONFIG "Debug")
endif()

set(_rasc_cmd
    "${RASC_EXE_PATH}" -nosplash --generate
    --devicefamily ra
    --compiler ${RASC_COMPILER}
    --toolchainversion ${RASC_TOOLCHAIN_VERSION}
    --buildconfiguration ${RASC_BUILD_CONFIG}
    "${RASC_CONFIG_FILE}")

string(REPLACE ";" " " _rasc_cmd_str "${_rasc_cmd}")
message(STATUS "RASC: ${_rasc_cmd_str}")

# --launcher.suppressErrors is deliberately omitted so failures are reported.
execute_process(
    COMMAND ${_rasc_cmd}
    WORKING_DIRECTORY "${_proj_dir}"
    RESULT_VARIABLE _rasc_result
    OUTPUT_VARIABLE _rasc_out
    ERROR_VARIABLE  _rasc_out
    TIMEOUT 600)

# RASC exits with 1 (after generating everything) when the set of FSP component files
# changed since the last run; that is a notice, not a failure.
if(_rasc_result EQUAL 1 AND _rasc_out MATCHES "Components have been added to, or removed from the project")
    set(_rasc_result 0)
endif()

if(NOT _rasc_result EQUAL 0)
    message(FATAL_ERROR "RASC content generation failed (${_rasc_result}):\n${_rasc_out}")
endif()
if(NOT EXISTS "${_proj_dir}/ra_gen")
    message(FATAL_ERROR "RASC finished but ${_proj_dir}/ra_gen was not created:\n${_rasc_out}")
endif()
message(STATUS "RASC: project content generated")
