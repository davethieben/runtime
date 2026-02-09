# Plan: Adding FreeRTOS Support for ARM32 Architecture (NativeAOT)

This document outlines the steps required to add FreeRTOS as a new target operating system to the .NET runtime using **NativeAOT**, targeting the ARM32 CPU architecture.

## Overview

**Target OS:** FreeRTOS
**Target Architecture:** ARM32 (arm)
**Runtime Identifier (RID):** `freertos-arm`
**Runtime:** NativeAOT (ILC compiler + minimal runtime)

### Why NativeAOT for FreeRTOS?

| Aspect | NativeAOT | Mono AOT | CoreCLR |
|--------|-----------|----------|---------|
| Binary output | Single native executable | Native + IL + Mono runtime | Requires full runtime |
| Runtime size | Minimal (~few hundred KB) | Larger (MB+) | Largest |
| IL at runtime | No | Yes | Yes |
| Virtual memory required | No | Typically yes | Yes |
| Dynamic loading | No | Yes | Yes |
| Reflection | Limited | Full | Full |
| **FreeRTOS fit** | **Best** | Possible | Not feasible |

NativeAOT produces standalone native binaries with a minimal runtime statically linked in. This is ideal for FreeRTOS because:
- No IL interpretation at runtime
- Small binary footprint
- No dynamic loading requirements
- Tree shaking removes unused code
- Self-contained deployment

### Limitations Accepted

By choosing NativeAOT, we accept these limitations:
- No `Reflection.Emit` or `DynamicMethod`
- No `Assembly.Load()` at runtime
- Limited reflection (only statically analyzable)
- Serialization requires source generators
- No hot reload or plugin systems

These limitations align well with embedded firmware constraints.

---

## Phase 1: Build System Infrastructure [COMPLETED]

Build system changes have been implemented to recognize `freertos` as a valid target OS.

### Files Modified
- `eng/build.sh` - Added freertos to OS options
- `eng/build.ps1` - Added freertos to ValidateSet
- `eng/RuntimeIdentifier.props` - Added `TargetsFreertos` property
- `eng/OSArch.props` - Added FreeRTOS host OS detection
- `eng/Subsets.props` - Configured CoreCLR/NativeAOT exclusions for FreeRTOS
- `eng/versioning.targets` - Added FreeRTOS to supported platforms
- `eng/native/configureplatform.cmake` - Added FreeRTOS host/target detection
- `eng/native/configurecompiler.cmake` - Added `TARGET_FREERTOS` compile definition
- `eng/native/tryrun.cmake` - Added FreeRTOS detection and cache values
- `eng/common/cross/toolchain.cmake` - Added `arm-none-eabi` toolchain config
- `eng/common/native/init-distro-rid.sh` - Added FreeRTOS RID generation
- `eng/common/cross/build-rootfs.sh` - Added FreeRTOS rootfs setup

---

## Phase 2: Enable NativeAOT for FreeRTOS

### 2.1 Update Build Configuration

**File: `eng/Subsets.props`**
- Currently FreeRTOS is excluded from NativeAOT. Need to enable it:
```xml
<!-- Change from excluding freertos to supporting it -->
<_NativeAotSupportedOS Condition="'$(TargetOS)' != 'browser' and '$(TargetOS)' != 'haiku' ...">true</_NativeAotSupportedOS>
```

**File: `src/coreclr/nativeaot/BuildIntegration/Microsoft.NETCore.Native.targets`**
- Add FreeRTOS-specific linker configuration
- Configure for bare-metal ARM linking

### 2.2 ILC Compiler Configuration

**File: `src/coreclr/tools/aot/ILCompiler/ILCompiler.csproj`**
- Ensure ARM32 target support for FreeRTOS

**File: `src/coreclr/tools/aot/ILCompiler.Compiler/Compiler/DependencyAnalysis/Target_ARM/`**
- Verify ARM32 code generation works for bare-metal targets

---

## Phase 3: NativeAOT Runtime PAL for FreeRTOS

The NativeAOT runtime has a Platform Abstraction Layer (PAL) that must be implemented for FreeRTOS.

### 3.1 Create FreeRTOS PAL Implementation

**New File: `src/coreclr/nativeaot/Runtime/freertos/PalFreeRTOS.cpp`**

This is the main PAL implementation. Key functions to implement:

#### Memory Management
```cpp
// Map to FreeRTOS heap allocation (no virtual memory)
void* PalVirtualAlloc(uintptr_t size, uint32_t protect)
{
    // FreeRTOS: pvPortMalloc() - ignore protection flags
    return pvPortMalloc(size);
}

void PalVirtualFree(void* pAddress, uintptr_t size)
{
    vPortFree(pAddress);
}

UInt32_BOOL PalVirtualProtect(void* pAddress, uintptr_t size, uint32_t protect)
{
    // No-op on FreeRTOS (no MMU protection)
    return TRUE;
}

uint32_t PalGetOsPageSize()
{
    // Return a reasonable default for embedded
    return 4096;
}
```

#### Threading (FreeRTOS Tasks)
```cpp
bool PalStartBackgroundGCThread(BackgroundCallback callback, void* pCallbackContext)
{
    // Create FreeRTOS task for GC
    return xTaskCreate(
        (TaskFunction_t)callback,
        "GC",
        configMINIMAL_STACK_SIZE * 4,
        pCallbackContext,
        tskIDLE_PRIORITY + 1,
        NULL
    ) == pdPASS;
}

bool PalStartFinalizerThread(BackgroundCallback callback, void* pCallbackContext)
{
    return xTaskCreate(
        (TaskFunction_t)callback,
        "Finalizer",
        configMINIMAL_STACK_SIZE * 2,
        pCallbackContext,
        tskIDLE_PRIORITY + 1,
        NULL
    ) == pdPASS;
}

void PalSleep(uint32_t milliseconds)
{
    vTaskDelay(pdMS_TO_TICKS(milliseconds));
}

UInt32_BOOL PalSwitchToThread()
{
    taskYIELD();
    return TRUE;
}

uint64_t PalGetCurrentOSThreadId()
{
    return (uint64_t)xTaskGetCurrentTaskHandle();
}
```

#### Synchronization (FreeRTOS Semaphores/Events)
```cpp
HANDLE PalCreateEventW(LPSECURITY_ATTRIBUTES pEventAttributes,
                       UInt32_BOOL manualReset,
                       UInt32_BOOL initialState,
                       LPCWSTR pName)
{
    // Use FreeRTOS binary semaphore for auto-reset events
    // Use event groups for manual-reset events
    SemaphoreHandle_t sem = xSemaphoreCreateBinary();
    if (initialState)
        xSemaphoreGive(sem);
    return (HANDLE)sem;
}

UInt32_BOOL PalSetEvent(HANDLE handle)
{
    return xSemaphoreGive((SemaphoreHandle_t)handle) == pdTRUE;
}

UInt32_BOOL PalResetEvent(HANDLE handle)
{
    xSemaphoreTake((SemaphoreHandle_t)handle, 0);
    return TRUE;
}

uint32_t PalWaitForSingleObjectEx(HANDLE handle, uint32_t timeout, UInt32_BOOL alertable)
{
    TickType_t ticks = (timeout == INFINITE) ? portMAX_DELAY : pdMS_TO_TICKS(timeout);
    if (xSemaphoreTake((SemaphoreHandle_t)handle, ticks) == pdTRUE)
        return WAIT_OBJECT_0;
    return WAIT_TIMEOUT;
}
```

#### System Functions
```cpp
uint32_t PalGetCurrentProcessId()
{
    return 1; // Single "process" on FreeRTOS
}

void PalGetSystemTimeAsFileTime(FILETIME* ft)
{
    // Use FreeRTOS tick count or RTC if available
    TickType_t ticks = xTaskGetTickCount();
    uint64_t time = (uint64_t)ticks * (10000000ULL / configTICK_RATE_HZ);
    ft->dwLowDateTime = (uint32_t)time;
    ft->dwHighDateTime = (uint32_t)(time >> 32);
}

void PalPrintFatalError(const char* message)
{
    // Output to UART/debug console
    printf("FATAL: %s\n", message);
    while(1) { } // Halt
}
```

### 3.2 PAL Header Updates

**File: `src/coreclr/nativeaot/Runtime/Pal.h`**
- Add `TARGET_FREERTOS` conditionals
- Include FreeRTOS headers conditionally

```cpp
#ifdef TARGET_FREERTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "event_groups.h"
#endif
```

### 3.3 Additional FreeRTOS-Specific Files

**New File: `src/coreclr/nativeaot/Runtime/freertos/HardwareExceptions.cpp`**
- Handle ARM HardFault, MemManage, BusFault
- Map to .NET exception handling

**New File: `src/coreclr/nativeaot/Runtime/freertos/cgroupcpu.cpp`**
- Stub implementation (no cgroups on FreeRTOS)

---

## Phase 4: GC Configuration for FreeRTOS

### 4.1 Memory Constraints

FreeRTOS typically has limited RAM (64KB - few MB). Configure GC accordingly.

**File: `src/coreclr/nativeaot/Runtime/gcenv.ee.cpp`**
- Add FreeRTOS-specific GC configuration

**Recommended settings:**
```cpp
#ifdef TARGET_FREERTOS
// Small heap for embedded
#define INITIAL_HEAP_SIZE (64 * 1024)      // 64KB initial
#define MAX_HEAP_SIZE     (256 * 1024)     // 256KB max
#define GC_SEGMENT_SIZE   (16 * 1024)      // 16KB segments
#endif
```

### 4.2 GC Mode

**File: `src/coreclr/gc/gcconfig.h`**
- Force Workstation GC (no Server GC)
- Disable concurrent GC initially for simplicity
- Consider conservative GC for initial bring-up

---

## Phase 5: ARM32 Assembly Stubs

### 5.1 Context Save/Restore

**New File: `src/coreclr/nativeaot/Runtime/arm/AsmMacros_freertos.inc`**
- ARM32 assembly macros for FreeRTOS

**New File: `src/coreclr/nativeaot/Runtime/arm/GcStubs_freertos.asm`**
- GC write barriers for ARM32

### 5.2 Exception Handling

**File: `src/coreclr/nativeaot/Runtime/arm/ExceptionHandling.asm`**
- Verify/adapt for bare-metal ARM
- Integrate with FreeRTOS exception handlers

---

## Phase 6: Linker Configuration

### 6.1 Linker Script

**New File: `src/coreclr/nativeaot/BuildIntegration/FreeRTOS/link.ld`**
```ld
/* FreeRTOS linker script for NativeAOT */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 512K
    RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    .text : { *(.text*) } > FLASH
    .rodata : { *(.rodata*) } > FLASH
    .data : { *(.data*) } > RAM AT > FLASH
    .bss : { *(.bss*) } > RAM

    /* .NET specific sections */
    .managed_code : { *(.managed*) } > FLASH
    .gc_info : { *(.gc_info*) } > FLASH
}
```

### 6.2 Startup Code

**New File: `src/coreclr/nativeaot/Bootstrap/freertos/startup.cpp`**
```cpp
// Entry point for FreeRTOS NativeAOT application
extern "C" void vApplicationStackOverflowHook(TaskHandle_t task, char* name)
{
    PalPrintFatalError("Stack overflow");
}

extern "C" void vApplicationMallocFailedHook()
{
    PalPrintFatalError("Malloc failed");
}

// Called from main() after FreeRTOS scheduler starts
extern "C" int managed_main(int argc, char* argv[]);

void dotnet_task(void* params)
{
    managed_main(0, NULL);
    vTaskDelete(NULL);
}

int main()
{
    // Initialize hardware

    // Create main .NET task
    xTaskCreate(dotnet_task, "DotNet",
                configMINIMAL_STACK_SIZE * 8,
                NULL, tskIDLE_PRIORITY + 2, NULL);

    // Start scheduler
    vTaskStartScheduler();

    return 0;
}
```

---

## Phase 7: Libraries Subset

### 7.1 Supported Libraries

For FreeRTOS, support a minimal subset:

| Library | Status | Notes |
|---------|--------|-------|
| System.Runtime | ✅ | Core types |
| System.Collections | ✅ | Basic collections |
| System.Text | ✅ | String handling |
| System.Threading | ⚠️ | Map to FreeRTOS tasks |
| System.IO | ❌ | No filesystem by default |
| System.Net | ❌ | Requires FreeRTOS+TCP |
| System.Diagnostics.Process | ❌ | No processes |

### 7.2 Test Exclusions

**File: `src/libraries/tests.proj`**
```xml
<ItemGroup Condition="'$(TargetOS)' == 'freertos'">
  <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.IO.FileSystem\**\*.csproj" />
  <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Net.*\**\*.csproj" />
  <ProjectExclusions Include="$(MSBuildThisFileDirectory)System.Diagnostics.Process\**\*.csproj" />
</ItemGroup>
```

---

## Phase 8: Testing Strategy

### 8.1 Hardware Targets

Initial targets for testing:
- **STM32F4** (Cortex-M4, 1MB Flash, 192KB RAM)
- **STM32F7** (Cortex-M7, 2MB Flash, 512KB RAM)
- **QEMU ARM** (for CI/automated testing)

### 8.2 Test Application

Create a minimal test application:
```csharp
// src/tests/nativeaot/freertos/HelloWorld/Program.cs
using System;

class Program
{
    static void Main()
    {
        Console.WriteLine("Hello from .NET on FreeRTOS!");

        int sum = 0;
        for (int i = 0; i < 100; i++)
            sum += i;

        Console.WriteLine($"Sum: {sum}");
    }
}
```

### 8.3 QEMU Testing

```bash
# Build for QEMU ARM
dotnet publish -r freertos-arm -c Release

# Run in QEMU
qemu-system-arm -M lm3s6965evb -kernel output.elf -nographic
```

---

## Implementation Timeline

| Week | Phase | Deliverable |
|------|-------|-------------|
| 1-2 | Phase 1 | Build system (DONE) |
| 3-4 | Phase 2 | NativeAOT enabled for FreeRTOS |
| 5-7 | Phase 3 | PAL implementation |
| 8-9 | Phase 4 | GC configuration |
| 10 | Phase 5 | ARM32 assembly stubs |
| 11 | Phase 6 | Linker/startup code |
| 12 | Phase 7-8 | Libraries subset + testing |

---

## File Summary

### New Files Required
```
src/coreclr/nativeaot/Runtime/freertos/
├── PalFreeRTOS.cpp           # Main PAL implementation
├── HardwareExceptions.cpp     # ARM exception handling
├── cgroupcpu.cpp             # Stub
└── CMakeLists.txt            # Build config

src/coreclr/nativeaot/Runtime/arm/
├── AsmMacros_freertos.inc    # Assembly macros
└── GcStubs_freertos.asm      # GC barriers

src/coreclr/nativeaot/Bootstrap/freertos/
├── startup.cpp               # Entry point
└── CMakeLists.txt

src/coreclr/nativeaot/BuildIntegration/FreeRTOS/
├── link.ld                   # Linker script
└── Microsoft.NETCore.Native.FreeRTOS.targets
```

### Files to Modify
```
eng/Subsets.props                          # Enable NativeAOT for FreeRTOS
src/coreclr/nativeaot/Runtime/Pal.h        # Add TARGET_FREERTOS
src/coreclr/nativeaot/Runtime/CMakeLists.txt
src/coreclr/nativeaot/BuildIntegration/Microsoft.NETCore.Native.targets
src/libraries/tests.proj                   # Test exclusions
```

---

## Phase 2 Progress: Windows Cross-Compilation (IN PROGRESS)

### What We've Accomplished

Successfully configured Windows-to-FreeRTOS ARM cross-compilation:

1. **Created Toolchain File** - `eng/common/cross/toolchain.freertos-windows.cmake`
   - Configures arm-none-eabi-gcc as the cross-compiler
   - Sets CMAKE_SYSTEM_NAME to "Generic" for bare-metal
   - Explicitly sets host architecture variables (CLR_CMAKE_HOST_WIN32, CLR_CMAKE_HOST_ARCH_AMD64)
   - Sets CMAKE_C_COMPILER_TARGET to enable proper tool detection

2. **Modified Build Scripts** - `eng/native/gen-buildsys.cmd`
   - Added FreeRTOS detection block (lines 103-110)
   - Forces Ninja generator for FreeRTOS builds
   - Sets CLR_CMAKE_HOST_ARCH=x64 (build machine) vs CLR_CMAKE_TARGET_ARCH=arm (target)
   - Applies toolchain file automatically
   - Skips Visual Studio-specific flags and Windows system version for FreeRTOS

3. **Updated Compiler Configuration** - `eng/native/configurecompiler.cmake`
   - Added FreeRTOS as separate elseif case (lines 776-779)
   - Defines TARGET_FREERTOS instead of TARGET_WINDOWS
   - Adds DISABLE_CONTRACTS for FreeRTOS (like Unix targets)

4. **Fixed Platform Detection** - `src/coreclr/nativeaot/Runtime/CMakeLists.txt`
   - Changed WIN32 to CLR_CMAKE_TARGET_WIN32 (line 85)
   - Prevents Windows-specific code from being included for FreeRTOS cross-compilation
   - Added FreeRTOS elseif block with proper ASM_SUFFIX and definitions

5. **Modified Unix Configuration** - `src/coreclr/nativeaot/Runtime/CMakeLists.txt`
   - Changed else() to elseif(CLR_CMAKE_TARGET_UNIX) (line 308)
   - Added separate elseif for FreeRTOS to skip unix/configure.cmake
   - Prevents FreeRTOS from falling through to Unix or Windows paths

6. **Created FreeRTOS Build Integration**
   - `src/coreclr/nativeaot/BuildIntegration/Microsoft.NETCore.Native.FreeRTOS.targets`
   - Configures arm-none-eabi toolchain, linker flags, and bare-metal specs
   - Supports configurable CPU type, FPU, and float ABI
   - Integrated into main targets file with conditional import

7. **Updated Native Libraries Configuration**
   - Excluded FreeRTOS from System.Net.Security.Native and System.Security.Cryptography.Native
   - Modified both `src/native/libs/CMakeLists.txt` and `src/native/corehost/apphost/static/CMakeLists.txt`
   - FreeRTOS grouped with Browser/WASI for library exclusions

8. **Excluded CoreCLR Tools** - `src/coreclr/CMakeLists.txt`
   - Added CLR_CMAKE_TARGET_FREERTOS exclusions to tools and hosts subdirectories (lines 312-318)
   - Prevents building JIT tools (superpmi, corerun, etc.) for NativeAOT-only targets

### Current Status

**Build Command:**
```powershell
.\build.cmd clr.nativeaotruntime -os freertos -arch arm -c Debug
```

**Current Issue:**
Build is still trying to compile CoreCLR/JIT components (jitinterface, clrgc, clrjit, createdump, mscorrc) even though `-component nativeaot` is specified. These components have "No SOURCES" errors because they're being configured for a Generic/FreeRTOS target where their source lists aren't populated.

**Root Cause:**
The `-component nativeaot` flag tells MSBuild which subset to build, but CMake is still processing the entire `src/coreclr/CMakeLists.txt` file, including CoreCLR components. The component filtering happens at the MSBuild level, not the CMake level.

### Key Learnings

1. **Cross-Compilation is Complex on Windows**
   - Windows build system assumes native Windows builds
   - Toolchain files with CMAKE_SYSTEM_NAME="Generic" break WIN32 detection
   - Must explicitly set host architecture variables in toolchain file

2. **Host vs Target Architecture**
   - CLR_CMAKE_HOST_ARCH = build machine architecture (x64 on Windows)
   - CLR_CMAKE_TARGET_ARCH = target architecture (arm for FreeRTOS)
   - Build scripts were incorrectly setting host arch to target arch

3. **Platform Detection Order Matters**
   - Toolchain file executes before command-line CMake variables
   - Platform detection in configureplatform.cmake depends on order of checks
   - FreeRTOS needed its own elseif block to avoid falling into Windows or Unix paths

4. **Component Build System**
   - CoreCLR CMakeLists.txt processes both JIT and NativeAOT components
   - Component filtering (-component nativeaot) happens in MSBuild, not CMake
   - Need to conditionally exclude JIT components at CMake level for NativeAOT-only targets

### Next Steps

1. **Investigate Component Build Logic**
   - Understand how -component nativeaot should work
   - Determine if NativeAOT subdirectory should be built separately
   - Check if there's a FEATURE_JIT or similar flag to skip CoreCLR components

2. **Add CMake Conditionals for NativeAOT-Only Builds**
   - Skip JIT subdirectories when building NativeAOT for embedded targets
   - Add CLR_CMAKE_TARGET_FREERTOS checks to gc/, jit/, debug/ subdirectory inclusions
   - Or determine if there's a better way to invoke CMake only on nativeaot/

3. **Phase 3: Implement FreeRTOS PAL**
   - Once build system issues are resolved
   - Create PalFreeRTOS.cpp with memory, threading, and synchronization implementations
   - Map NativeAOT PAL calls to FreeRTOS APIs

### Files Modified (Phase 2)

```
eng/common/cross/toolchain.freertos-windows.cmake   # NEW
eng/native/gen-buildsys.cmd                         # Modified
eng/native/configurecompiler.cmake                  # Modified
eng/Subsets.props                                   # Modified
src/coreclr/CMakeLists.txt                          # Modified
src/coreclr/nativeaot/Runtime/CMakeLists.txt        # Modified
src/coreclr/nativeaot/Runtime/freertos/PalFreeRTOS.h # NEW
src/coreclr/nativeaot/BuildIntegration/Microsoft.NETCore.Native.FreeRTOS.targets # NEW
src/coreclr/nativeaot/BuildIntegration/Microsoft.NETCore.Native.targets # Modified
src/native/libs/CMakeLists.txt                      # Modified
src/native/corehost/apphost/static/CMakeLists.txt   # Modified
```

---

## Open Questions

1. **FreeRTOS version?** - Target FreeRTOS 10.x or 11.x?
2. **Hardware BSP?** - Which board support packages to include?
3. **Console output?** - UART? Semihosting? RTT?
4. **Minimum RAM?** - 128KB? 256KB? 512KB?
5. **FreeRTOS+TCP?** - Include networking support?
6. **FreeRTOS+FAT?** - Include filesystem support?

---

## References

- NativeAOT Runtime: `src/coreclr/nativeaot/Runtime/`
- NativeAOT PAL (Unix): `src/coreclr/nativeaot/Runtime/unix/PalUnix.cpp`
- NativeAOT PAL (Windows): `src/coreclr/nativeaot/Runtime/windows/PalMinWin.cpp`
- FreeRTOS API: https://www.freertos.org/a00106.html
- ARM Cortex-M: https://developer.arm.com/documentation
