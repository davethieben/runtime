# Plan: Adding FreeRTOS Support for ARM32 Architecture

This document outlines the steps required to add FreeRTOS as a new target operating system to the .NET runtime, targeting the ARM32 CPU architecture.

## Overview

**Target OS:** FreeRTOS
**Target Architecture:** ARM32 (arm)
**Runtime Identifier (RID):** `freertos-arm`

FreeRTOS is a real-time operating system (RTOS) for embedded systems. Unlike illumos or other Unix-like systems, FreeRTOS has a fundamentally different execution model:
- No virtual memory / MMU (typically)
- Single address space
- Cooperative/preemptive task scheduling (not processes)
- Limited or no dynamic linking
- No POSIX API (though FreeRTOS+POSIX exists)

This means a full CoreCLR port is likely **not feasible**. The realistic targets are:
1. **Mono runtime** (interpreter mode) - Most viable
2. **NativeAOT** - Possible but requires significant work

## Phase 1: Build System Infrastructure

### 1.1 Build Script Updates

**File: `eng/build.sh`**
- Add `freertos` to the target OS options in help text (line ~34)
- Add case handler for `freertos` OS selection (around line 290)

```bash
freertos)
  os="freertos" ;;
```

### 1.2 MSBuild Properties

**File: `eng/RuntimeIdentifier.props`**
- Add `TargetsFreertos` property (following illumos pattern at line 50):
```xml
<TargetsFreertos Condition="'$(TargetOS)' == 'freertos'">true</TargetsFreertos>
```
- Update `TargetsUnix` to include freertos (or create separate `TargetsRTOS` property)

**File: `eng/OSArch.props`**
- Add FreeRTOS host OS detection (line ~7):
```xml
<_hostOS Condition="$([MSBuild]::IsOSPlatform('FREERTOS'))">freertos</_hostOS>
```

**File: `eng/Subsets.props`**
- Add `freertos` to the NativeAOT exclusion list if not supporting NativeAOT (line ~50)
- Define which subsets are buildable for FreeRTOS

**File: `eng/versioning.targets`**
- Add FreeRTOS to supported platforms (line ~84):
```xml
<SupportedPlatform Condition="'$(TargetPlatformIdentifier)' == 'freertos'" Include="freertos" />
```

### 1.3 CMake Configuration

**File: `eng/native/configureplatform.cmake`**
- Add FreeRTOS detection block (following illumos pattern at lines 208-214):
```cmake
if(CLR_CMAKE_HOST_OS STREQUAL freertos)
    set(CLR_CMAKE_HOST_FREERTOS 1)
    # FreeRTOS is not Unix-like in the traditional sense
    set(CLR_CMAKE_HOST_RTOS 1)
endif()
```
- Add target OS configuration (around line 423):
```cmake
if(CLR_CMAKE_TARGET_OS STREQUAL freertos)
    set(CLR_CMAKE_TARGET_FREERTOS 1)
    set(CLR_CMAKE_TARGET_RTOS 1)
endif()
```

**File: `eng/native/configurecompiler.cmake`**
- Add `TARGET_FREERTOS` compile definition (following line 764):
```cmake
if(CLR_CMAKE_TARGET_FREERTOS)
  add_compile_definitions($<$<NOT:$<BOOL:$<TARGET_PROPERTY:IGNORE_DEFAULT_TARGET_OS>>>:TARGET_FREERTOS>)
endif()
```
- Configure ARM32 compiler flags for embedded targets (thumb mode, no-exceptions, etc.)

**File: `eng/native/tryrun.cmake`**
- Add FreeRTOS detection in cross-compilation rootfs check (around line 39)
- Set appropriate cache values for FreeRTOS capabilities (no POSIX semaphores, etc.)

**File: `eng/common/cross/toolchain.cmake`**
- Add toolchain configuration for FreeRTOS cross-compilation:
```cmake
elseif(FREERTOS)
  set(TOOLCHAIN "arm-none-eabi")
```
- Configure sysroot and linker flags for FreeRTOS SDK

### 1.4 Shell Scripts

**File: `eng/common/native/init-os-and-arch.sh`**
- Add FreeRTOS detection (though this may not apply for cross-compilation only)

**File: `eng/common/native/init-distro-rid.sh`**
- Add FreeRTOS RID generation (around line 45):
```bash
elif [ "$targetOs" = "freertos" ]; then
    nonPortableRid="freertos-${targetArch}"
```

### 1.5 Cross-Compilation Rootfs

**File: `eng/common/cross/build-rootfs.sh`**
- Add FreeRTOS rootfs setup support
- Define `__FreertosPackages` for required dependencies
- Add download/setup for FreeRTOS SDK and ARM toolchain

## Phase 2: Runtime Selection

Given FreeRTOS constraints, **CoreCLR is not a viable target**. Focus on:

### 2.1 Mono Runtime (Recommended Path)

Mono has better support for constrained environments and interpreter mode.

**Key files to modify:**
- `src/mono/mono.proj` - Add FreeRTOS as target
- `src/mono/CMakeLists.txt` - Add FreeRTOS configuration
- `src/mono/mono/utils/` - Add FreeRTOS-specific implementations

**Required Mono work:**
1. Implement FreeRTOS threading primitives (`mono-threads-freertos.c`)
2. Implement memory allocation using FreeRTOS heap APIs
3. Disable/stub unavailable features (signals, fork, mmap)
4. Configure for interpreter-only mode (no JIT on typical embedded ARM)

### 2.2 NativeAOT (Alternative Path)

NativeAOT compiles to standalone native binaries, potentially suitable for embedded.

**File: `eng/Subsets.props`**
- Consider enabling NativeAOT for FreeRTOS if pursuing this path

**Required work:**
- Implement minimal PAL for FreeRTOS
- Configure linker scripts for embedded targets
- Implement FreeRTOS-specific startup code

## Phase 3: Native Host Components

**File: `src/native/corehost/hostmisc/pal.unix.cpp`**
- Add FreeRTOS RID platform detection (following illumos at line 709):
```cpp
#elif defined(TARGET_FREERTOS)
pal::string_t pal::get_current_os_rid_platform()
{
    return _X("freertos");
}
```

**File: `src/native/corehost/hostpolicy/deps_format.cpp`**
- Add FreeRTOS to platform fallback chain (line ~209)

**Note:** For embedded FreeRTOS, the standard corehost may not apply. A custom minimal host or direct embedding may be needed.

## Phase 4: PAL Implementation

Create FreeRTOS-specific PAL implementation:

### 4.1 New Files Required

```
src/coreclr/pal/src/arch/arm/freertos/
├── context.S          # Context save/restore for ARM32
├── exceptionhelper.S  # Exception handling stubs
└── processor.cpp      # ARM32 processor detection

src/coreclr/pal/src/thread/
└── thread-freertos.cpp  # FreeRTOS task/thread mapping

src/coreclr/pal/src/sync/
└── sync-freertos.cpp    # FreeRTOS semaphore/mutex wrappers
```

### 4.2 PAL Header Updates

**File: `src/coreclr/pal/inc/pal.h`**
- Add `TARGET_FREERTOS` conditionals
- Define CONTEXT structure for ARM32
- Stub out unavailable APIs (signals, fork, etc.)

### 4.3 Key PAL Functions to Implement

| Function | FreeRTOS Implementation |
|----------|------------------------|
| `PAL_Initialize` | Initialize FreeRTOS task context |
| `CreateThread` | `xTaskCreate()` wrapper |
| `WaitForSingleObject` | `xSemaphoreTake()` |
| `SetEvent/ResetEvent` | FreeRTOS event groups |
| `VirtualAlloc` | `pvPortMalloc()` (no virtual memory) |
| `GetCurrentThreadId` | `xTaskGetCurrentTaskHandle()` |

## Phase 5: GC Considerations

**File: `src/coreclr/gc/gcpriv.h`**
- Disable regions for FreeRTOS (similar to illumos note at line 144)
- Consider enabling `FEATURE_CONSERVATIVE_GC` for initial bring-up

**Memory constraints:**
- FreeRTOS typically has limited RAM (KB to low MB)
- Configure small GC heap defaults
- Consider workstation GC only (no server GC)

## Phase 6: Libraries Exclusions

**File: `src/libraries/tests.proj`**
- Add FreeRTOS exclusions (following illumos pattern at line 136):
```xml
<ItemGroup Condition="'$(TargetOS)' == 'freertos'">
  <!-- Exclude tests requiring features not available on FreeRTOS -->
  <ProjectExclusions Include="..." />
</ItemGroup>
```

**Libraries to exclude or stub:**
- `System.Diagnostics.Process` - No process model
- `System.IO.FileSystem` - Limited or no filesystem (depends on FreeRTOS config)
- `System.Net.Sockets` - Requires FreeRTOS+TCP
- `System.Threading.Thread` - Map to FreeRTOS tasks

## Phase 7: Package Configuration

**File: `src/coreclr/.nuget/Directory.Build.props`**
- Add FreeRTOS to supported package OS groups (line ~23):
```xml
<SupportedPackageOSGroups>...;freertos</SupportedPackageOSGroups>
```
- Add `freertos-arm` RID (around line 89):
```xml
<ItemGroup Condition="$(SupportedPackageOSGroups.Contains(';freertos;'))">
  <OfficialBuildRID Include="freertos-arm" />
</ItemGroup>
```

## Phase 8: Crossgen/AOT

**File: `src/coreclr/crossgen-corelib.proj`**
- Disable crossgen for FreeRTOS initially (line ~21):
```xml
<BuildDll Condition="'$(TargetOS)' == 'freertos'">false</BuildDll>
```

## Implementation Order

1. **Week 1-2:** Build system infrastructure (Phase 1)
   - All MSBuild/CMake changes
   - Cross-compilation toolchain setup

2. **Week 3-4:** Mono runtime PAL (Phase 2.1 + Phase 4)
   - Threading primitives
   - Memory management
   - Basic task execution

3. **Week 5-6:** Minimal managed code execution
   - Hello World execution
   - Basic type system

4. **Week 7-8:** GC bring-up (Phase 5)
   - Conservative GC initially
   - Memory pressure handling

5. **Week 9-12:** Libraries and stabilization (Phase 6)
   - Core libraries subset
   - Test coverage

## Key Differences from illumos Port

| Aspect | illumos | FreeRTOS |
|--------|---------|----------|
| Type | Unix-like OS | RTOS |
| Process model | Full processes | Tasks (single address space) |
| Memory | Virtual memory | Physical memory only |
| POSIX | Full POSIX | Limited/none |
| Filesystem | Full | Optional (FatFS, LittleFS) |
| Networking | BSD sockets | FreeRTOS+TCP |
| Target runtime | CoreCLR | Mono (interpreter) |

## Open Questions

1. **FreeRTOS version requirements?** - Minimum FreeRTOS version to support
2. **Hardware targets?** - Specific ARM32 SoCs/boards to target
3. **Memory budget?** - Minimum RAM requirements for .NET on FreeRTOS
4. **Feature subset?** - Which .NET APIs are in scope?
5. **FreeRTOS+POSIX?** - Use POSIX compatibility layer or native FreeRTOS APIs?

## References

- FreeRTOS documentation: https://www.freertos.org/Documentation/
- .NET Porting Guide: `docs/design/coreclr/botr/guide-for-porting.md`
- Mono embedded: https://www.mono-project.com/docs/advanced/embedding/
- ARM32 ABI: `docs/design/coreclr/botr/clr-abi.md`
