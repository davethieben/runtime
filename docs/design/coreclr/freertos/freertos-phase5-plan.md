# Phase 5: End-to-End Linking and Minimal Execution - Plan

## Objective

Link the NativeAOT runtime static libraries with a minimal FreeRTOS application and achieve first managed code execution on ARM Cortex-M bare-metal (or QEMU emulation).

## Prerequisites

- Phase 4 complete ✅ (all runtime libraries compile and link)
- ARM GNU Toolchain 13.3.1+ installed
- QEMU ARM system emulator (for testing without hardware)
- .NET SDK with NativeAOT ILC

## Scope

Phase 5 bridges the gap between "runtime libraries compile" and "managed code runs". This involves:

1. Getting NativeAOT ILC to produce an ARM32 bare-metal object file from C#
2. Creating the linker script and startup code for a FreeRTOS target
3. Linking everything together into a bootable ELF
4. Running it on QEMU or real hardware

## Architecture

```
┌─────────────────────────────────────────────────────┐
│                  Final ELF Binary                    │
├─────────────────────────────────────────────────────┤
│  startup.o          Vector table, reset handler      │
│  main.o             FreeRTOS init, managed entry     │
│  FreeRTOS/*.o       FreeRTOS kernel                  │
│  managed_app.o      ILC-compiled C# code             │
│  libRuntime.WorkstationGC.a  NativeAOT runtime       │
│  libBootstrapper.a           NativeAOT bootstrapper   │
│  libc.a (newlib-nano)        C library               │
├─────────────────────────────────────────────────────┤
│  link.ld            Memory layout (flash, SRAM)      │
└─────────────────────────────────────────────────────┘
```

## Step 1: Investigate ILC ARM32 Bare-Metal Output

### Questions to Answer

- [ ] Can ILC target ARM32 with no OS (`TargetOS=none` or `TargetOS=freertos`)?
- [ ] What object format does ILC produce? (ELF `.o`, static library?)
- [ ] What external symbols does ILC-generated code reference?
- [ ] What entry point convention does ILC use? (`__managed__Main`? `StartupCodeMain`?)
- [ ] Does ILC need a custom System.Private.CoreLib for bare-metal?

### Investigation Steps

1. Study ILC source code for target OS handling
2. Try compiling a minimal C# program with ILC for Linux ARM32
3. Examine the symbol table of the resulting object
4. Identify all undefined symbols that must be resolved by the runtime libraries

## Step 2: Create Minimal C# Test Application

```csharp
// Program.cs - Absolute minimum managed code
static class Program
{
    static int Main()
    {
        // Phase 5 goal: reach this point and return
        return 42;
    }
}
```

Compile with NativeAOT ILC:
```bash
# Determine exact ILC invocation needed
dotnet publish -r linux-arm -c Release /p:PublishAot=true
# Then extract and adapt the ILC command for FreeRTOS
```

## Step 3: Create Linker Script

Target: STM32F407 (Cortex-M4F, 192KB SRAM, 1MB Flash) or QEMU `lm3s6965evb`

```ld
/* link.ld - ARM Cortex-M memory layout */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

SECTIONS
{
    .text : {
        KEEP(*(.isr_vector))    /* Vector table at start of flash */
        *(.text*)               /* Code */
        *(.rodata*)             /* Read-only data */
    } > FLASH

    .data : {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > SRAM AT > FLASH

    .bss : {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > SRAM

    /* GC heap region - large contiguous block */
    .gc_heap (NOLOAD) : {
        . = ALIGN(8);
        _gc_heap_start = .;
        . = . + 64K;           /* Adjust for available RAM */
        _gc_heap_end = .;
    } > SRAM

    _estack = ORIGIN(SRAM) + LENGTH(SRAM);
}
```

## Step 4: Create Startup Code

```asm
/* startup.S - ARM Cortex-M vector table and reset handler */
.syntax unified
.thumb

.section .isr_vector, "a"
.word _estack           /* Initial stack pointer */
.word Reset_Handler     /* Reset handler */
.word NMI_Handler
.word HardFault_Handler
/* ... remaining vectors ... */

.section .text
.thumb_func
.global Reset_Handler
Reset_Handler:
    /* Copy .data from flash to SRAM */
    /* Zero .bss */
    /* Call SystemInit if needed */
    /* Call main */
    bl main
    b .                 /* Hang if main returns */
```

## Step 5: Create FreeRTOS Main

```c
/* main.c - FreeRTOS initialization and managed code entry */
#include "FreeRTOS.h"
#include "task.h"

/* Forward declaration - provided by NativeAOT bootstrapper */
extern void ManagedEntry(void);

static void managed_task(void *pvParameters)
{
    ManagedEntry();
    vTaskDelete(NULL);
}

int main(void)
{
    /* Initialize hardware (clocks, UART for debug) */

    /* Create task for managed code */
    xTaskCreate(managed_task, "managed", 4096, NULL, 1, NULL);

    /* Start FreeRTOS scheduler */
    vTaskStartScheduler();

    /* Should never reach here */
    for (;;);
}
```

## Step 6: Link Everything

```bash
arm-none-eabi-gcc \
    -mcpu=cortex-m4 -mthumb -mfloat-abi=hard -mfpu=fpv4-sp-d16 \
    -specs=nosys.specs -specs=nano.specs \
    -T link.ld \
    -Wl,--gc-sections \
    startup.o main.o \
    managed_app.o \
    -Wl,--whole-archive \
    -lRuntime.WorkstationGC \
    -Wl,--no-whole-archive \
    -lBootstrapper \
    -lFreeRTOS \
    -o app.elf

# Check size
arm-none-eabi-size app.elf

# Check for undefined symbols
arm-none-eabi-nm app.elf | grep " U "
```

## Step 7: Test on QEMU

```bash
qemu-system-arm \
    -machine lm3s6965evb \
    -kernel app.elf \
    -semihosting \
    -nographic
```

## Expected Challenges

### 1. ILC Target Configuration
ILC may not have a FreeRTOS target. May need to:
- Use `linux-arm` target and post-process the output
- Add a new ILC target for bare-metal
- Use the `-Os` or `--targetOS` ILC flags

### 2. Missing Symbols
ILC-generated code likely references symbols not in our runtime:
- `System.Private.CoreLib` internal calls
- GC allocation helpers with specific signatures
- Static constructor initialization
- Module initialization

### 3. CoreLib Dependency
NativeAOT requires `System.Private.CoreLib` compiled for the target. This is a large dependency that may need trimming for embedded.

### 4. Binary Size
The full runtime libraries are ~6-9 MB. For a Cortex-M4 with 1MB flash, aggressive linker garbage collection (`--gc-sections`) is essential. May also need:
- LTO (Link-Time Optimization)
- Custom stripped CoreLib
- Removal of unused GC features

### 5. Stack Size
FreeRTOS tasks have limited stack. NativeAOT managed code may use more stack than typical embedded C. Need to:
- Profile stack usage
- Configure adequate task stack size
- Enable FreeRTOS stack overflow detection

## Success Criteria

Phase 5 is complete when:

- [ ] A minimal C# program is compiled with ILC for ARM32 bare-metal
- [ ] The ILC output links successfully with the runtime libraries
- [ ] The linked ELF binary runs on QEMU (or hardware) and reaches the managed entry point
- [ ] The managed `Main()` method executes and returns
- [ ] All undefined symbol references are resolved

## Stretch Goals

- [ ] `Console.Write` or equivalent output via UART/semihosting
- [ ] Object allocation (triggers `RhpNewFast`)
- [ ] GC collection (triggers `RhpGcPoll`, write barriers)
- [ ] Simple arithmetic and control flow in managed code

## Estimated Effort

This phase involves significant investigation and experimentation:
- **ILC investigation**: Understanding how to target bare-metal
- **Build integration**: Linker script, startup code, FreeRTOS integration
- **Symbol resolution**: Identifying and resolving all missing symbols
- **Debugging**: First-time bare-metal execution will require extensive debugging

---

Created: 2026-03-01
Status: ⏳ NEXT
