# FreeRTOS NativeAOT - Design Decisions and Architecture

This document records the key architectural decisions, trade-offs, and technical considerations made during the FreeRTOS NativeAOT implementation.

## Table of Contents

1. [Cross-Compilation Strategy](#cross-compilation-strategy)
2. [Type System Compatibility](#type-system-compatibility)
3. [Platform Abstraction Layer](#platform-abstraction-layer)
4. [Memory Management](#memory-management)
5. [Threading Model](#threading-model)
6. [Assembly Helper Strategy](#assembly-helper-strategy)
7. [Exception Handling](#exception-handling)
8. [Feature Exclusions](#feature-exclusions)
9. [Build System Design](#build-system-design)
10. [Future Extensibility](#future-extensibility)

## Cross-Compilation Strategy

### Decision: HOST vs TARGET Separation

**Rationale**: Building on Windows (x64) for ARM bare-metal requires clear distinction between:
- **HOST**: Windows x64 build machine
- **TARGET**: FreeRTOS ARM32 bare-metal device

**Implementation**:
```cmake
# Check TARGET platform, not HOST platform
if(CLR_CMAKE_TARGET_FREERTOS)
    # FreeRTOS-specific configuration
elseif(CLR_CMAKE_TARGET_UNIX)
    # Unix configuration
else()
    # Windows configuration
endif()
```

**Key Insight**: Many files incorrectly used `HOST_WINDOWS` checks where they should use `TARGET_WINDOWS`. This caused Windows-specific code (Windows.h includes, Win32 APIs) to be included when cross-compiling for FreeRTOS from Windows.

**Files Fixed**:
- `src/native/minipal/cpuid.h` - Changed `HOST_WINDOWS` to `TARGET_WINDOWS || (HOST_WINDOWS && !TARGET_FREERTOS)`
- `src/native/minipal/time.c` - Added TARGET checks before HOST checks
- `src/native/minipal/guid.c` - Similar pattern
- `src/coreclr/minipal/CMakeLists.txt` - Check target OS first

**Alternative Considered**: Could have required a Linux host for cross-compilation, but this would reduce accessibility for Windows developers.

### Decision: ARM GNU Toolchain (arm-none-eabi)

**Rationale**:
- Industry standard for ARM Cortex-M bare-metal development
- Well-tested newlib-nano C library
- Excellent code generation for ARM
- Wide hardware support

**Alternatives Considered**:
- **LLVM/Clang**: Modern, but less mature newlib-nano support
- **IAR EWARM**: Proprietary, licensing concerns
- **Keil MDK**: Windows-only, proprietary

**Trade-off**: GNU toolchain has slower compile times than LLVM, but better bare-metal ecosystem maturity.

## Type System Compatibility

### Decision: ARM32 Type Definitions

**Problem**: ARM32 with GCC defines types differently than Windows or Unix:
- `int32_t` = `long int` (not `int`)
- `uint32_t` = `unsigned long int` (not `unsigned int`)
- `size_t` = `unsigned int` (not `unsigned long`)

This causes template deduction failures and function overload ambiguities.

**Solution 1**: Interlocked Template Overloads

Added explicit overloads accepting `int32_t` volatile pointers with `int` arguments:

```cpp
// gcenv.interlocked.h
#if defined(TARGET_ARM) && !defined(_MSC_VER)
static int32_t CompareExchange(int32_t volatile *destination, int exchange, int comparand);
static int32_t Exchange(int32_t volatile *destination, int value);
static int32_t ExchangeAdd(int32_t volatile *addend, int value);
#endif
```

**Alternative Considered**: Could have used `static_cast<int32_t>` at all call sites, but this would require changes throughout the codebase and reduce type safety.

**Solution 2**: std::min/max Overloads

Template deduction for `std::min<T>` fails when mixing `unsigned int` and `unsigned long`:

```cpp
// gcenv.base.h - Added to std namespace to take precedence
#if defined(TARGET_ARM) && !defined(_MSC_VER)
namespace std {
    inline unsigned int min(unsigned int a, unsigned long b) { return a < b ? a : (unsigned int)b; }
    inline unsigned long min(unsigned long a, unsigned int b) { return a < b ? a : b; }
    inline unsigned int max(unsigned int a, unsigned long b) { return a > b ? a : (unsigned int)b; }
    inline unsigned long max(unsigned long a, unsigned int b) { return a > b ? a : b; }
}
#endif
```

**Why in std namespace**: Putting overloads in global namespace didn't work; `std::min` template still took precedence. Putting them in `std` namespace makes them visible during overload resolution.

**Trade-off**: Pollutes std namespace, but necessary for compatibility without extensive codebase changes.

### Decision: pthread Stubs for Bare-Metal

**Problem**: `EEThreadId` class uses `pthread_t`, `pthread_self()`, `pthread_equal()` which are not available in bare-metal newlib-nano without `_POSIX_THREADS` defined.

**Solution**: Provide minimal pthread stubs directly in `gcenv.structs.h`:

```cpp
#if defined(TARGET_FREERTOS) && !defined(_POSIX_THREADS)
typedef uint32_t pthread_t;
inline pthread_t pthread_self(void) { return 1; }
inline int pthread_equal(pthread_t t1, pthread_t t2) { return t1 == t2; }
#endif
```

**Location Choice**: Put stubs immediately before first use in `gcenv.structs.h` rather than in `pal.h` or `palrt.h`:
- Avoids redefinition errors if real pthread.h is later included
- Minimal scope - only visible where needed
- Self-contained with the class that uses it

**Alternative Considered**: Could have avoided pthread entirely and used native types, but this would require modifying `EEThreadId` class and all its usages.

**Future Work**: When threading is implemented (Phase 6), these stubs will be replaced with proper FreeRTOS task handle integration.

## Platform Abstraction Layer

### Decision: Separate freertos/ Directory

**Rationale**: Keep FreeRTOS-specific PAL code isolated from Windows and Unix implementations.

**Structure**:
```
src/coreclr/nativeaot/Runtime/
├── windows/          # Windows PAL
├── unix/             # Unix/Linux/macOS PAL
└── freertos/         # FreeRTOS bare-metal PAL
    ├── PalFreeRTOS.h
    ├── PalFreeRTOS.cpp
    └── NativeContext.h
```

**Alternative Considered**: Could have reused unix/ directory with conditional compilation, but this would make the code harder to understand and maintain.

### Decision: Stub Implementation for Phase 3

**Rationale**: Implement minimal stubs to get C/C++ compilation working, then fill in functionality in later phases.

**Example** - `mutex.c`:
```c
#if defined(TARGET_FREERTOS)
    // Stub for single-threaded runtime
    mtx->_impl.placeholder = 0;
    return true;
#endif
```

**Trade-off**: Runtime cannot be used yet, but allows incremental development and testing of compilation infrastructure.

**Future Work**: Replace stubs with FreeRTOS APIs in Phase 6 (Threading).

## Memory Management

### Decision: No Virtual Memory Abstraction

**Problem**: Bare-metal systems don't have virtual memory, paging, or memory protection.

**Solution**: Map `PalVirtualAlloc()` directly to heap allocation:
```cpp
void* PalVirtualAlloc_FreeRTOS(size_t size) {
    return pvPortMalloc(size);  // FreeRTOS heap allocation
}
```

**Implications**:
1. **No memory protection**: Invalid accesses cause hard faults
2. **No commit/reserve**: All memory is committed on allocation
3. **No guard pages**: Stack overflow detection must use FreeRTOS mechanisms
4. **Fixed heap size**: Configured at compile time in `FreeRTOSConfig.h`

**Alternative Considered**: Could have implemented a software-based virtual memory system, but this would be complex and wasteful on resource-constrained MCUs.

**Best Practice**: Users should:
- Reserve large contiguous region for GC heap at startup
- Use FreeRTOS heap_5 for multiple memory regions (external SDRAM)
- Configure `configCHECK_FOR_STACK_OVERFLOW` for overflow detection

### Decision: Disable StressLog

**Problem**: StressLog uses floating-point arithmetic and `va_list` processing not suitable for bare-metal.

**Solution**: Added `NO_STRESS_LOG` definition and comprehensive stubs in `stressLog.h`:

```cpp
#define NO_STRESS_LOG
// ... in CMakeLists.txt

// stressLog.h
#else // !STRESS_LOG
// Define log level and facility constants as dummies
#define LL_INFO10000    7
// ...
enum LogFacilitiesEnum: unsigned int {
    LF_ALWAYS = 0x80000000u,
    // ...
};
// Stub out all STRESS_LOG macros
#define STRESS_LOG0(facility, level, msg)  do { } WHILE_0
// ...
#endif
```

**Why Comprehensive Stubs**: Many files use `STRESS_LOG*` macros unconditionally, and some use log facility constants (`LF_ALWAYS`, etc.) in other contexts.

**Alternative Considered**: Could have ported StressLog to use integer-only formatting, but this would be significant effort for a debug-only feature.

**Future Enhancement**: Consider implementing lightweight bare-metal logging using UART and integer-only formatting.

## Threading Model

### Decision: Single-Threaded Runtime for Phase 3

**Rationale**:
- Simplifies initial implementation
- Allows focus on core runtime functionality
- Many embedded applications are single-threaded

**Implications**:
- No concurrent GC
- No thread synchronization primitives
- No async/await support (requires threading)
- All managed code runs on one FreeRTOS task

**Future Work (Phase 6)**:
1. Map NativeAOT `Thread` to FreeRTOS `TaskHandle_t`
2. Implement GC suspension across tasks
3. Use FreeRTOS semaphores/mutexes for synchronization
4. Map Thread Local Storage to FreeRTOS task storage

**Design Consideration**: When threading is added, must handle:
- **GC suspension**: Suspend all tasks during GC
- **Stack scanning**: Scan stacks of all tasks
- **Synchronization**: Map `Monitor`, `Mutex`, `Semaphore` to FreeRTOS primitives
- **Task scheduling**: Coordinate with FreeRTOS scheduler

## Assembly Helper Strategy

### Decision: Exclude Assembly Files in Phase 3

**Rationale**: ARM assembly files use Linux-specific macros (`prolog_push`, `prolog_vpush`, `.att_syntax`) that don't work with bare-metal assembler.

**Implementation**:
```cmake
# CMakeLists.txt
if(NOT CLR_CMAKE_TARGET_FREERTOS)
  list(APPEND RUNTIME_SOURCES_ARCH_ASM
    ${ARCH_SOURCES_DIR}/ExceptionHandling.${ASM_SUFFIX}
    ${ARCH_SOURCES_DIR}/GcProbe.${ASM_SUFFIX}
    # ... other .S files
  )
endif()
```

**Files Excluded** (9 total):
1. AllocFast.S
2. ExceptionHandling.S
3. GcProbe.S
4. MiscStubs.S
5. PInvoke.S
6. InteropThunksHelpers.S
7. StubDispatch.S
8. UniversalTransition.S
9. WriteBarriers.S

**Phase 4 Strategy**: Reimplement these using bare-metal compatible assembly:

```asm
// Example: Write Barrier (bare-metal style)
.syntax unified
.thumb
.global JIT_WriteBarrier

JIT_WriteBarrier:
    // ARM Cortex-M implementation
    // No prolog_push macro - use direct push
    push    {r0-r3, lr}
    // ... implementation
    pop     {r0-r3, pc}
```

**Challenges**:
1. **No DWARF unwinding**: Must use simplified unwinding or FreeRTOS context
2. **Different calling convention**: Bare-metal may have different register usage
3. **No OS exception handlers**: Hardware faults go to ARM exception vectors
4. **Testing**: Requires hardware or QEMU for validation

**Priority Order**:
1. **WriteBarriers.S** - Critical for GC correctness
2. **GcProbe.S** - GC suspension points
3. **AllocFast.S** - Performance critical
4. Others as needed

### Decision: Exclude AsmOffsetsVerify.cpp

**Problem**: Verifies that assembly offset constants match C++ structure layout. Fails when assembly files are not built.

**Solution**: Conditional exclusion:
```cmake
if(NOT CLR_CMAKE_TARGET_FREERTOS)
    list(APPEND FULL_RUNTIME_SOURCES AsmOffsetsVerify.cpp)
endif()
```

**Future**: Re-enable when assembly helpers are implemented and offsets can be verified.

## Exception Handling

### Decision: Defer Hardware Exception Integration to Phase 5

**Rationale**:
- Complex integration with ARM Cortex-M exception model
- Requires assembly helper support (Phase 4) first
- Separate phase allows focused development

**ARM Cortex-M Exception Model**:
```
Exception Vectors (in flash/RAM):
  0x00: Initial SP
  0x04: Reset_Handler
  0x08: NMI_Handler
  0x0C: HardFault_Handler
  0x10: MemManage_Handler
  0x14: BusFault_Handler
  0x18: UsageFault_Handler
  ...
```

**Hardware Exception Frame** (pushed by processor):
```
SP → R0, R1, R2, R3, R12, LR, PC, xPSR (8 words)
```

**Phase 5 Design**:
1. Install NativeAOT exception handlers in vector table
2. Capture hardware exception frame to `CONTEXT` structure
3. Call managed exception handling (`RhThrowHwEx`, etc.)
4. Restore context and return from exception (or terminate)

**Trade-off**: No managed exception handling in Phase 3, but hardware faults can still be caught and debugged using standard ARM debugging tools.

## Feature Exclusions

### Decision: Exclude Debug-PAL, Unwinder, Interop

**Rationale**: These components have hard dependencies on Windows or POSIX that aren't applicable to bare-metal:

```cmake
# src/coreclr/CMakeLists.txt
if(NOT CLR_CMAKE_TARGET_FREERTOS)
  if(CLR_CMAKE_TARGET_WIN32 OR CLR_CMAKE_TARGET_UNIX)
    add_subdirectory(debug/debug-pal)
  endif()
  add_subdirectory(unwinder)  # Requires DWARF/SEH unwinding
  add_subdirectory(interop)   # Requires COM, WinRT
endif()
```

**Implications**:
- **debug-pal**: Debugging via hardware debugger (OpenOCD, J-Link) instead
- **unwinder**: Stack unwinding via simplified mechanism or FreeRTOS context
- **interop**: No COM/WinRT on bare-metal (not needed)

**Alternative**: Could have stubbed these out, but complete exclusion is cleaner and reduces binary size.

### Decision: Exclude DebugHeader.cpp

**Problem**: `DebugHeader.cpp` directly uses `StressLogChunk` and `StressMsg` types which don't exist when `NO_STRESS_LOG` is defined.

**Solution**: Conditional exclusion:
```cmake
if(CLR_CMAKE_TARGET_FREERTOS)
    list(REMOVE_ITEM COMMON_RUNTIME_SOURCES DebugHeader.cpp)
endif()
```

**Trade-off**: Loses some debug information embedding, but this is primarily for development debugging which can use hardware debugger instead.

## Build System Design

### Decision: CMake-based Build System

**Rationale**:
- Consistent with existing dotnet/runtime build system
- Good cross-compilation support
- IDE integration (VS Code, CLion)

**Key CMake Variables**:
```cmake
CLR_CMAKE_TARGET_FREERTOS=1    # Enable FreeRTOS target
TARGET_FREERTOS=1              # Preprocessor define
FEATURE_NATIVEAOT_FREERTOS=1   # Feature flag
NO_STRESS_LOG=1                # Disable StressLog
```

**Toolchain File Pattern**:
```cmake
# arm-none-eabi-gcc.cmake
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
```

**Alternative Considered**: Makefile-based system would be simpler but less portable and harder to integrate with IDEs.

### Decision: Separate Build Configuration

**Structure**:
```
artifacts/
├── obj/coreclr/
│   ├── windows.x64.Debug/     # Windows builds
│   ├── linux.x64.Debug/       # Linux builds
│   └── freertos.arm.Debug/    # FreeRTOS builds
└── bin/coreclr/
    └── freertos.arm.Debug/
        └── nativeaot/Runtime/Full/
            ├── libRuntime.WorkstationGC.a
            └── libRuntime.ServerGC.a
```

**Rationale**: Separate directories prevent cross-contamination between host and target builds.

## Future Extensibility

### Multi-Architecture Support

**Current**: ARM32 (Cortex-M) only

**Future Targets**:
- **ARM64** (Cortex-A for larger FreeRTOS systems)
- **RISC-V** (growing embedded market)
- **Xtensa** (ESP32 chips)

**Design**: Architecture-specific code in subdirectories:
```
freertos/
├── arm/          # ARM32 Cortex-M
├── arm64/        # ARM64 Cortex-A
├── riscv/        # RISC-V
└── common/       # Shared code
```

### RTOS Portability

**Current**: FreeRTOS-specific

**Potential Future RTOSes**:
- **Zephyr** - Linux Foundation RTOS
- **Azure RTOS** (ThreadX) - Microsoft RTOS
- **QNX Neutrino** - Real-time UNIX
- **VxWorks** - Industrial RTOS

**Design Consideration**: Could create generic "RTOS PAL" abstraction:
```
Runtime/
├── rtos/             # Generic RTOS abstraction
│   └── freertos/     # FreeRTOS implementation
│   └── zephyr/       # Zephyr implementation
│   └── threadx/      # ThreadX implementation
```

**Trade-off**: Abstraction adds complexity; only create when second RTOS is supported.

### Memory Model Options

**Current**: Single heap, no memory protection

**Future Options**:
1. **MPU Support**: Use ARM Cortex-M MPU for memory protection
2. **External SDRAM**: Large heap in external memory
3. **Hybrid Model**: Critical data in internal SRAM, bulk data in external SDRAM
4. **Multi-Heap**: Separate heaps for different purposes

**Implementation**: Configure via `FreeRTOSConfig.h` and linker script.

## Lessons Learned

### 1. HOST vs TARGET Distinction Critical

Many cross-compilation issues stemmed from code checking `HOST_*` instead of `TARGET_*`. Always check target platform first:

```cpp
#if defined(TARGET_WINDOWS) || (defined(HOST_WINDOWS) && !defined(TARGET_FREERTOS))
    // Windows-specific code
#elif defined(TARGET_FREERTOS)
    // FreeRTOS-specific code
#else
    // Unix/POSIX code
#endif
```

### 2. ARM32 Type System Requires Care

Template metaprogramming fails with ARM32 type definitions. Always test with ARM GCC, not just MSVC or Clang.

### 3. Incremental Implementation Works Well

Phased approach (build system → types → PAL → assembly → exceptions → threading) allowed:
- Early validation of approach
- Parallel work on different phases
- Clear progress milestones

### 4. Comprehensive Stubs Essential

Stubbing out unavailable features with proper signatures and constants (e.g., StressLog) prevents cascade of compilation errors.

### 5. Documentation Critical for Bare-Metal

Embedded developers need clear guidance on memory configuration, build integration, and hardware requirements. Extensive documentation is not optional.

## References

### ARM Architecture
- [ARM Architecture Reference Manual](https://developer.arm.com/documentation/ddi0406/latest/)
- [AAPCS - Procedure Call Standard](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst)
- [Cortex-M Programming Guide](https://developer.arm.com/documentation/den0042/latest/)

### FreeRTOS
- [FreeRTOS Developer Documentation](https://www.freertos.org/Documentation/)
- [FreeRTOS API Reference](https://www.freertos.org/a00106.html)
- [FreeRTOS Memory Management](https://www.freertos.org/a00111.html)

### NativeAOT
- [NativeAOT Overview](https://github.com/dotnet/runtime/blob/main/docs/design/coreclr/botr/README.md)
- [Runtime Architecture](https://github.com/dotnet/runtime/tree/main/docs/design/coreclr/botr)
- [GC Design](https://github.com/dotnet/runtime/blob/main/docs/design/coreclr/botr/garbage-collection.md)

### Build Systems
- [CMake Documentation](https://cmake.org/documentation/)
- [CMake Cross Compiling](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)

---

Last Updated: 2026-02-13
Document Version: 1.0
Authors: NativeAOT FreeRTOS Team
