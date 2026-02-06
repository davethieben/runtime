# Mono Platform Porting Guide

This document outlines the steps and components required to port the Mono runtime to a new platform.

## Overview

Porting Mono requires implementing three concentric layers:

1. **OS Abstraction Layer** - Threading, file I/O, synchronization primitives
2. **ABI Layer** - Context structures, calling conventions, exception handling
3. **JIT Layer** - Architecture-specific instruction selection and code generation (optional if using interpreter)

---

## Phase 1: Build System Configuration

### Location: `src/mono/CMakeLists.txt`

**Tasks:**
- [ ] Add `HOST_<YOURPLATFORM>` detection based on `CMAKE_SYSTEM_NAME`
- [ ] Define `TARGET_<ARCH>` flags for your architecture
- [ ] Set platform-specific library dependencies (math, pthread equivalents, etc.)
- [ ] Configure AOT cross-compilation support if needed

**Reference Code (CMakeLists.txt lines ~50-100):**
```cmake
# Example pattern for new platform
if(CLR_CMAKE_HOST_OS STREQUAL "yourplatform")
  set(HOST_YOURPLATFORM 1)
endif()
```

**Key Variables to Define:**
- `HOST_<PLATFORM>` - The OS you're building on
- `TARGET_<ARCH>` - The CPU architecture (AMD64, ARM64, etc.)
- Platform-specific compiler flags
- Required system libraries

---

## Phase 2: Threading Layer

### Location: `src/mono/mono/utils/`

**Primary File to Create:** `mono-threads-<platform>.c`

**Required Implementations:**

| Function | Purpose |
|----------|---------|
| `mono_threads_suspend_begin_async_suspend` | Start suspending a thread |
| `mono_threads_suspend_check_suspend_result` | Verify suspension succeeded |
| `mono_threads_suspend_begin_async_resume` | Resume a suspended thread |
| `mono_native_thread_create` | Create OS thread |
| `mono_native_thread_join` | Wait for thread termination |
| `mono_native_thread_set_name` | Set thread debug name |

**Threading Backend Selection** (`mono-threads.h`):
```c
#ifdef HOST_YOURPLATFORM
#define USE_YOURPLATFORM_BACKEND
#endif
```

**Reference Files:**
- `mono-threads-posix.c` - Standard POSIX implementation
- `mono-threads-windows.c` - Windows implementation
- `mono-threads-wasm.c` - Minimal/sandboxed environment

**Key Considerations:**
- Thread suspension mechanism (signals on POSIX, SuspendThread on Windows)
- Thread-local storage API
- Thread stack bounds detection
- Cooperative vs preemptive suspension support

### Threading Backend Selection

The backend is selected in `mono-threads.h` (lines 131-143):

```c
#ifdef HOST_WASM
#define USE_WASM_BACKEND        // Cooperative only, no signals
#elif defined (_POSIX_VERSION)
  #if defined (__MACH__) && !defined (USE_SIGNALS_ON_MACH)
  #define USE_MACH_BACKEND      // macOS Mach ports
  #else
  #define USE_POSIX_BACKEND     // Signals-based (SIGUSR1/SIGUSR2)
  #endif
#elif HOST_WIN32
#define USE_WINDOWS_BACKEND     // SuspendThread/ResumeThread
#else
#error "no backend support for current platform"
#endif
```

### Embedded/Minimal Threading Implementations

For constrained or embedded platforms, study these minimal implementations:

#### Option A: Cooperative-Only (Recommended for Embedded)

**Best for:** Sandboxed environments, single-threaded systems, platforms without signal support

**Reference:** `mono-threads-wasm.c` (WASM implementation)

**Key insight:** All suspension functions become no-ops because the runtime polls for suspension at safepoints:

```c
// All return TRUE - cooperative suspension handles everything
gboolean mono_threads_suspend_begin_async_suspend(MonoThreadInfo *info, gboolean interrupt_kernel) {
    return TRUE;
}

gboolean mono_threads_suspend_check_suspend_result(MonoThreadInfo *info) {
    return TRUE;
}

gboolean mono_threads_suspend_begin_async_resume(MonoThreadInfo *info) {
    return TRUE;
}

// Critical: Assert cooperative mode is enabled
void mono_threads_suspend_init(void) {
    g_assert(mono_threads_is_cooperative_suspension_enabled());
}
```

**Single-threaded mode** (`DISABLE_THREADS` defined):
```c
MonoNativeThreadId mono_native_thread_id_get(void) {
    return (MonoNativeThreadId)1;  // Fixed thread ID
}

gboolean mono_native_thread_create(...) {
    g_error("Platform doesn't support threading");
}
```

**With pthreads** (optional threading support):
```c
MonoNativeThreadId mono_native_thread_id_get(void) {
    return pthread_self();
}

gboolean mono_native_thread_create(MonoNativeThreadId *tid, gpointer func, gpointer arg) {
    return pthread_create(tid, NULL, (void *(*)(void *))func, arg) == 0;
}
```

#### Option B: Minimal POSIX Extension

**Best for:** POSIX-like systems needing only platform-specific stack bounds

**Reference:** `mono-threads-haiku.c` (31 lines total)

**Only two functions needed** - everything else comes from POSIX backend:

```c
void mono_threads_platform_get_stack_bounds(guint8 **staddr, size_t *stsize) {
    // Use platform-specific API to get stack base and size
    thread_info ti;
    get_thread_info(get_pthread_thread_id(pthread_self()), &ti);
    *staddr = ti.stack_base;
    *stsize = ti.stack_end - ti.stack_base;
}

guint64 mono_native_thread_os_id_get(void) {
    return (guint64)get_pthread_thread_id(pthread_self());
}
```

#### Option C: Linux Variant with Fallbacks

**Best for:** Linux-based embedded systems with incomplete pthread support

**Reference:** `mono-threads-android.c`

```c
void mono_threads_platform_get_stack_bounds(guint8 **staddr, size_t *stsize) {
    pthread_attr_t attr;
    guint8 *current = (guint8*)&attr;

    // Primary: pthread API
    pthread_getattr_np(pthread_self(), &attr);
    pthread_attr_getstack(&attr, (void**)staddr, stsize);
    pthread_attr_destroy(&attr);

    // Fallback: Parse /proc/self/maps if pthread fails
    if (*staddr && ((current <= *staddr) || (current > *staddr + *stsize)))
        slow_get_thread_bounds(current, staddr, stsize);
}

guint64 mono_native_thread_os_id_get(void) {
    return (guint64)syscall(SYS_gettid);
}
```

### Minimum Threading Functions

| Function | Purpose | Cooperative Mode |
|----------|---------|------------------|
| `mono_threads_platform_get_stack_bounds` | GC stack scanning | **Required** |
| `mono_native_thread_os_id_get` | Debugging/diagnostics | **Required** |
| `mono_native_thread_id_get` | Thread identification | **Required** |
| `mono_native_thread_id_equals` | Thread comparison | Simple: `id1 == id2` |
| `mono_native_thread_create` | Thread spawning | Error or pthread |
| `mono_native_thread_join` | Wait for termination | No-op or pthread |
| `mono_threads_platform_yield` | Yield CPU | `return TRUE;` |
| `mono_threads_suspend_*` | Suspension control | All return TRUE |

### Choosing Your Threading Approach

| Platform Characteristics | Recommended Approach |
|-------------------------|---------------------|
| No signal support | Cooperative-only (WASM model) |
| Single-threaded only | Cooperative + `DISABLE_THREADS` |
| POSIX with full signals | POSIX backend + platform file |
| POSIX without signals | Cooperative-only |
| Has pthreads but unusual stack API | Android model with fallbacks |
| Windows-like | Windows backend |

---

## Phase 3: Machine Context

### Location: `src/mono/mono/utils/mono-context.h`

**Tasks:**
- [ ] Define `MonoContext` structure for your architecture
- [ ] Implement context accessor macros
- [ ] Handle signal context conversion

**Required Macros:**
```c
#define MONO_CONTEXT_SET_IP(ctx, ip)
#define MONO_CONTEXT_SET_SP(ctx, sp)
#define MONO_CONTEXT_SET_BP(ctx, bp)
#define MONO_CONTEXT_GET_IP(ctx)
#define MONO_CONTEXT_GET_SP(ctx)
#define MONO_CONTEXT_GET_BP(ctx)
```

**Example Structure (ARM64):**
```c
struct MonoContext {
    host_mgreg_t gregs[MONO_ARCH_NUM_LMF_REGS];
    gpointer pc;
};
```

**Signal Context** (`mono-sigcontext.h`):
- Define how to extract register state from platform signal handlers
- Map signal context fields to MonoContext

---

## Phase 4: OS Abstractions

### 4a. eglib Platform Layer

**Location:** `src/mono/mono/eglib/`

| File Pattern | Purpose | Reference |
|-------------|---------|-----------|
| `gfile-<platform>.c` | File operations | `gfile-posix.c` |
| `gdate-<platform>.c` | Date/time functions | `gdate-unix.c` |
| `gmisc-<platform>.c` | Miscellaneous utilities | `gmisc-unix.c` |
| `gmodule-<platform>.c` | Dynamic library loading | `gmodule-unix.c` |

### 4b. Low-Level Utilities

**Location:** `src/mono/mono/utils/`

| File Pattern | Purpose |
|-------------|---------|
| `mono-dl-<platform>.c` | dlopen/LoadLibrary equivalent |
| `mono-log-<platform>.c` | Platform logging |
| `mono-os-mutex.c` | Mutex primitives |
| `mono-os-semaphore-<platform>.c` | Semaphore primitives |

**Key APIs to Implement:**
- `mono_dl_open` - Load shared library
- `mono_dl_symbol` - Get symbol address
- `mono_dl_close` - Unload library
- Memory mapping (mmap equivalent)

---

## Phase 5: JIT Compiler (Optional)

> **Note:** You can skip this phase initially by using the interpreter (`MONO_USE_INTERPRETER=1`).

### Location: `src/mono/mono/mini/`

**Files to Create:**

| File | Purpose |
|------|---------|
| `mini-<arch>.h` | Register definitions, calling conventions, stack layout |
| `mini-<arch>.c` | Instruction selection and code emission |
| `exceptions-<arch>.c` | Stack unwinding, exception dispatch |
| `tramp-<arch>.c` | Method call trampolines |
| `cpu-<arch>.h` | CPU descriptor and capabilities |
| `mini-<arch>-gsharedvt.c` | Generic value type support |

**Architecture Registration** (`mini-arch.h`):
```c
#elif defined(TARGET_YOURARCH)
#include "mini-yourarch.h"
```

**CMake Integration** (`src/mono/mono/mini/CMakeLists.txt`):
```cmake
elseif(TARGET_YOURARCH)
  set(arch_sources
    mini-yourarch.c
    mini-yourarch.h
    exceptions-yourarch.c
    tramp-yourarch.c
    cpu-yourarch.h)
endif()
```

**Key JIT Components:**
- Register allocator configuration
- Instruction encoding macros
- Method prologue/epilogue generation
- P/Invoke and managed-to-unmanaged transitions
- Exception handling and stack walking

---

## Phase 6: Garbage Collector Integration

### Location: `src/mono/mono/sgen/`

**Typically Requires Minimal Changes:**
- [ ] Ensure stack walking works with your MonoContext
- [ ] Implement memory allocation (mmap or platform equivalent)
- [ ] Support thread enumeration for GC roots
- [ ] Write barriers (if platform requires special handling)

**Key File:** `sgen-archdep.h` - Architecture-specific GC hooks

---

## Reusable Components (No Changes Needed)

These components are platform-independent:

- **IL Interpreter** - Fully portable bytecode execution
- **Metadata Engine** - Assembly loading and type system
- **Managed Libraries** - System.* and Microsoft.* assemblies
- **SGen GC Core** - Collection algorithms (just needs platform hooks)

---

## Testing Strategy

### Step 1: Minimal Runtime
```bash
# Build with interpreter only
./build.sh mono -c Debug -os <platform> --arch <arch>
```

### Step 2: Hello World
```csharp
// Minimal test program
class Program {
    static void Main() {
        System.Console.WriteLine("Hello from new platform!");
    }
}
```

### Step 3: Basic Libraries
```bash
./build.sh mono+libs -c Release -os <platform> --arch <arch>
```

### Step 4: Test Suites
```bash
# Run Mono runtime tests
cd src/mono && dotnet build /t:test
```

---

## Reference Implementations

| Platform Type | Directory/Files | Good For | Threading Model |
|--------------|-----------------|----------|-----------------|
| Linux (POSIX) | `*-posix.c`, `*-unix.c` | Standard Unix systems | Preemptive (signals) |
| Windows | `*-windows.c`, `*-win32.c` | Non-POSIX systems | Preemptive (SuspendThread) |
| macOS | `*-darwin.c`, `*-mach.c` | Mach kernel systems | Preemptive (Mach ports) |
| Android | `mono-threads-android.c` | Mobile/embedded Linux | Preemptive (signals) |
| WebAssembly | `*-wasm.c` | Sandboxed/constrained | **Cooperative only** |
| WASI | `*-wasm.c` + `HOST_WASI` | Serverless/sandboxed | **Cooperative only** |
| Haiku | `mono-threads-haiku.c` | Alternative POSIX-like | Preemptive (POSIX) |

### File Size Comparison (Lines of Code)

| Implementation | LOC | Complexity |
|----------------|-----|------------|
| `mono-threads-haiku.c` | 31 | Minimal - just stack bounds |
| `mono-threads-android.c` | 74 | Low - stack bounds + fallback |
| `mono-threads-wasm.c` | 663 | Medium - full cooperative impl |
| `mono-threads-posix.c` | ~400 | High - signal handling |
| `mono-threads-windows.c` | ~500 | High - Windows threading |

---

## Key Documentation

- `docs/workflow/building/mono/README.md` - Mono build instructions
- `docs/design/mono/components.md` - Runtime component architecture
- `docs/design/coreclr/botr/` - Book of the Runtime (concepts apply to Mono)

---

## Common Issues and Solutions

### "Shared framework must be built first"
Build runtime and libs together:
```bash
./build.sh mono+libs -c Release
```

### Enabling Cooperative Suspension (for embedded platforms)
If your platform cannot support preemptive suspension (signals), enable cooperative mode:

**Build-time:** Set in CMake configuration
```cmake
set(ENABLE_HYBRID_SUSPEND 0)
set(MONO_FEATURE_THREAD_SUSPEND_COOPERATIVE 1)
```

**Runtime verification:** Your threading init should assert cooperative mode:
```c
void mono_threads_suspend_init(void) {
    g_assert(mono_threads_is_cooperative_suspension_enabled());
}
```

### Thread suspension failures
- Verify signal handling is correctly implemented
- Check thread-local storage initialization
- Ensure context capture works in signal handlers
- **For embedded:** Switch to cooperative suspension if signals aren't available

### JIT crashes
- Start with interpreter to isolate JIT issues
- Verify register allocation constants match ABI
- Check calling convention implementation

### Missing symbols at runtime
- Verify dynamic loader implementation
- Check library search paths
- Ensure P/Invoke resolution works

---

## Implementation Checklist

### Must Have (Interpreter-Only)
- [ ] CMake platform detection
- [ ] Threading layer implementation
- [ ] Machine context structures
- [ ] Basic OS abstractions (file, memory, time)
- [ ] Dynamic library loading
- [ ] Mutex/semaphore primitives

### Threading Layer Details

**For Cooperative Suspension (embedded/constrained):**
- [ ] `mono_threads_platform_get_stack_bounds` - Stack base and size
- [ ] `mono_native_thread_os_id_get` - OS thread identifier
- [ ] `mono_native_thread_id_get` - Native thread ID (can be fixed `1` if single-threaded)
- [ ] `mono_native_thread_id_equals` - Thread comparison
- [ ] `mono_threads_suspend_init` - Assert cooperative mode enabled
- [ ] All `mono_threads_suspend_*` functions return TRUE (no-ops)
- [ ] Enable `MONO_FEATURE_THREAD_SUSPEND_COOPERATIVE` in build

**For Preemptive Suspension (full POSIX/Windows):**
- [ ] All of the above, plus:
- [ ] Signal handlers (POSIX) or SuspendThread (Windows)
- [ ] `mono_threads_suspend_begin_async_suspend` - Actual suspension
- [ ] `mono_threads_suspend_check_suspend_result` - Verify suspended
- [ ] `mono_threads_suspend_begin_async_resume` - Resume thread
- [ ] Signal context to MonoContext conversion

### Required for JIT
- [ ] Architecture header (`mini-<arch>.h`)
- [ ] Code generator (`mini-<arch>.c`)
- [ ] Exception handling (`exceptions-<arch>.c`)
- [ ] Trampolines (`tramp-<arch>.c`)
- [ ] CPU descriptor (`cpu-<arch>.h`)

### Optional Enhancements
- [ ] AOT compilation support
- [ ] Debugger agent integration
- [ ] Profiler support
- [ ] Hot reload support
- [ ] EventPipe diagnostics

---

## Estimated Effort

| Component | Complexity | Can Reference |
|-----------|------------|---------------|
| Build system | Low | Any existing platform |
| Threading (cooperative) | **Low** | WASM impl (~50 lines core) |
| Threading (preemptive) | High | POSIX or Windows impl |
| Context/ABI | Medium | Same architecture on different OS |
| OS abstractions | Medium | Similar OS family |
| JIT backend | Very High | Same architecture |
| Interpreter-only | N/A | Works out of box |

### Embedded Platform Fast Path

For embedded/constrained platforms, the minimal path is:

1. **Build system** - Add platform detection (~20 lines CMake)
2. **Cooperative threading** - Copy WASM pattern (~50-100 lines)
3. **Stack bounds** - Implement one function (platform-specific)
4. **Use interpreter** - Skip JIT entirely

**Total estimated new code:** 100-200 lines for minimal embedded port

**Recommendation:** Start with interpreter-only mode and cooperative threading to validate the OS abstraction layer before tackling preemptive suspension or JIT implementation.
