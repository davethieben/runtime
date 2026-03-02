# Phase 4: Assembly Helpers - Completion Report

## Overview

Phase 4 achieved compilation and linking of all 9 ARM assembly files for the FreeRTOS bare-metal target. Rather than creating duplicate FreeRTOS-specific assembly files (the original plan), Phase 4 took a fundamentally different approach: **fixing the cross-compilation infrastructure** so the existing assembly files work unchanged.

## Original Plan vs Actual Approach

### Original Plan (Rejected)

The original plan called for creating 9 new assembly files in `freertos/`:
- `freertos/WriteBarriers.S`
- `freertos/AllocFast.S`
- `freertos/GcProbe.S`
- etc.

Each would be a bare-metal reimplementation of the corresponding `arm/*.S` file. Estimated effort: 6-8 weeks.

### Actual Approach (Include-Path Shims)

Investigation revealed the existing assembly files already use:
- Standard GAS syntax (`.syntax unified`, `.thumb`)
- AAPCS calling conventions (compatible with bare-metal)
- No Linux-specific system calls or OS dependencies

The compilation failures were caused by **build infrastructure issues**, not incompatible assembly code:

| Issue | Root Cause | Fix |
|-------|-----------|-----|
| ARM macros not included | `HOST_ARM` check in `unixasmmacros.inc` fails for x64→ARM cross-compile | Include-path shim bypasses check |
| Wrong struct offsets | `HOST_64BIT` in `AsmOffsets.h` leaks from x64 host | Added `-UHOST_64BIT` for 32-bit targets |
| FPU instructions fail | ASM flags missing `-mfpu=fpv4-sp-d16` | Added FPU flags to toolchain |
| GLOBAL_LABEL parsing | GAS splits `"string"\param` into two args | Override macro to accept optional second arg |
| TLS not available | No ELF TLS on bare-metal | Emulated TLS via `RhpGetThread()` |

## Files Changed

### Created (2 files, ~90 lines total)

| File | Purpose | Lines |
|------|---------|-------|
| `freertos/unixasmmacros.inc` | Shim: redirects `#include <unixasmmacros.inc>` to FreeRTOS macros | 3 |
| `freertos/asmmacros.inc` | FreeRTOS macro overrides (TLS, GLOBAL_LABEL) | ~85 |

### Modified (3 files)

| File | Change |
|------|--------|
| `CMakeLists.txt` | Re-enabled assembly files, added `unix/` include dir, added `FEATURE_EMULATED_TLS=1` |
| `Full/CMakeLists.txt` | Added `-UHOST_64BIT` for 32-bit AsmOffsets.inc generation |
| `eng/common/cross/toolchain.freertos-windows.cmake` | Added `-mfloat-abi=hard -mfpu=fpv4-sp-d16` to `CMAKE_ASM_FLAGS_INIT` |

## Build Results

**38/38 ninja targets built** - zero failures.

### Assembly Files Compiled

| # | File | Location | Description |
|---|------|----------|-------------|
| 1 | WriteBarriers.S | `runtime/arm/` | GC write barriers, card table updates |
| 2 | AllocFast.S | `runtime/arm/` | Fast path object/array allocation |
| 3 | StubDispatch.S | `runtime/arm/` | Interface dispatch stubs |
| 4 | GcProbe.S | `arm/` | GC suspension points |
| 5 | PInvoke.S | `arm/` | Managed-to-native transitions |
| 6 | ExceptionHandling.S | `arm/` | Exception dispatch, funclet calls |
| 7 | UniversalTransition.S | `arm/` | Universal transition thunks |
| 8 | MiscStubs.S | `arm/` | Stack probing |
| 9 | InteropThunksHelpers.S | `arm/` | Interop common stub |

### Libraries Produced

| Library | Size | Text Symbols |
|---------|------|-------------|
| `libRuntime.WorkstationGC.a` | 5.9 MB | 1,774 |
| `libRuntime.ServerGC.a` | 8.7 MB | 2,596 |
| `libstandalonegc-disabled.a` | 60 KB | - |
| `libstandalonegc-enabled.a` | 226 KB | - |

### Key Symbols Verified

All critical runtime entry points present:
- `RhpAssignRef`, `RhpCheckedAssignRef` (write barriers)
- `RhpNewFast` (fast allocation)
- `RhpGcPoll` (GC suspension)
- `RhpPInvoke` (P/Invoke)
- `RhpThrowEx` (exceptions)
- `RhpStackProbe` (stack probing)
- `RhCommonStub` (interop)
- `RhpInterfaceDispatchSlow` (interface dispatch)
- `RhpUniversalTransition` (universal transitions)

### AsmOffsets.inc Verified

Correct 32-bit offsets generated:
- `OFFSETOF__Array__m_Length` = `0x4` (was incorrectly `0x8` before fix)
- `OFFSETOF__Thread__m_ThreadStateFlags` = `0x2c` (was incorrectly `0x40`)
- `MIN_OBJECT_SIZE` = `0xC` (12 bytes, correct for 32-bit)

## Key Design Decisions

### Why Include-Path Shims?

1. **Zero assembly code duplication** — no maintenance burden for ~2000 lines of assembly
2. **Automatic upstream compatibility** — future changes to ARM assembly files apply to FreeRTOS builds automatically
3. **Minimal surface area** — only ~90 lines of shim code to maintain
4. **Transparent** — the mechanism is well-documented and easy to understand

### Why Override Instead of Modify?

The shim overrides macros (`.purgem` + `.macro`) rather than modifying the originals because:
- Original files work correctly for all other targets (Linux ARM, Windows ARM)
- Modifications would require `#ifdef` guards that complicate the shared code
- Overrides are isolated to the FreeRTOS build path

## Lessons Learned

1. **Investigate before reimplementing**: The assembly code was already bare-metal compatible. The issues were entirely in the build infrastructure.
2. **Include path ordering is powerful**: Placing `freertos/` before `unix/` in include paths allows selective file interception without modifying originals.
3. **HOST vs TARGET leaks are pervasive**: Both `HOST_64BIT` (for offsets) and `HOST_ARM` (for macro includes) leaked incorrectly into the cross-compilation pipeline.
4. **Toolchain FLAGS_INIT quirks**: CMake only reads `_INIT` variables during initial configuration; cached values must be updated separately.

---

Created: 2026-02-13
Completed: 2026-03-01
Status: ✅ COMPLETE
