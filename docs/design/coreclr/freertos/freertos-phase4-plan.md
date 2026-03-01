# Phase 4: FreeRTOS ARM Assembly Helpers - Implementation Plan

## Overview

Phase 4 implements the 9 ARM assembly files required for NativeAOT runtime execution on FreeRTOS bare-metal systems. These files provide critical low-level functionality for:
- Memory allocation (fast paths)
- Garbage collection (write barriers, suspension points)
- Exception handling and unwinding
- Virtual method dispatch
- Managed-to-native transitions (P/Invoke)

**Target**: ARM Cortex-M (ARM32 Thumb-2 instruction set)

## Prerequisites

- Phase 3 complete ✅
- ARM Assembly knowledge (AAPCS calling convention)
- Understanding of ARM Cortex-M exception model
- FreeRTOS task context structure
- Access to ARM development board or QEMU

## Assembly Files to Implement (9 total)

### Priority 1: Critical for GC (Required First)

#### 1.1 WriteBarriers.S ⭐⭐⭐ HIGHEST PRIORITY
**Location**: `src/coreclr/nativeaot/Runtime/arm/WriteBarriers.S` → `freertos/WriteBarriers.S`

**Purpose**: GC write barriers ensure heap consistency when managed references are written

**Functions to Implement**:
```asm
// Primary write barrier - called by JIT for reference stores
JIT_WriteBarrier:
    // Input: R0 = destination address
    //        R1 = object reference being stored
    // Postcondition: Reference stored, card table updated if needed

// Checked write barrier - with null check
JIT_CheckedWriteBarrier:
    // Same as above but checks if source is null first

// Byte ranged write barrier - for array copies
JIT_ByRefWriteBarrier:
    // Input: R0 = destination byref
    //        R1 = value to write
```

**Key Implementation Details**:
- Must update GC card table to mark memory region as dirty
- Card table base address from global variable `g_card_table`
- Card size typically 512 bytes (adjust based on GC configuration)
- Must handle both WriteWatch and non-WriteWatch modes
- Critical path: Must be fast (inlined by JIT frequently)

**Algorithm**:
```asm
JIT_WriteBarrier:
    // 1. Store the reference
    str     r1, [r0]

    // 2. Calculate card table entry
    ldr     r2, =g_card_table
    ldr     r2, [r2]            // Load card table base
    lsr     r3, r0, #9          // Divide address by 512 (card size)
    add     r2, r2, r3          // Card table entry address

    // 3. Mark card as dirty
    mov     r3, #0xFF
    strb    r3, [r2]            // Set card table byte

    // 4. Return
    bx      lr
```

**Existing Reference**: Study `src/coreclr/nativeaot/Runtime/arm/WriteBarriers.asm` (Windows MASM syntax)

**Testing**:
- Allocate objects and assign references
- Verify card table updates with GC stress tests
- Performance: Should be ~10-15 ARM instructions

**Complexity**: Medium
**Estimated Effort**: 2-3 days
**Blocking**: All managed code that allocates or assigns references

---

#### 1.2 GcProbe.S ⭐⭐⭐ CRITICAL
**Location**: `src/coreclr/nativeaot/Runtime/arm/GcProbe.S` → `freertos/GcProbe.S`

**Purpose**: GC suspension points - allows GC to suspend threads and scan stacks

**Functions to Implement**:
```asm
// Main GC poll/probe - called at method prologues and loop backedges
RhpGcPoll:
    // Check if GC is pending
    // If yes, suspend thread and wait for GC to complete

// GC poll for loops
RhpGcPollRare:
    // Less frequent polling version

// GC suspension helper
RhpWaitForGC:
    // Suspend current thread
    // Cooperate with GC
    // Resume when GC completes
```

**Key Implementation Details**:
- Check global GC trap flag
- If GC pending, call suspension helper
- Suspension helper must:
  1. Save all callee-saved registers
  2. Call into runtime to report suspension
  3. Wait for GC completion
  4. Restore registers and return
- Must preserve all registers across suspension
- Critical for GC correctness

**Algorithm**:
```asm
RhpGcPoll:
    // 1. Load GC trap address
    ldr     r0, =RhpTrapThreads
    ldr     r0, [r0]

    // 2. Check trap flag
    ldr     r0, [r0]
    cmp     r0, #0
    beq     .LNoTrap            // If not trapped, return

    // 3. GC is pending - suspend
    b       RhpWaitForGC

.LNoTrap:
    bx      lr

RhpWaitForGC:
    // Save all registers
    push    {r0-r12, lr}

    // Call runtime suspension helper
    bl      RhpWaitForGCWorker  // C++ function

    // Restore and return
    pop     {r0-r12, pc}
```

**Testing**:
- Trigger GC from managed code
- Verify thread suspension/resumption
- Stress test with allocations

**Complexity**: Medium-High
**Estimated Effort**: 3-4 days
**Blocking**: Garbage collection cannot work without this

---

### Priority 2: Performance Critical

#### 2.1 AllocFast.S ⭐⭐ HIGH PRIORITY
**Location**: `src/coreclr/nativeaot/Runtime/arm/AllocFast.S` → `freertos/AllocFast.S`

**Purpose**: Fast path object allocation - bypasses slow allocation helpers when possible

**Functions to Implement**:
```asm
// Fast path for allocating small objects
RhpNewFast:
    // Input: R0 = MethodTable pointer
    // Output: R0 = allocated object (or null if fast path fails)

// Fast path for arrays
RhpNewArrayFast:
    // Input: R0 = MethodTable pointer
    //        R1 = element count
    // Output: R0 = allocated array (or null if fast path fails)
```

**Key Implementation Details**:
- Try to allocate from thread-local allocation context (alloc_ptr, alloc_limit)
- If sufficient space, bump allocate and return
- If not enough space, return null (slow path will handle)
- Must initialize object header (MethodTable pointer, sync block)
- Must account for alignment (8-byte on ARM32)

**Algorithm**:
```asm
RhpNewFast:
    // 1. Load allocation context
    ldr     r1, =tls_alloc_context  // Thread-local storage
    ldr     r2, [r1, #0]            // alloc_ptr
    ldr     r3, [r1, #4]            // alloc_limit

    // 2. Calculate object size from MethodTable
    ldr     r12, [r0, #METHODTABLE_BASE_SIZE_OFFSET]

    // 3. Check if enough space
    add     r12, r2, r12            // new_alloc_ptr = alloc_ptr + size
    cmp     r12, r3
    bhi     .LSlowPath              // If over limit, fail

    // 4. Allocate (bump pointer)
    str     r12, [r1, #0]           // Update alloc_ptr

    // 5. Initialize object header
    str     r0, [r2, #0]            // Store MethodTable pointer
    mov     r0, r2                  // Return object address
    bx      lr

.LSlowPath:
    mov     r0, #0                  // Return null
    bx      lr
```

**Testing**:
- Allocate many small objects
- Verify object layout and header
- Performance: Should be ~20-30 instructions

**Complexity**: Medium
**Estimated Effort**: 2-3 days
**Impact**: Performance optimization (not blocking correctness)

---

### Priority 3: Core Functionality

#### 3.1 MiscStubs.S ⭐⭐ MEDIUM PRIORITY
**Location**: `src/coreclr/nativeaot/Runtime/arm/MiscStubs.S` → `freertos/MiscStubs.S`

**Purpose**: Miscellaneous runtime helper stubs

**Functions to Implement**:
```asm
// Call counting stubs (for tiered compilation - may not be needed for bare-metal)
RhpCallCounting:
RhpCallCountingIncrement:

// Dispatch stubs
RhpResolveInterfaceMethod:
RhpResolveVirtualMethod:

// Type casting helpers
RhpStelemRef:           // Array element store with type check
RhpLdelemaRef:          // Array element address load
```

**Testing**:
- Interface method calls
- Virtual method calls
- Array operations with reference types

**Complexity**: Low-Medium
**Estimated Effort**: 2-3 days

---

#### 3.2 StubDispatch.S ⭐⭐ MEDIUM PRIORITY
**Location**: `src/coreclr/nativeaot/Runtime/arm/StubDispatch.S` → `freertos/StubDispatch.S`

**Purpose**: Virtual stub dispatch for interface and virtual calls

**Functions to Implement**:
```asm
// Virtual stub dispatch
VSD_ResolveStub:
    // Input: Interface/virtual method call
    // Output: Resolved method address

// Interface dispatch
RhpInterfaceDispatch:
    // Input: R0 = this pointer
    //        Interface method token
    // Output: Method address
```

**Testing**:
- Interface method calls
- Virtual method calls through base class
- Performance benchmarks

**Complexity**: Medium-High
**Estimated Effort**: 3-4 days

---

### Priority 4: Interop and Transitions

#### 4.1 PInvoke.S ⭐ LOWER PRIORITY
**Location**: `src/coreclr/nativeaot/Runtime/arm/PInvoke.S` → `freertos/PInvoke.S`

**Purpose**: Managed-to-native transitions for P/Invoke calls

**Functions to Implement**:
```asm
// Enter native code (P/Invoke entry)
RhpPInvoke:
    // 1. Transition thread to preemptive mode
    // 2. Call native function
    // 3. Transition back to cooperative mode

// Reverse P/Invoke (native calling managed)
RhpReversePInvoke:
RhpReversePInvokeReturn:
```

**Key Implementation Details**:
- Transition GC mode (cooperative → preemptive)
- Save managed context
- Call native function (may block)
- Restore managed context
- Transition back (preemptive → cooperative)

**Testing**:
- Call native C functions from managed code
- Pass various parameter types
- Verify GC can't corrupt during native calls

**Complexity**: Medium-High
**Estimated Effort**: 3-4 days
**Note**: May be lower priority if P/Invoke not needed initially

---

#### 4.2 UniversalTransition.S ⭐ LOWER PRIORITY
**Location**: `src/coreclr/nativeaot/Runtime/arm/UniversalTransition.S` → `freertos/UniversalTransition.S`

**Purpose**: Generic transition mechanism for various runtime operations

**Functions to Implement**:
```asm
// Universal transition thunk
RhpUniversalTransition:
    // Generic wrapper for transitions
    // Used by runtime for various callbacks
```

**Complexity**: High
**Estimated Effort**: 4-5 days
**Note**: Complex, may defer to later in Phase 4

---

### Priority 5: Exception Handling (Phase 5 Dependency)

#### 5.1 ExceptionHandling.S ⭐ DEFER TO PHASE 5
**Location**: `src/coreclr/nativeaot/Runtime/arm/ExceptionHandling.S` → `freertos/ExceptionHandling.S`

**Purpose**: Exception dispatch and handling for managed exceptions

**Functions to Implement**:
```asm
// Throw managed exception
RhThrowEx:
RhThrowHwEx:            // Hardware exception wrapper

// Exception dispatch
RhpCallCatchFunclet:    // Call catch handler
RhpCallFinallyFunclet:  // Call finally handler
RhpCallFilterFunclet:   // Call exception filter

// Stack unwinding
RhpUnwindFunclet:
```

**Dependencies**: Requires Phase 5 (Hardware Exception Support) design

**Complexity**: Very High
**Estimated Effort**: 5-7 days
**Recommendation**: Implement after WriteBarriers, GcProbe, AllocFast are working

---

#### 5.2 InteropThunksHelpers.S ❓ MAY NOT BE NEEDED
**Location**: `src/coreclr/nativeaot/Runtime/arm/InteropThunksHelpers.S` → `freertos/InteropThunksHelpers.S`

**Purpose**: COM interop helpers (likely not needed for bare-metal)

**Recommendation**: Skip for FreeRTOS bare-metal. COM/WinRT not applicable.

---

## Implementation Approach

### Step 1: Setup Development Environment

1. **Install Tools**:
   ```bash
   # ARM GNU Toolchain
   # OpenOCD for debugging
   # QEMU for emulation (optional)
   ```

2. **Get Hardware**: ARM Cortex-M dev board (STM32F4 Discovery recommended)

3. **Create Test Project**: Minimal FreeRTOS + NativeAOT test harness

### Step 2: Study Existing Implementations

Before implementing, study these references:

1. **Windows MASM Versions** (`src/coreclr/nativeaot/Runtime/arm/*.asm`):
   - Original implementations for Windows ARM
   - Understand algorithm and data structures

2. **Unix GAS Versions** (`src/coreclr/nativeaot/Runtime/arm/*.S`):
   - Similar but uses GAS syntax (closer to bare-metal)
   - May have Linux-specific macros to adapt

3. **ARM Calling Convention**:
   - [AAPCS](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst)
   - Register usage: R0-R3 (arguments), R4-R11 (callee-saved), R12 (IP), R13 (SP), R14 (LR), R15 (PC)

4. **GC Documentation**:
   - [Book of the Runtime - Garbage Collection](../../../docs/design/coreclr/botr/garbage-collection.md)
   - Understand write barriers, card tables, allocation contexts

### Step 3: Create Template Files

Create template for each assembly file:

```asm
// FreeRTOS bare-metal ARM assembly
// Based on: src/coreclr/nativeaot/Runtime/arm/[original].S

.syntax unified         // Use unified ARM/Thumb syntax
.thumb                  // Thumb-2 instruction set (Cortex-M)

// Function: [Name]
// Purpose: [Description]
// Arguments:
//   R0 = [arg0]
//   R1 = [arg1]
// Returns:
//   R0 = [result]
// Preserved: R4-R11
.global [FunctionName]
.type [FunctionName], %function
[FunctionName]:
    // Implementation
    bx lr
.size [FunctionName], .-[FunctionName]
```

### Step 4: Implement in Priority Order

**Week 1-2**: WriteBarriers.S + GcProbe.S
- Most critical for GC functionality
- Test with simple allocation/collection cycles

**Week 3**: AllocFast.S
- Performance optimization
- Test with allocation stress tests

**Week 4**: MiscStubs.S + StubDispatch.S
- Interface/virtual calls
- Test with polymorphism

**Week 5-6**: PInvoke.S + UniversalTransition.S
- Native interop
- Test with native library calls

**Week 7+**: ExceptionHandling.S (Phase 5 overlap)
- Complex exception handling
- Requires Phase 5 design decisions

### Step 5: Testing Strategy

For each assembly file:

1. **Unit Test**: Create minimal C# test that exercises the function
   ```csharp
   class Test {
       static void TestWriteBarrier() {
           object a = new object();
           object b = new object();
           // Assignment triggers write barrier
           a = b;
       }
   }
   ```

2. **Hardware Test**: Run on real ARM board with debugger
   - Set breakpoints in assembly
   - Verify register states
   - Check card table updates

3. **QEMU Test**: Run in emulator for faster iteration
   ```bash
   qemu-system-arm -machine lm3s6965evb -kernel test.elf -semihosting -nographic
   ```

4. **GC Stress Test**: Run with frequent GC to expose bugs
   ```csharp
   for (int i = 0; i < 10000; i++) {
       object[] arr = new object[100];
       GC.Collect();
   }
   ```

### Step 6: Integration Testing

After all priority 1-3 files implemented:

1. **Build Complete Runtime**: Link all assembly files
2. **Run Managed Application**: Execute real C# code
3. **Performance Benchmarks**: Measure allocation, GC, dispatch overhead
4. **Stress Tests**: Run for hours with allocation/collection cycles

### Step 7: Documentation

For each implemented file, document:
- Function signatures and register usage
- Algorithm and data structure dependencies
- Testing approach and results
- Performance characteristics
- Known limitations or TODOs

## Success Criteria

Phase 4 complete when:

✅ All 7-8 assembly files implemented (excluding InteropThunksHelpers if not needed)
✅ Simple managed application runs on hardware
✅ GC works correctly (allocate, collect, no corruption)
✅ Interface/virtual calls dispatch correctly
✅ P/Invoke works (if needed)
✅ Unit tests pass for all functions
✅ No memory corruption under stress testing
✅ Performance acceptable (compared to C++ equivalent)

## Estimated Timeline

- **Minimum (priority 1-2 only)**: 2-3 weeks (1 developer)
- **Full Phase 4 (priority 1-4)**: 6-8 weeks (1 developer)
- **With Exception Handling**: 8-10 weeks (overlaps with Phase 5)

## Common Pitfalls to Avoid

1. **Wrong Calling Convention**: AAPCS is strict, follow it exactly
2. **Register Corruption**: Save/restore callee-saved registers (R4-R11)
3. **Alignment Issues**: ARM requires aligned access, especially for stack
4. **GC Hole**: Missing write barrier or GC probe can corrupt heap
5. **Stack Overflow**: FreeRTOS tasks have limited stack, watch recursion
6. **Thumb Mode**: Ensure .thumb directive, use BX not B for returns
7. **Card Table**: Wrong card size or base address corrupts GC
8. **Race Conditions**: Atomic operations needed for GC flags

## Resources

- **ARM Assembly**: [ARM Developer Documentation](https://developer.arm.com/documentation/)
- **AAPCS**: [Procedure Call Standard](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst)
- **GC Internals**: [Book of the Runtime - GC](../../../docs/design/coreclr/botr/garbage-collection.md)
- **Existing Code**: `src/coreclr/nativeaot/Runtime/arm/` (Windows/Unix versions)
- **FreeRTOS**: [FreeRTOS Documentation](https://www.freertos.org/Documentation/)

## Next Steps After Phase 4

Once assembly helpers complete:
- **Phase 5**: Hardware exception integration
- **Phase 6**: Multi-threading support
- **Phase 7**: Testing and validation on real applications

---

Created: 2026-02-13
Status: Phase 3 Complete, Phase 4 Ready to Start
