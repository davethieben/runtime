// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

// FreeRTOS ARM assembly macros for NativeAOT
//
// This file provides the same macros as unixasmmacros.inc but adapted for
// FreeRTOS bare-metal cross-compilation. The key differences from the unix
// version are:
//
//  1. Always includes ARM macros (the unix version checks HOST_ARM which
//     isn't defined when cross-compiling from x64)
//  2. Uses emulated TLS (FEATURE_EMULATED_TLS) since bare-metal has no
//     ELF TLS support
//  3. No PLT relocations (no dynamic linker on bare-metal)

#define INVALIDGCVALUE 0xCCCCCCCD

// This must match HwExceptionCode.STATUS_NATIVEAOT_THREAD_ABORT
#define STATUS_NATIVEAOT_THREAD_ABORT     0x43

// Bare-metal ELF: no underscore prefix, no PLT
#define C_FUNC(name) name
#define EXTERNAL_C_FUNC(name) C_FUNC(name)
#define LOCAL_LABEL(name) .L##name
#define C_PLTFUNC(name) name

.macro END_PROLOGUE
.endm

.macro SETALIAS New, Old
        .equiv \New, \Old
.endm

// Rename offsets of nested structures
#define OFFSETOF__ee_alloc_context__alloc_ptr (OFFSETOF__ee_alloc_context__m_rgbAllocContextBuffer + OFFSETOF__gc_alloc_context__alloc_ptr)
#define OFFSETOF__ee_alloc_context            OFFSETOF__Thread__m_eeAllocContext

// GC type flags
#define GC_ALLOC_FINALIZE           1
#define GC_ALLOC_ALIGN8_BIAS        4
#define GC_ALLOC_ALIGN8             8

#define G_FREE_OBJECT_METHOD_TABLE  g_pFreeObjectEEType

// Force-include ARM macros (the unix version checks HOST_ARM, which isn't
// defined when cross-compiling from x64 to ARM32)
#include "unixasmmacrosarm.inc"

// Override GLOBAL_LABEL to accept an optional second argument.
// GAS on arm-none-eabi splits "QuotedString"\MacroParam into two arguments
// (e.g., in WriteBarriers.S: GLOBAL_LABEL "RhpAssignRefAvLocation"\EXPORT_REG_NAME).
// This override concatenates both arguments when a second is provided.
.purgem GLOBAL_LABEL
.macro GLOBAL_LABEL Name, Suffix
.ifnb \Suffix
        .global C_FUNC(\Name\Suffix)
C_FUNC(\Name\Suffix):
.else
        .global C_FUNC(\Name)
C_FUNC(\Name):
.endif
.endm

// Override INLINE_GETTHREAD for FreeRTOS bare-metal.
// Bare-metal has no ELF TLS support, so we always use the emulated TLS path
// which calls RhpGetThread() - a C function that returns the current Thread*.
// This is equivalent to the FEATURE_EMULATED_TLS path in the unix macros.
//
// NOTE: This .purgem/.macro pattern overrides the macro defined in
// unixasmmacrosarm.inc. We must purge the old definition first.
.purgem INLINE_GETTHREAD
.macro INLINE_GETTHREAD
        bl C_FUNC(RhpGetThread)
.endm

.purgem INLINE_GET_ALLOC_CONTEXT_BASE
.macro INLINE_GET_ALLOC_CONTEXT_BASE
        INLINE_GETTHREAD
.endm

// Override INLINE_GET_TLS_VAR - not available on bare-metal.
// Code that needs TLS should use FEATURE_EMULATED_TLS instead.
.purgem INLINE_GET_TLS_VAR
.macro INLINE_GET_TLS_VAR Var
        // FreeRTOS: emulated TLS - load from global variable
        PREPARE_EXTERNAL_VAR_INDIRECT \Var, r0
.endm
