# FreeRTOS NativeAOT Quick Start Guide

This guide walks you through setting up a basic FreeRTOS project with NativeAOT runtime support.

**⚠️ Current Status**: Phase 4 complete - all C/C++ and assembly code compiles and links into static libraries. End-to-end execution (linking with a managed app and running on hardware/QEMU) is Phase 5. This guide is for developers who want to contribute to Phase 5+ or experiment with the foundation.

## Prerequisites

### Software Requirements

1. **ARM GNU Toolchain** (13.3.1 or later)
   - Download from: https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads
   - Install to: `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1`

2. **CMake** (3.20 or later)
   ```bash
   winget install cmake
   ```

3. **Ninja Build System**
   ```bash
   winget install ninja
   ```

4. **FreeRTOS Source**
   ```bash
   git clone https://github.com/FreeRTOS/FreeRTOS.git
   ```

5. **.NET SDK** (9.0 or later)
   ```bash
   winget install Microsoft.DotNet.SDK.9
   ```

### Hardware Requirements

One of the following ARM Cortex-M development boards:
- **STM32F4 Discovery** (Cortex-M4, 192KB RAM) - Recommended for beginners
- **STM32F7 Discovery** (Cortex-M7, 512KB RAM) - Better performance
- **STM32H7 Nucleo** (Cortex-M7, 1MB RAM) - Best performance
- Any ARM Cortex-M3/M4/M7 board with at least 128KB RAM

For testing without hardware:
- **QEMU** (ARM system emulation)
  ```bash
  winget install qemu
  ```

## Step 1: Build NativeAOT Runtime for FreeRTOS

### Clone and Configure

```bash
# Clone dotnet/runtime repository
git clone https://github.com/dotnet/runtime.git
cd runtime

# Checkout branch with FreeRTOS support
git checkout dave/freertos  # Or main once merged
```

### Build Runtime

```bash
# Navigate to CoreCLR directory
cd src/coreclr

# Configure build for FreeRTOS ARM32 Debug
python build-runtime.py -os freertos -arch arm -c Debug

# Or for Release
python build-runtime.py -os freertos -arch arm -c Release
```

### Build Output

Static libraries will be in:
```
artifacts/bin/coreclr/freertos.arm.Debug/
├── libRuntime.WorkstationGC.a      # Workstation GC
├── libRuntime.ServerGC.a            # Server GC (for larger systems)
├── libstandalonegc-disabled.a      # GC disabled config
└── libstandalonegc-enabled.a       # Standalone GC enabled
```

## Step 2: Create FreeRTOS Project

### Project Structure

```
my-freertos-nativeaot-app/
├── FreeRTOS/                    # FreeRTOS source (git submodule)
├── managed/                     # Managed C# code
│   ├── Program.cs
│   └── App.csproj
├── native/                      # Native C/C++ entry point
│   ├── main.c                   # FreeRTOS initialization
│   ├── managed_entry.cpp        # NativeAOT entry point wrapper
│   ├── FreeRTOSConfig.h        # FreeRTOS configuration
│   └── syscalls.c              # Newlib syscalls (UART, etc.)
├── linker/
│   └── stm32f4.ld              # Linker script for your MCU
├── CMakeLists.txt               # Build configuration
└── arm-none-eabi-toolchain.cmake  # CMake toolchain file
```

### FreeRTOS Configuration

Create `native/FreeRTOSConfig.h`:

```c
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// MCU Configuration (adjust for your board)
#define configCPU_CLOCK_HZ                      168000000  // 168 MHz (STM32F4)
#define configTICK_RATE_HZ                      1000       // 1 ms tick

// Basic FreeRTOS Configuration
#define configUSE_PREEMPTION                    1
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                128        // Words
#define configMAX_TASK_NAME_LEN                 16

// Memory Configuration (IMPORTANT for NativeAOT)
#define configTOTAL_HEAP_SIZE                   (64 * 1024)  // 64 KB
#define configSUPPORT_DYNAMIC_ALLOCATION        1

// Required for NativeAOT Runtime
#define configUSE_TASK_NOTIFICATIONS            1
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_EVENT_GROUPS                  1

// Task Management
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configUSE_TASK_FPU_SUPPORT              1  // Enable if using FPU

// API Includes
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_xTaskGetSchedulerState          1

// Cortex-M Specific
#define configPRIO_BITS                         4  // STM32F4 has 4 bits
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY 15
#define configKERNEL_INTERRUPT_PRIORITY         (configLIBRARY_LOWEST_INTERRUPT_PRIORITY << (8 - configPRIO_BITS))
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    (5 << (8 - configPRIO_BITS))

// Assert
extern void vAssertCalled(const char* file, int line);
#define configASSERT(x) if((x) == 0) vAssertCalled(__FILE__, __LINE__)

#endif // FREERTOS_CONFIG_H
```

### Native Entry Point

Create `native/main.c`:

```c
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>

// Declare managed entry point (generated by NativeAOT compiler)
extern int __managed__Main(int argc, char** argv);

// Assert handler
void vAssertCalled(const char* file, int line) {
    printf("ASSERT: %s:%d\r\n", file, line);
    while(1);
}

// NativeAOT managed task
static void managedTask(void* pvParameters) {
    printf("Starting managed code...\r\n");

    // Call into managed C# code
    int result = __managed__Main(0, NULL);

    printf("Managed code returned: %d\r\n", result);

    // Task done
    vTaskDelete(NULL);
}

// System clock configuration (STM32F4 specific)
static void SystemClock_Config(void) {
    // TODO: Configure your MCU clock
    // This is board-specific
}

// UART initialization for printf
static void UART_Init(void) {
    // TODO: Initialize UART for debug output
    // This is board-specific
}

int main(void) {
    // Initialize hardware
    SystemClock_Config();
    UART_Init();

    printf("\r\n=================================\r\n");
    printf("FreeRTOS + NativeAOT Starting...\r\n");
    printf("=================================\r\n");

    // Create task for managed code
    // Large stack needed for GC (adjust based on your app)
    xTaskCreate(managedTask, "Managed", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);

    // Start FreeRTOS scheduler
    printf("Starting scheduler...\r\n");
    vTaskStartScheduler();

    // Should never reach here
    printf("ERROR: Scheduler failed to start!\r\n");
    while(1);
}
```

### Managed C# Application

Create `managed/Program.cs`:

```csharp
using System;
using System.Runtime.InteropServices;

class Program
{
    // P/Invoke to native printf (for console output)
    [DllImport("*", CallingConvention = CallingConvention.Cdecl)]
    private static extern int printf(string format, __arglist);

    // Entry point called from native code
    static int Main()
    {
        printf("Hello from managed C# code!\n");
        printf("Running on FreeRTOS with NativeAOT\n");

        // Basic tests
        TestValueTypes();
        TestArrays();
        TestStrings();

        printf("All tests completed successfully!\n");
        return 0;
    }

    static void TestValueTypes()
    {
        printf("Testing value types...\n");
        int a = 42;
        int b = 13;
        int sum = a + b;
        printf("  42 + 13 = %d\n", __arglist(sum));
    }

    static void TestArrays()
    {
        printf("Testing arrays...\n");
        int[] arr = new int[10];
        for (int i = 0; i < arr.Length; i++)
        {
            arr[i] = i * 2;
        }
        printf("  Array[5] = %d\n", __arglist(arr[5]));
    }

    static void TestStrings()
    {
        printf("Testing strings...\n");
        string hello = "Hello";
        string world = "World";
        string combined = hello + " " + world;
        // Note: String operations require GC
        printf("  String test passed\n");
    }
}
```

Create `managed/App.csproj`:

```xml
<Project Sdk="Microsoft.NET.Sdk">
  <PropertyGroup>
    <OutputType>Exe</OutputType>
    <TargetFramework>net9.0</TargetFramework>
    <RuntimeIdentifier>freertos-arm</RuntimeIdentifier>
    <PublishAot>true</PublishAot>
    <IlcOptimizationPreference>Speed</IlcOptimizationPreference>
    <IlcGenerateStackTraceData>false</IlcGenerateStackTraceData>

    <!-- FreeRTOS-specific settings -->
    <IlcDisableReflection>true</IlcDisableReflection>
    <InvariantGlobalization>true</InvariantGlobalization>
    <EventSourceSupport>false</EventSourceSupport>
    <UseSystemResourceKeys>true</UseSystemResourceKeys>

    <!-- Optimize for size (important for embedded) -->
    <IlcOptimizationPreference>Size</IlcOptimizationPreference>
    <IlcFoldIdenticalMethodBodies>true</IlcFoldIdenticalMethodBodies>
  </PropertyGroup>
</Project>
```

## Step 3: CMake Build Configuration

**⚠️ Note**: Full build integration requires Phase 4 (Assembly Helpers) to be complete. This section shows the intended integration.

Create `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.20)
project(FreeRTOS-NativeAOT-App C CXX ASM)

# ARM Cortex-M4F settings (adjust for your MCU)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16")

# Linker flags
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T${CMAKE_SOURCE_DIR}/linker/stm32f4.ld")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -Wl,--print-memory-usage")

# FreeRTOS source files
set(FREERTOS_DIR ${CMAKE_SOURCE_DIR}/FreeRTOS/FreeRTOS/Source)
file(GLOB FREERTOS_SOURCES
    ${FREERTOS_DIR}/*.c
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F/*.c
    ${FREERTOS_DIR}/portable/MemMang/heap_4.c
)

include_directories(
    ${CMAKE_SOURCE_DIR}/native
    ${FREERTOS_DIR}/include
    ${FREERTOS_DIR}/portable/GCC/ARM_CM4F
    ${NATIVEAOT_RUNTIME_DIR}/inc  # From dotnet/runtime build
)

# Native sources
file(GLOB NATIVE_SOURCES native/*.c native/*.cpp)

# NativeAOT managed code (TODO: integrate with dotnet publish)
# For now, manually compile managed code:
#   cd managed && dotnet publish -r freertos-arm -c Release

# Link everything together
add_executable(app.elf
    ${NATIVE_SOURCES}
    ${FREERTOS_SOURCES}
)

# Link NativeAOT runtime libraries
target_link_libraries(app.elf
    # Path to NativeAOT runtime libraries
    ${NATIVEAOT_RUNTIME_DIR}/libRuntime.WorkstationGC.a

    # Path to compiled managed code
    ${CMAKE_SOURCE_DIR}/managed/bin/Release/net9.0/freertos-arm/publish/App.a

    # System libraries
    -lm -lc -lgcc
)

# Generate binary and hex files
add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary app.elf app.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex app.elf app.hex
    COMMENT "Generating binary and hex files"
)

# Print size information
add_custom_command(TARGET app.elf POST_BUILD
    COMMAND ${CMAKE_SIZE} app.elf
    COMMENT "Binary size:"
)
```

## Step 4: Build and Flash

### Build Managed Code

```bash
cd managed
dotnet publish -r freertos-arm -c Release
```

**Note**: This requires NativeAOT compiler support for `freertos-arm` RID. Currently not available until Phase 4+ complete.

### Build Firmware

```bash
mkdir build && cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=../arm-none-eabi-toolchain.cmake -G Ninja
ninja
```

### Flash to Hardware

Using OpenOCD (STM32F4 Discovery):
```bash
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg \
  -c "program app.elf verify reset exit"
```

Or using st-flash:
```bash
st-flash write app.bin 0x08000000
```

### Test with QEMU

```bash
qemu-system-arm \
  -machine lm3s6965evb \
  -kernel app.elf \
  -semihosting \
  -nographic
```

## Step 5: Debug

### OpenOCD + GDB

Terminal 1 - Start OpenOCD:
```bash
openocd -f interface/stlink-v2.cfg -f target/stm32f4x.cfg
```

Terminal 2 - Connect GDB:
```bash
arm-none-eabi-gdb app.elf
(gdb) target remote localhost:3333
(gdb) monitor reset halt
(gdb) load
(gdb) break main
(gdb) break __managed__Main
(gdb) continue
```

### Serial Console

Connect UART (115200 baud):
```bash
# Windows
putty -serial COM3 -sercfg 115200,8,n,1,N

# Linux/macOS
screen /dev/ttyUSB0 115200
```

## Current Limitations

**⚠️ Phase 3 Status**: The runtime compiles but cannot execute yet because:

1. **Assembly Helpers Missing** (Phase 4): 9 ARM assembly files need implementation
   - Without these, managed code cannot allocate objects, handle exceptions, or perform GC

2. **Hardware Exceptions Not Integrated** (Phase 5): ARM Cortex-M exception handling not connected

3. **Threading Not Implemented** (Phase 6): Single-threaded runtime only

**To Contribute**: See [FreeRTOS NativeAOT Status](freertos-nativeaot-status.md) for implementation roadmap.

## Next Steps

Once Phase 4+ are complete:

1. **Deploy to hardware**: Flash and run on real ARM Cortex-M boards
2. **Add peripherals**: GPIO, UART, SPI, I2C, etc.
3. **Optimize memory**: Reduce heap usage for smaller MCUs
4. **Performance tuning**: Profile and optimize hot paths
5. **Production hardening**: Watchdog, brown-out detection, error handling

## Troubleshooting

### Build Errors

**"arm-none-eabi-gcc not found"**
- Add ARM toolchain to PATH
- Windows: `C:\Program Files (x86)\Arm GNU Toolchain arm-none-eabi\13.3 rel1\bin`

**"Cannot find FreeRTOS headers"**
- Update `FREERTOS_DIR` in CMakeLists.txt
- Ensure FreeRTOS submodule is initialized

**"Undefined reference to __managed__Main"**
- Ensure managed code is compiled with NativeAOT
- Check that `.a` library is linked

### Runtime Errors

**Hard Fault**
- Check stack size (increase `configMINIMAL_STACK_SIZE`)
- Verify memory regions in linker script
- Check alignment (ARM requires aligned access)

**Heap Exhaustion**
- Increase `configTOTAL_HEAP_SIZE`
- Reduce managed object allocations
- Profile memory usage with GC statistics

**Scheduler Won't Start**
- Verify clock configuration
- Check interrupt priorities
- Ensure sufficient heap for idle task

## Resources

- [FreeRTOS Documentation](https://www.freertos.org/Documentation/)
- [ARM Cortex-M Programming](https://developer.arm.com/documentation/)
- [NativeAOT Compilation](https://learn.microsoft.com/dotnet/core/deploying/native-aot/)
- [FreeRTOS NativeAOT Status](freertos-nativeaot-status.md)
- [FreeRTOS PAL README](../../src/coreclr/nativeaot/Runtime/freertos/README.md)

## Getting Help

- **Issues**: File on [dotnet/runtime GitHub](https://github.com/dotnet/runtime/issues) with `area-NativeAOT` and `os-freertos` labels
- **Discussions**: [Discussions tab](https://github.com/dotnet/runtime/discussions)
- **Discord**: [.NET Discord Server](https://aka.ms/dotnet-discord) - #nativeaot channel

---

Last Updated: 2026-02-13
Status: Phase 3 Complete - Foundation Ready, Assembly Helpers Needed
