# CMake toolchain file: GNU Arm Embedded (arm-none-eabi-gcc) - fallback toolchain.
# Used by the VS Code / CMake build only (e2studio uses its own managed build).
#
# ARM_TOOLCHAIN_PATH = GNU Arm "bin" folder (contains arm-none-eabi-gcc). Resolution order:
#   -DARM_TOOLCHAIN_PATH=..., env ARM_GCC_TOOLCHAIN_PATH, newest known install.
# (env ARM_TOOLCHAIN_PATH is deliberately NOT used here - it points to ATfE.)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

if(NOT ARM_TOOLCHAIN_PATH AND DEFINED ENV{ARM_GCC_TOOLCHAIN_PATH} AND NOT "$ENV{ARM_GCC_TOOLCHAIN_PATH}" STREQUAL "")
    set(ARM_TOOLCHAIN_PATH "$ENV{ARM_GCC_TOOLCHAIN_PATH}")
endif()
if(NOT ARM_TOOLCHAIN_PATH)
    if(CMAKE_HOST_WIN32)
        file(TO_CMAKE_PATH "$ENV{USERPROFILE}" _home)
        file(TO_CMAKE_PATH "$ENV{LOCALAPPDATA}" _lad)
        file(GLOB _gcc LIST_DIRECTORIES true
            "${_home}/.renesas/platform/arm-gnu/*/*/bin"
            "${_lad}/Programs/Renesas/e2_studio/toolchains/gcc_arm/*/bin"
            "C:/arm-gnu-toolchain-*/bin")
    else()
        file(GLOB _gcc LIST_DIRECTORIES true
            "$ENV{HOME}/.renesas/platform/arm-gnu/*/*/bin"
            "/opt/arm-gnu-toolchain-*/bin")
    endif()
    if(_gcc)
        list(SORT _gcc COMPARE NATURAL)
        list(GET _gcc -1 ARM_TOOLCHAIN_PATH)
    endif()
endif()
if(NOT ARM_TOOLCHAIN_PATH)
    message(FATAL_ERROR "GNU Arm toolchain not found. Set ARM_GCC_TOOLCHAIN_PATH to its 'bin' folder (run setup-env.cmd / setup-env.sh).")
endif()
file(TO_CMAKE_PATH "${ARM_TOOLCHAIN_PATH}" ARM_TOOLCHAIN_PATH)
set(ARM_TOOLCHAIN_PATH "${ARM_TOOLCHAIN_PATH}" CACHE PATH "GNU Arm toolchain bin folder" FORCE)

# try_compile() projects re-read this file; forward the resolved path to them.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES ARM_TOOLCHAIN_PATH)

if(CMAKE_HOST_WIN32)
    set(_exe ".exe")
else()
    set(_exe "")
endif()

set(_p "${ARM_TOOLCHAIN_PATH}/arm-none-eabi-")
set(CMAKE_C_COMPILER   "${_p}gcc${_exe}")
set(CMAKE_CXX_COMPILER "${_p}g++${_exe}")
set(CMAKE_ASM_COMPILER "${_p}gcc${_exe}")

set(CMAKE_AR      "${_p}ar${_exe}"      CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB  "${_p}ranlib${_exe}"  CACHE FILEPATH "" FORCE)
set(CMAKE_OBJCOPY "${_p}objcopy${_exe}" CACHE FILEPATH "" FORCE)
set(CMAKE_OBJDUMP "${_p}objdump${_exe}" CACHE FILEPATH "" FORCE)
set(CMAKE_SIZE    "${_p}size${_exe}"    CACHE FILEPATH "" FORCE)

# Bare-metal: the compiler checks must not try to link an executable.
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
