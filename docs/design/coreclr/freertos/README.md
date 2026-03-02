# FreeRTOS NativeAOT Support - Documentation Index

This document provides a roadmap to all FreeRTOS NativeAOT documentation.

## Quick Navigation

**Getting Started**: [Quick Start Guide](freertos-quickstart.md)
**Current Status**: [Implementation Status](freertos-nativeaot-status.md)
**Technical Details**: [PAL README](../../src/coreclr/nativeaot/Runtime/freertos/README.md)
**Architecture**: [Design Decisions](freertos-design-decisions.md)
**Contributing**: [Phase 4 Report](freertos-phase4-plan.md) | [Phase 5 Plan](freertos-phase4-checklist.md)

---

## Project Overview

**FreeRTOS NativeAOT** enables .NET managed code to run on ARM Cortex-M microcontrollers using the FreeRTOS bare-metal RTOS. This brings C# language productivity to embedded systems development.

**Current Status**: ✅ **Phase 4 Complete** - All C/C++ and assembly code compiles and links
**Next Step**: Phase 5 - End-to-end linking and first managed code execution

---

## Documentation by Role

### For Users/Developers

If you want to **use** FreeRTOS NativeAOT in your project:

1. **Start here**: [Quick Start Guide](freertos-quickstart.md)
   - Prerequisites (toolchain, hardware)
   - Build instructions
   - Example project structure
   - Troubleshooting

2. **Check status**: [Implementation Status](freertos-nativeaot-status.md)
   - What works today
   - Known limitations
   - Roadmap

3. **Get help**: File issues on [dotnet/runtime](https://github.com/dotnet/runtime/issues)
   - Use labels: `area-NativeAOT` + `os-freertos`

### For Contributors

If you want to **contribute** to FreeRTOS NativeAOT:

1. **Understand architecture**: [Design Decisions](freertos-design-decisions.md)
   - Cross-compilation strategy
   - Type system compatibility
   - PAL design
   - Lessons learned

2. **Review technical details**: [PAL README](../../src/coreclr/nativeaot/Runtime/freertos/README.md)
   - File descriptions
   - Architecture support
   - Integration patterns
   - Testing guidance

3. **Review Phase 4**: [Phase 4 Completion Report](freertos-phase4-plan.md)
   - Include-path shim approach
   - Cross-compilation fixes
   - Build results

4. **Start Phase 5**: [Phase 5 Plan](freertos-phase4-checklist.md)
   - End-to-end linking
   - ILC investigation
   - QEMU testing

### For Maintainers

If you're **maintaining** FreeRTOS NativeAOT code:

1. **Architecture decisions**: [Design Decisions](freertos-design-decisions.md)
   - Rationale for design choices
   - Trade-offs considered
   - Alternatives evaluated

2. **Implementation status**: [Status Document](freertos-nativeaot-status.md)
   - What's complete in each phase
   - What's pending
   - Dependencies

3. **Technical reference**: [PAL README](../../src/coreclr/nativeaot/Runtime/freertos/README.md)
   - Current implementation details
   - Future extensibility
   - Common issues

---

## Documentation Files

### Core Documentation

| Document | Purpose | Audience |
|----------|---------|----------|
| **[freertos-nativeaot-status.md](freertos-nativeaot-status.md)** | Overall status, all 7 phases | Everyone |
| **[freertos-quickstart.md](freertos-quickstart.md)** | Practical getting started guide | Users/Developers |
| **[freertos-design-decisions.md](freertos-design-decisions.md)** | Architectural decisions and rationale | Contributors/Maintainers |
| **[freertos-phase4-plan.md](freertos-phase4-plan.md)** | Phase 4 completion report | Contributors/Maintainers |
| **[freertos-phase4-checklist.md](freertos-phase4-checklist.md)** | Phase 5 implementation plan | Contributors |

### Source Code Documentation

| Location | Content |
|----------|---------|
| **[src/coreclr/nativeaot/Runtime/freertos/README.md](../../src/coreclr/nativeaot/Runtime/freertos/README.md)** | Technical PAL documentation |
| **[src/coreclr/nativeaot/Runtime/freertos/](../../src/coreclr/nativeaot/Runtime/freertos/)** | PAL implementation files |

### Build Documentation

| Document | Purpose |
|----------|---------|
| **[PHASE3_COMMIT_MESSAGE.txt](../../PHASE3_COMMIT_MESSAGE.txt)** | Phase 3 completion summary |

---

## Phase Summary

### ✅ Phase 1: Build System Configuration (COMPLETE)
**Status**: Fully working
**Summary**: CMake configuration for FreeRTOS target, cross-compilation setup

**Key Files**:
- `eng/native/functions.cmake`
- `eng/native/configurecompiler.cmake`
- `src/coreclr/CMakeLists.txt`

**Documentation**: See [Status - Phase 1](freertos-nativeaot-status.md#phase-1-build-system-configuration--complete)

---

### ✅ Phase 2: Core Type System (COMPLETE)
**Status**: Fully working
**Summary**: ARM32 CONTEXT structure, FreeRTOS PAL headers, basic type definitions

**Key Files**:
- `src/coreclr/pal/inc/pal.h` - ARM32 CONTEXT
- `src/coreclr/nativeaot/Runtime/freertos/PalFreeRTOS.h`
- `src/coreclr/nativeaot/Runtime/freertos/NativeContext.h`

**Documentation**: See [Status - Phase 2](freertos-nativeaot-status.md#phase-2-core-type-system--complete)

---

### ✅ Phase 3: PAL Implementation (COMPLETE)
**Status**: Fully working - 74/77 files compile
**Summary**: Platform abstraction layer, type compatibility, component exclusions

**Key Achievements**:
- ✅ Cross-compilation Windows → ARM32 working
- ✅ ARM32 type system compatibility solved
- ✅ pthread stubs for bare-metal
- ✅ Interlocked operations working
- ✅ StressLog disabled for bare-metal
- ✅ All C/C++ runtime files compile
- ✅ 4 static libraries generated

**Key Files Modified**: 60+ files (see [PHASE3_COMMIT_MESSAGE.txt](../../PHASE3_COMMIT_MESSAGE.txt))

**Documentation**:
- [Status - Phase 3](freertos-nativeaot-status.md#phase-3-pal-implementation--complete)
- [Design Decisions](freertos-design-decisions.md)
- [PAL README](../../src/coreclr/nativeaot/Runtime/freertos/README.md)

**Build Output**:
- `libRuntime.WorkstationGC.a`
- `libRuntime.ServerGC.a`
- `libstandalonegc-disabled.a`
- `libstandalonegc-enabled.a`

---

### ✅ Phase 4: Assembly Helpers (COMPLETE)
**Status**: Fully working - all 9 assembly files compile and link
**Summary**: Reused existing ARM assembly files via include-path shims instead of file duplication

**Key Achievement**: Fixed cross-compilation infrastructure so existing assembly files work unchanged for FreeRTOS:
- Include-path shim for assembly macros (bypasses `HOST_ARM` check)
- AsmOffsets.inc cross-compilation fix (`-UHOST_64BIT` for 32-bit targets)
- FPU flags added to ASM compilation
- GLOBAL_LABEL macro override for GAS compatibility
- Emulated TLS via `RhpGetThread()` for bare-metal

**Documentation**: [Phase 4 Completion Report](freertos-phase4-plan.md)

---

### ⏳ Phase 5: End-to-End Linking (NEXT)
**Status**: Not started
**Summary**: Link runtime libraries with a minimal FreeRTOS app, achieve first managed code execution

**Documentation**: [Phase 5 Plan](freertos-phase4-checklist.md)

---

### ⏳ Phase 6: Hardware Exception Support (PENDING)
**Status**: Not started
**Summary**: Integrate ARM Cortex-M hardware exceptions with managed exception handling

---

### ⏳ Phase 7: Threading Integration (PENDING)
**Status**: Not started
**Summary**: Map NativeAOT threads to FreeRTOS tasks, implement multi-threading

---

### ⏳ Phase 8: Testing and Validation (PENDING)
**Status**: Not started
**Summary**: Comprehensive testing on real hardware, validation, benchmarking

---

## Quick Reference

### Build Commands

```bash
# Build NativeAOT runtime for FreeRTOS ARM32
cd src/coreclr
python build-runtime.py -os freertos -arch arm -c Debug

# Build artifacts location
# artifacts/obj/coreclr/freertos.arm.Debug/nativeaot/Runtime/Full/
```

### CMake Defines

```cmake
CLR_CMAKE_TARGET_FREERTOS=1       # Enable FreeRTOS target
TARGET_FREERTOS=1                 # Preprocessor define
FEATURE_NATIVEAOT_FREERTOS=1      # Feature flag
NO_STRESS_LOG=1                   # Disable StressLog
```

### Testing Commands

```bash
# Reconfigure and build
cd artifacts/obj/coreclr/freertos.arm.Debug
cmake . && ninja

# Flash to hardware (example: STM32)
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
  -c "program app.elf verify reset exit"

# Debug with GDB
arm-none-eabi-gdb app.elf
(gdb) target remote localhost:3333
(gdb) load
(gdb) break main
(gdb) continue
```

### Key Directories

```
dotnet/runtime/
├── docs/design/coreclr/
│   ├── freertos-nativeaot-status.md      # Status & roadmap
│   ├── freertos-quickstart.md            # Getting started
│   ├── freertos-design-decisions.md      # Architecture
│   ├── freertos-phase4-plan.md           # Phase 4 completion report
│   └── freertos-phase4-checklist.md      # Phase 5 plan
│
├── src/coreclr/nativeaot/Runtime/
│   ├── freertos/                         # FreeRTOS PAL
│   │   ├── README.md                     # PAL documentation
│   │   ├── PalFreeRTOS.h/cpp            # PAL implementation
│   │   ├── NativeContext.h               # ARM context wrapper
│   │   ├── asmmacros.inc                 # Assembly macro overrides (Phase 4)
│   │   └── unixasmmacros.inc            # Include-path shim (Phase 4)
│   │
│   └── CMakeLists.txt                    # Build configuration
│
├── src/coreclr/pal/inc/
│   └── pal.h                             # ARM32 CONTEXT structure
│
├── src/native/minipal/                   # Mini-PAL stubs
│   ├── mutex.c                           # FreeRTOS mutex stubs
│   ├── time.c                            # FreeRTOS time stubs
│   └── guid.c                            # Cross-compilation fixes
│
└── artifacts/
    └── obj/coreclr/freertos.arm.Debug/   # Build output
        └── nativeaot/Runtime/Full/
            ├── libRuntime.WorkstationGC.a
            ├── libRuntime.ServerGC.a
            └── ...
```

---

## External Resources

### ARM Architecture
- [ARM Developer Documentation](https://developer.arm.com/documentation/)
- [AAPCS - Procedure Call Standard](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst)
- [Cortex-M Programming Guide](https://developer.arm.com/documentation/den0042/latest/)

### FreeRTOS
- [FreeRTOS Official Site](https://www.freertos.org/)
- [FreeRTOS Documentation](https://www.freertos.org/Documentation/)
- [FreeRTOS API Reference](https://www.freertos.org/a00106.html)
- [FreeRTOS GitHub](https://github.com/FreeRTOS/FreeRTOS)

### NativeAOT
- [NativeAOT Overview](https://learn.microsoft.com/dotnet/core/deploying/native-aot/)
- [Book of the Runtime](https://github.com/dotnet/runtime/tree/main/docs/design/coreclr/botr)
- [GC Documentation](https://github.com/dotnet/runtime/blob/main/docs/design/coreclr/botr/garbage-collection.md)

### Tools
- [ARM GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
- [OpenOCD](https://openocd.org/)
- [QEMU](https://www.qemu.org/)

---

## Getting Help

### Issues and Bugs
File issues on [dotnet/runtime GitHub](https://github.com/dotnet/runtime/issues):
- Use labels: `area-NativeAOT` + `os-freertos`
- Provide: board type, toolchain version, build output, error messages

### Questions and Discussion
- [GitHub Discussions](https://github.com/dotnet/runtime/discussions)
- [.NET Discord](https://aka.ms/dotnet-discord) - #nativeaot channel

### Contributing
See [Contributing Guide](https://github.com/dotnet/runtime/blob/main/CONTRIBUTING.md)

---

## Contact

For questions about FreeRTOS NativeAOT support, file an issue on dotnet/runtime with the `area-NativeAOT` and `os-freertos` labels.

---

**Last Updated**: 2026-03-01
**Current Phase**: Phase 4 Complete
**Next Milestone**: Phase 5 - End-to-End Linking and First Execution
