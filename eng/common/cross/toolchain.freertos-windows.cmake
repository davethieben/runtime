# CMake toolchain file for cross-compiling from Windows to FreeRTOS ARM
# This file configures CMake to use the ARM bare-metal toolchain instead of MSVC

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR arm)

# Set host architecture (Windows x64) since WIN32 won't be set with CMAKE_SYSTEM_NAME=Generic
set(CLR_CMAKE_HOST_WIN32 1)
set(CLR_CMAKE_HOST_ARCH_AMD64 1)

# Specify the cross compiler
set(TOOLCHAIN_PREFIX arm-none-eabi)
set(CMAKE_C_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
set(CMAKE_CXX_COMPILER_TARGET ${TOOLCHAIN_PREFIX})
set(CMAKE_C_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}-g++)
set(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}-gcc)
set(CMAKE_AR ${TOOLCHAIN_PREFIX}-ar)
set(CMAKE_RANLIB ${TOOLCHAIN_PREFIX}-ranlib)
set(CMAKE_OBJCOPY ${TOOLCHAIN_PREFIX}-objcopy)
set(CMAKE_OBJDUMP ${TOOLCHAIN_PREFIX}-objdump)
set(CMAKE_SIZE ${TOOLCHAIN_PREFIX}-size)

# Compiler flags for ARM Cortex-M4 (default, can be overridden)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_ASM_FLAGS_INIT "-mcpu=cortex-m4 -mthumb")

# Bare-metal linker flags
set(CMAKE_EXE_LINKER_FLAGS_INIT "-specs=nosys.specs -specs=nano.specs")

# Search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Search for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Don't try to compile a test program with the cross-compiler
# (it will fail because we don't have startup code or linker script yet)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)
set(CMAKE_ASM_COMPILER_WORKS 1)

# Set target OS for .NET runtime (use CACHE to ensure persistence)
set(CLR_CMAKE_TARGET_OS freertos CACHE STRING "Target OS")
set(CLR_CMAKE_TARGET_FREERTOS 1 CACHE BOOL "Building for FreeRTOS")
