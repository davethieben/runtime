// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// FreeRTOS shim for unixasmmacros.inc
//
// This file overrides the standard unixasmmacros.inc for FreeRTOS bare-metal
// cross-compilation. It provides the same interface but ensures that:
//  1. ARM macros are included even when HOST_ARM is not defined (cross-compile)
//  2. Thread-local storage uses emulated TLS (RhpGetThread) not ELF TLS
//  3. No PLT relocations (bare-metal, no dynamic linker)
//
// This file is found before unix/unixasmmacros.inc via include path ordering:
//   include_directories(freertos)  // first
//   include_directories(unix)      // second

#include "asmmacros.inc"
