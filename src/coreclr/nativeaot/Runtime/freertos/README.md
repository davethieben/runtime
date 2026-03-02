# FreeRTOS Platform Abstraction Layer (PAL)

This directory contains the Platform Abstraction Layer (PAL) implementation for FreeRTOS bare-metal RTOS support in NativeAOT.

## Overview

The FreeRTOS PAL provides the bridge between the NativeAOT runtime and FreeRTOS APIs, enabling managed .NET code to run on ARM Cortex-M microcontrollers in bare-metal environments.

## Files

### PalFreeRTOS.h
Platform abstraction declarations for FreeRTOS:
- Memory allocation wrappers (no virtual memory on bare-metal)
- Threading primitives mapped to FreeRTOS tasks
- Synchronization using FreeRTOS semaphores and event groups
- System information APIs
- Debug output via UART

### PalFreeRTOS.cpp
Implementation of FreeRTOS PAL functions.

**Current Status**: Basic stubs implemented for single-threaded runtime. Full threading support pending.

**Key Functions**:
- `PalVirtualAlloc_FreeRTOS()` - Maps to heap allocation (no virtual memory)
- `PalVirtualFree_FreeRTOS()` - Maps to heap deallocation
- `PalCreateThread_FreeRTOS()` - Creates FreeRTOS tasks (stub)
- `PalSleep_FreeRTOS()` - Delays using `vTaskDelay()`
- `PalGetCurrentThreadId_FreeRTOS()` - Returns FreeRTOS task handle
- Synchronization primitives using FreeRTOS semaphores

### asmmacros.inc (Phase 4)
FreeRTOS-specific assembly macro overrides for cross-compilation.

**Purpose**: Fixes assembly macro includes that fail during x64→ARM32 cross-compilation:
- Force-includes ARM macros (bypassing `HOST_ARM` check in `unixasmmacros.inc`)
- Overrides `INLINE_GETTHREAD` for emulated TLS (bare-metal has no ELF TLS)
- Overrides `GLOBAL_LABEL` to handle GAS quoted-string argument parsing
- Overrides `INLINE_GET_TLS_VAR` and `INLINE_GET_ALLOC_CONTEXT_BASE`

**Key Design**: Uses `.purgem`/`.macro` pattern to override macros defined in `unixasmmacrosarm.inc`.

### unixasmmacros.inc (Phase 4)
Include-path shim that redirects `#include <unixasmmacros.inc>` to `asmmacros.inc`.

**How It Works**: The `freertos/` directory is listed before `unix/` in include paths. When assembly files do `#include <unixasmmacros.inc>`, this shim is found first and redirects to the FreeRTOS-specific `asmmacros.inc` instead of the unix version.

### NativeContext.h
ARM Cortex-M native context structure for FreeRTOS.

**Purpose**: Provides a wrapper around the ARM `CONTEXT` structure for:
- Exception handling
- Stack unwinding
- GC stack scanning
- Thread hijacking (for GC suspension)

**Current Implementation**: ARM32 (Cortex-M) only
- Maps to `CONTEXT` structure in `pal.h`
- Provides register accessors (R0-R12, SP, LR, PC)
- Implements `GetIp()`, `SetIp()`, `GetSp()`, `SetSp()` for stack walking
- Implements `SetArg0Reg()`, `SetArg1Reg()` for argument passing
- Implements `ForEachPossibleObjectRef()` for GC scanning

**TODO**: Add ARM64 (Cortex-A) support when needed

## Architecture Support

### Current
- ✅ **ARM32** (Cortex-M series: M0, M0+, M3, M4, M7, M33)
  - Thumb-2 instruction set
  - AAPCS calling convention
  - Hardware divide support (M3+)
  - Optional FPU (M4, M7)

### Future
- ⏳ **ARM64** (Cortex-A series for larger FreeRTOS systems)
- ⏳ **RISC-V** (if FreeRTOS RISC-V support added)

## Integration with FreeRTOS

### Memory Management

NativeAOT runtime requires contiguous memory regions for the GC heap. On bare-metal systems:

1. **Heap Configuration**: Configure FreeRTOS heap size in `FreeRTOSConfig.h`:
   ```c
   #define configTOTAL_HEAP_SIZE ((size_t)(64 * 1024))  // Adjust for your MCU
   ```

2. **Heap Scheme**: Use FreeRTOS heap_4.c or heap_5.c for best compatibility
   - `heap_4.c`: Coalescence, good for NativeAOT GC
   - `heap_5.c`: Multiple memory regions (for external RAM)

3. **Memory Layout**: Reserve separate regions for:
   - FreeRTOS heap (managed allocations)
   - NativeAOT GC heap (garbage collected objects)
   - System stack (FreeRTOS tasks)

### Threading Model

**Phase 3 Status**: Single-threaded runtime only (threading stubs implemented)

**Future Threading Integration** (Phase 6):
1. Map each NativeAOT `Thread` to a FreeRTOS task
2. Use FreeRTOS task notifications for thread synchronization
3. Implement GC suspension using task suspension
4. Map Thread Local Storage to FreeRTOS task storage

### Synchronization Primitives

Mapping NativeAOT synchronization to FreeRTOS:

| NativeAOT Primitive | FreeRTOS Primitive | Status |
|---------------------|-------------------|---------|
| Event (Manual Reset) | Event Groups | Stub |
| Event (Auto Reset) | Binary Semaphore | Stub |
| Mutex | Recursive Mutex | Stub |
| Semaphore | Counting Semaphore | Stub |
| Critical Section | Task Suspend/Resume | Stub |

### Time and Delays

Map to FreeRTOS tick-based timing:
- `PalGetTickCount64()` → `xTaskGetTickCount()`
- `PalSleep()` → `vTaskDelay()`
- High-resolution timing → SysTick or hardware timer

**Current Status**: Placeholder implementations (Phase 3)

## Hardware Exception Handling

**Status**: Not implemented (Phase 5 pending)

### Cortex-M Exception Model

ARM Cortex-M processors use a hardware exception model:

1. **Exception Vectors**: Exception vector table in flash/RAM
   - Reset_Handler
   - NMI_Handler
   - HardFault_Handler
   - MemManage_Handler (M3+)
   - BusFault_Handler (M3+)
   - UsageFault_Handler (M3+)
   - SVC_Handler
   - PendSV_Handler
   - SysTick_Handler

2. **Exception Frame**: Hardware-saved context
   ```
   SP → R0, R1, R2, R3, R12, LR, PC, xPSR (8 words)
   ```

3. **Integration Strategy**:
   - Install NativeAOT handlers in vector table
   - Capture hardware exception frame
   - Convert to `CONTEXT` structure
   - Dispatch to managed exception handlers
   - Restore context and return from exception

### Required Files (Phase 5)
- `HardwareExceptions.cpp` - Exception handler implementations
- `ExceptionVectors.S` - ARM assembly exception vector table
- Integration with FreeRTOS exception handlers

## Assembly Helper Status

**Status**: ✅ All 9 assembly files compile and link (Phase 4 complete)

The existing ARM assembly files (`arm/*.S` and `runtime/arm/*.S`) are reused unchanged. Cross-compilation issues are fixed via include-path shims in this directory (`asmmacros.inc`, `unixasmmacros.inc`). See the [Phase 4 Completion Report](../../../../docs/design/coreclr/freertos/freertos-phase4-plan.md) for details.

**Assembly Files (all compiling)**:
1. WriteBarriers.S - GC write barriers and card table updates
2. AllocFast.S - Fast path object/array allocation
3. StubDispatch.S - Interface dispatch stubs
4. GcProbe.S - GC suspension points
5. PInvoke.S - Managed-to-native transitions
6. ExceptionHandling.S - Exception dispatch and funclet calls
7. UniversalTransition.S - Universal transition thunks
8. MiscStubs.S - Stack probing
9. InteropThunksHelpers.S - Interop common stub

## Testing

### Unit Testing
Run runtime tests targeting FreeRTOS:
```bash
# Build runtime tests for FreeRTOS
./build-runtime.sh -os freertos -arch arm -c Debug -subset clr.runtime+clr.nativeaot.runtime

# Tests require hardware or QEMU for execution
```

### Hardware Testing
Test on real ARM Cortex-M hardware:

1. **Development Boards**:
   - STM32F4 Discovery (Cortex-M4, 192KB RAM)
   - STM32F7 Discovery (Cortex-M7, 512KB RAM)
   - STM32H7 Nucleo (Cortex-M7, 1MB RAM)

2. **Test Application Structure**:
   ```
   ├── app/
   │   ├── main.c              # FreeRTOS initialization
   │   ├── managed_entry.cpp   # NativeAOT entry point
   │   └── FreeRTOSConfig.h    # FreeRTOS configuration
   ├── managed/
   │   └── Program.cs          # Managed C# application
   └── link.ld                 # Linker script
   ```

3. **Linker Integration**:
   Link NativeAOT libraries with application:
   ```bash
   arm-none-eabi-gcc \
     main.o managed_entry.o \
     -lRuntime.WorkstationGC \
     -lBootstrapper \
     -T link.ld \
     -o app.elf
   ```

### QEMU Testing
Emulate ARM Cortex-M with QEMU:
```bash
qemu-system-arm \
  -machine lm3s6965evb \
  -kernel app.elf \
  -semihosting \
  -nographic
```

## Debugging

### Hardware Debugging
Use OpenOCD + GDB for on-chip debugging:
```bash
# Start OpenOCD (adjust for your board)
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg

# In another terminal, connect GDB
arm-none-eabi-gdb app.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) continue
```

### Serial Console
Configure UART for debug output:
1. Implement `PalDebugPrint_FreeRTOS()` using UART
2. Redirect `printf` to UART
3. Connect serial console (115200 baud)

### Common Issues

**Stack Overflow**: Increase FreeRTOS task stack size
```c
xTaskCreate(task_func, "task", 4096, NULL, 1, NULL);  // 4KB stack
```

**Hard Fault**: Check:
- Alignment issues (ARM requires aligned access)
- Invalid memory access (outside RAM regions)
- Stack overflow
- Uninitialized pointers

**Memory Exhaustion**: Increase heap sizes in `FreeRTOSConfig.h`

## Configuration

### FreeRTOSConfig.h Requirements

Minimum required FreeRTOS configuration:

```c
// Basic configuration
#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      (168000000)  // Adjust for MCU
#define configTICK_RATE_HZ                      (1000)       // 1ms tick
#define configMAX_PRIORITIES                    (5)
#define configMINIMAL_STACK_SIZE                (512)
#define configTOTAL_HEAP_SIZE                   (64 * 1024)  // Adjust for MCU

// Required for NativeAOT
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_EVENT_GROUPS                  1

// Memory allocation
#define configSUPPORT_DYNAMIC_ALLOCATION        1

// Task management
#define configUSE_TASK_FPU_SUPPORT              1  // If using FPU

// Include FreeRTOS API
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
```

### Build Requirements

Include paths for NativeAOT with FreeRTOS:
```cmake
include_directories(
  ${FREERTOS_PATH}/include
  ${FREERTOS_PATH}/portable/GCC/ARM_CM4F  # Adjust for your MCU
  ${NATIVEAOT_PATH}/Runtime/freertos
  ${NATIVEAOT_PATH}/Runtime/inc
)
```

## Contributing

To implement missing functionality:

1. **Phase 5 (End-to-End)**: Link and run managed code on bare-metal
   - Investigate ILC ARM32 bare-metal output
   - Create linker script and startup code
   - Test on QEMU or hardware

2. **Phase 6 (Exceptions)**: Hardware exception integration
   - Study ARM Cortex-M exception model
   - Integrate with FreeRTOS exception handlers
   - Test fault handling

3. **Phase 7 (Threading)**: Multi-threading support
   - Map threads to FreeRTOS tasks
   - Implement synchronization primitives
   - Test GC suspension across tasks

## References

- [FreeRTOS API Reference](https://www.freertos.org/a00106.html)
- [ARM Cortex-M Programming Guide](https://developer.arm.com/documentation/den0042/latest/)
- [AAPCS - ARM Procedure Call Standard](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst)
- [NativeAOT Architecture](../../docs/design/coreclr/botr/ryujit-overview.md)
- [FreeRTOS NativeAOT Status](../../../../docs/design/coreclr/freertos/freertos-nativeaot-status.md)

---

Last Updated: 2026-03-01
