# Phase 4 Implementation Checklist

Track progress implementing FreeRTOS ARM assembly helpers. Check off items as completed.

## Development Environment Setup

- [ ] ARM GNU Toolchain installed (13.3.1+)
- [ ] OpenOCD configured for target board
- [ ] GDB ARM debugging working
- [ ] ARM Cortex-M development board available
  - [ ] Board type: ________________
  - [ ] RAM size: ________________
  - [ ] Flash size: ________________
- [ ] QEMU ARM emulation working (optional)
- [ ] Serial console configured (UART)
- [ ] Test FreeRTOS project building
- [ ] Phase 3 runtime libraries building successfully

## Documentation Review

- [ ] Read [AAPCS](https://github.com/ARM-software/abi-aa/blob/main/aapcs32/aapcs32.rst) calling convention
- [ ] Review [Garbage Collection BOTR](../../../docs/design/coreclr/botr/garbage-collection.md)
- [ ] Study existing ARM assembly: `src/coreclr/nativeaot/Runtime/arm/*.asm`
- [ ] Study existing Unix assembly: `src/coreclr/nativeaot/Runtime/arm/*.S`
- [ ] Understand FreeRTOS task context structure
- [ ] Review ARM Cortex-M exception model

## Priority 1: Critical GC Support (REQUIRED)

### WriteBarriers.S ⭐⭐⭐

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/WriteBarriers.S`
- [ ] `JIT_WriteBarrier` implemented
  - [ ] Reference store (str r1, [r0])
  - [ ] Card table calculation (divide by card size)
  - [ ] Card marking (store dirty byte)
  - [ ] Register preservation verified
- [ ] `JIT_CheckedWriteBarrier` implemented
  - [ ] Null check before barrier
  - [ ] Optimization for common case
- [ ] `JIT_ByRefWriteBarrier` implemented (for array copies)
- [ ] Global variables accessible
  - [ ] `g_card_table` base address
  - [ ] `g_ephemeral_low` / `g_ephemeral_high` (if using)

#### Testing
- [ ] Unit test: Simple object assignment
  ```csharp
  object a = new object();
  object b = new object();
  a = b; // Triggers write barrier
  ```
- [ ] Test: Card table inspection (verify dirty marking)
- [ ] Test: Array of objects with assignments
- [ ] Test: GC collection after many assignments
- [ ] Stress test: 10,000+ assignments
- [ ] Performance: Measure write barrier overhead
- [ ] Hardware debug: Step through with GDB
- [ ] Verify: No register corruption (R4-R11 preserved)

#### Validation
- [ ] No crashes during object assignment
- [ ] Card table correctly marked
- [ ] GC successfully collects after assignments
- [ ] Performance acceptable (< 20 instructions)
- [ ] **APPROVAL**: Write barriers working ✅

---

### GcProbe.S ⭐⭐⭐

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/GcProbe.S`
- [ ] `RhpGcPoll` implemented
  - [ ] Load trap flag address
  - [ ] Check trap flag
  - [ ] Branch to suspension if trapped
  - [ ] Fast path optimized (no trap case)
- [ ] `RhpGcPollRare` implemented (variant for loops)
- [ ] `RhpWaitForGC` implemented
  - [ ] Save all registers (R0-R12, LR)
  - [ ] Call `RhpWaitForGCWorker` (C++ helper)
  - [ ] Restore all registers
  - [ ] Return to caller
- [ ] Global variables accessible
  - [ ] `RhpTrapThreads` flag address

#### Testing
- [ ] Unit test: Trigger GC from managed code
  ```csharp
  GC.Collect(); // Should poll and suspend
  ```
- [ ] Test: GC suspension and resumption
- [ ] Test: Multiple GC cycles
- [ ] Test: GC during allocation loop
- [ ] Stress test: Rapid GC triggers
- [ ] Hardware debug: Verify register saves
- [ ] Verify: Thread resumes correctly after GC

#### Validation
- [ ] GC can suspend managed thread
- [ ] Thread resumes after GC completes
- [ ] No register corruption across suspension
- [ ] No deadlocks or hangs
- [ ] **APPROVAL**: GC probes working ✅

---

## Priority 2: Performance Optimization

### AllocFast.S ⭐⭐

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/AllocFast.S`
- [ ] `RhpNewFast` implemented (object allocation)
  - [ ] Load thread allocation context (alloc_ptr, alloc_limit)
  - [ ] Read object size from MethodTable
  - [ ] Check sufficient space
  - [ ] Bump allocate if space available
  - [ ] Initialize object header (MethodTable pointer)
  - [ ] Return object or null (slow path)
- [ ] `RhpNewArrayFast` implemented (array allocation)
  - [ ] Similar logic for arrays
  - [ ] Calculate array size (base + elements * element_size)
  - [ ] Alignment handling (8-byte align)
- [ ] Thread-local storage access
  - [ ] TLS allocation context structure defined
  - [ ] Access pattern tested on FreeRTOS

#### Testing
- [ ] Unit test: Allocate single object
  ```csharp
  object obj = new object();
  ```
- [ ] Test: Allocate many small objects
- [ ] Test: Allocate arrays of various sizes
- [ ] Test: Verify object layout (MethodTable pointer correct)
- [ ] Test: Fast path success rate (should be >95%)
- [ ] Test: Slow path fallback works
- [ ] Performance: Measure allocation time
  - [ ] Fast path: ____ cycles
  - [ ] Slow path: ____ cycles

#### Validation
- [ ] Objects allocated correctly
- [ ] No memory corruption
- [ ] Performance improvement measurable
- [ ] **APPROVAL**: Fast allocation working ✅

---

## Priority 3: Core Functionality

### MiscStubs.S ⭐⭐

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/MiscStubs.S`
- [ ] `RhpStelemRef` implemented (array element store with type check)
  - [ ] Type compatibility check
  - [ ] Store element if compatible
  - [ ] Throw exception if incompatible
- [ ] `RhpLdelemaRef` implemented (array element address)
- [ ] Call counting stubs (if needed)
  - [ ] `RhpCallCounting`
  - [ ] `RhpCallCountingIncrement`
- [ ] Other misc helpers identified and implemented

#### Testing
- [ ] Test: Store reference in array
  ```csharp
  object[] arr = new object[10];
  arr[0] = new object();
  ```
- [ ] Test: Type safety violation throws exception
  ```csharp
  string[] arr = new string[10];
  arr[0] = new object(); // Should throw
  ```
- [ ] Test: Various array types (int[], string[], object[])

#### Validation
- [ ] Array operations work correctly
- [ ] Type safety enforced
- [ ] **APPROVAL**: Misc stubs working ✅

---

### StubDispatch.S ⭐⭐

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/StubDispatch.S`
- [ ] `VSD_ResolveStub` implemented (virtual stub dispatch)
- [ ] `RhpInterfaceDispatch` implemented (interface method dispatch)
  - [ ] Load interface map from MethodTable
  - [ ] Search for matching interface
  - [ ] Resolve method address
  - [ ] Jump to method
- [ ] Virtual method dispatch tested

#### Testing
- [ ] Test: Interface method call
  ```csharp
  IDisposable obj = new MyDisposable();
  obj.Dispose(); // Interface call
  ```
- [ ] Test: Virtual method call
  ```csharp
  Base obj = new Derived();
  obj.VirtualMethod(); // Virtual call
  ```
- [ ] Test: Multiple interface implementations
- [ ] Performance: Measure dispatch overhead

#### Validation
- [ ] Interface calls dispatch correctly
- [ ] Virtual calls dispatch correctly
- [ ] Polymorphism works as expected
- [ ] **APPROVAL**: Dispatch working ✅

---

## Priority 4: Interop and Transitions

### PInvoke.S ⭐

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/PInvoke.S`
- [ ] `RhpPInvoke` implemented (managed → native)
  - [ ] Transition to preemptive mode
  - [ ] Save managed context
  - [ ] Call native function
  - [ ] Restore managed context
  - [ ] Transition back to cooperative mode
- [ ] `RhpReversePInvoke` implemented (native → managed)
- [ ] `RhpReversePInvokeReturn` implemented

#### Testing
- [ ] Test: Call native C function
  ```csharp
  [DllImport("*")]
  static extern int printf(string format, __arglist);

  printf("Hello from managed!\n");
  ```
- [ ] Test: Pass various parameter types
- [ ] Test: Return values
- [ ] Test: GC during native call (should be safe in preemptive mode)

#### Validation
- [ ] P/Invoke calls work
- [ ] Parameters passed correctly
- [ ] Return values correct
- [ ] GC safety maintained
- [ ] **APPROVAL**: P/Invoke working ✅

---

### UniversalTransition.S ⭐

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/UniversalTransition.S`
- [ ] `RhpUniversalTransition` implemented
  - [ ] Generic transition mechanism
  - [ ] Parameter marshalling
  - [ ] Return value handling
- [ ] Used by runtime for callbacks

#### Testing
- [ ] Test: Runtime callbacks work
- [ ] Test: Delegate invocations
- [ ] Stress test: Many transitions

#### Validation
- [ ] Transitions work correctly
- [ ] No corruption across transitions
- [ ] **APPROVAL**: Universal transitions working ✅

---

## Priority 5: Exception Handling (Phase 5 Overlap)

### ExceptionHandling.S ⭐ (Defer until Phase 5 design complete)

#### Implementation
- [ ] File created: `src/coreclr/nativeaot/Runtime/freertos/ExceptionHandling.S`
- [ ] `RhThrowEx` implemented (throw managed exception)
- [ ] `RhThrowHwEx` implemented (hardware exception wrapper)
- [ ] `RhpCallCatchFunclet` implemented
- [ ] `RhpCallFinallyFunclet` implemented
- [ ] `RhpCallFilterFunclet` implemented
- [ ] `RhpUnwindFunclet` implemented

#### Testing
- [ ] Test: Throw and catch exception
  ```csharp
  try {
      throw new Exception("Test");
  } catch (Exception e) {
      // Should catch here
  }
  ```
- [ ] Test: Finally blocks execute
- [ ] Test: Exception filters
- [ ] Test: Nested try/catch
- [ ] Test: Hardware exception (null reference, etc.)

#### Validation
- [ ] Exceptions throw correctly
- [ ] Catch blocks execute
- [ ] Finally blocks execute
- [ ] Stack unwinding works
- [ ] Hardware exceptions caught
- [ ] **APPROVAL**: Exception handling working ✅

---

## InteropThunksHelpers.S ❓ (Skip for FreeRTOS)

- [ ] **DECISION**: Not needed for bare-metal FreeRTOS
- [ ] OR: Identify if any functions needed for FreeRTOS
- [ ] If needed: Implement minimal subset

---

## Integration Testing

### Build Integration
- [ ] All assembly files compile without errors
- [ ] Link with Phase 3 C/C++ runtime libraries
- [ ] No undefined symbols
- [ ] Binary size reasonable for target MCU

### Functional Testing
- [ ] **Minimal App**: "Hello World" runs
  ```csharp
  class Program {
      static void Main() {
          Console.WriteLine("Hello from FreeRTOS!");
      }
  }
  ```
- [ ] **Allocation Test**: Create many objects
- [ ] **GC Test**: Trigger collections, verify correctness
- [ ] **Interface Test**: Interface method calls work
- [ ] **Virtual Test**: Virtual method calls work
- [ ] **P/Invoke Test**: Call native functions
- [ ] **Array Test**: Array operations work
- [ ] **Exception Test**: Throw/catch works (if Phase 5 done)

### Stress Testing
- [ ] Run for 1 hour continuously
- [ ] Allocate 1,000,000+ objects
- [ ] Trigger 1,000+ GC cycles
- [ ] No crashes, hangs, or corruption
- [ ] Memory usage stable (no leaks)

### Performance Testing
- [ ] Allocation benchmark: ____ allocs/second
- [ ] GC benchmark: ____ collections/second
- [ ] Interface dispatch: ____ calls/second
- [ ] Virtual dispatch: ____ calls/second
- [ ] Write barrier overhead: ____ % of assignment time

### Hardware Validation
- [ ] Tested on: ________________ (board type)
- [ ] RAM usage: ____ KB (measure actual)
- [ ] Flash usage: ____ KB (measure actual)
- [ ] Stack usage: ____ KB per task
- [ ] No hard faults or exceptions
- [ ] Stable for extended run (24+ hours)

---

## Documentation

- [ ] Each assembly file has header comment explaining:
  - [ ] Purpose and functions
  - [ ] Register usage and calling convention
  - [ ] Data structure dependencies
  - [ ] Known limitations
- [ ] Update `freertos-nativeaot-status.md` with Phase 4 completion
- [ ] Update `freertos-phase4-plan.md` with lessons learned
- [ ] Create troubleshooting guide for common issues
- [ ] Document performance characteristics
- [ ] Create example application repository

---

## Code Review

- [ ] Assembly code follows AAPCS strictly
- [ ] All callee-saved registers preserved (R4-R11)
- [ ] Stack alignment maintained (8-byte)
- [ ] No undefined behavior
- [ ] Comments explain non-obvious code
- [ ] Error handling appropriate
- [ ] Performance optimized (hot paths)
- [ ] Code review by another developer
- [ ] Static analysis passed (if available)

---

## Phase 4 Completion Criteria

✅ **Phase 4 COMPLETE** when ALL of the following are true:

- [ ] **Priority 1** (WriteBarriers + GcProbe) working perfectly
- [ ] **Priority 2** (AllocFast) working and providing performance benefit
- [ ] **Priority 3** (MiscStubs + StubDispatch) working correctly
- [ ] **Priority 4** (PInvoke + UniversalTransition) working (or marked not needed)
- [ ] Managed application runs on hardware without crashes
- [ ] GC works correctly (no corruption under stress)
- [ ] All tests passing
- [ ] Stress tests stable (24+ hours)
- [ ] Documentation complete
- [ ] Code reviewed and approved

---

## Known Issues / TODOs

Document any issues or TODOs discovered during implementation:

### Issue #1: [Title]
- **Description**:
- **Workaround**:
- **Resolution**:

### Issue #2: [Title]
- **Description**:
- **Workaround**:
- **Resolution**:

---

## Time Tracking

Track actual time spent (helpful for future estimates):

| Task | Estimated | Actual | Notes |
|------|-----------|--------|-------|
| Environment setup | 2 days | ____ | |
| WriteBarriers.S | 3 days | ____ | |
| GcProbe.S | 4 days | ____ | |
| AllocFast.S | 3 days | ____ | |
| MiscStubs.S | 3 days | ____ | |
| StubDispatch.S | 4 days | ____ | |
| PInvoke.S | 4 days | ____ | |
| UniversalTransition.S | 5 days | ____ | |
| ExceptionHandling.S | 7 days | ____ | |
| Integration testing | 5 days | ____ | |
| Documentation | 2 days | ____ | |
| **TOTAL** | **42 days** | **____** | |

---

## Sign-Off

Phase 4 implemented by: ___________________________

Date started: _______________

Date completed: _______________

Tested on hardware: ___________________________

Reviewed by: ___________________________

**PHASE 4 STATUS**: ⏳ In Progress / ✅ Complete

---

Last Updated: 2026-02-13
