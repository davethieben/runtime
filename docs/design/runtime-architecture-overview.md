# .NET Runtime Architecture Overview

This document provides a high-level visual overview of the dotnet/runtime repository structure and CoreCLR's internal architecture.

## Repository Structure

```
┌────────────────────────────────────────────────────────────────────────────┐
│                            dotnet/runtime                                  │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                           src/                                      │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │                                                                     │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐   │   │
│  │  │   coreclr/   │  │    mono/     │  │      libraries/          │   │   │
│  │  │              │  │              │  │                          │   │   │
│  │  │  Primary     │  │  Lightweight │  │  246+ managed libraries  │   │   │
│  │  │  .NET        │  │  runtime for │  │  (System.*, Microsoft.*) │   │   │
│  │  │  execution   │  │  mobile,     │  │                          │   │   │
│  │  │  engine      │  │  WASM, IoT   │  │  • Collections           │   │   │
│  │  │              │  │              │  │  • IO, Net, Http         │   │   │
│  │  │  • JIT       │  │  • Interpreter  │  • Text, Xml, Json       │   │   │
│  │  │  • GC        │  │  • AOT       │  │  • Threading             │   │   │
│  │  │  • Type Sys  │  │  • GC        │  │  • Linq, Reflection      │   │   │
│  │  └──────────────┘  └──────────────┘  └──────────────────────────┘   │   │
│  │                                                                     │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────────┐   │   │
│  │  │  installer/  │  │   native/    │  │        tests/            │   │   │
│  │  │              │  │              │  │                          │   │   │
│  │  │  MSI, PKG,   │  │  corehost/   │  │  Runtime test suites     │   │   │
│  │  │  DEB, RPM    │  │  (dotnet     │  │  and infrastructure      │   │   │
│  │  │  installers  │  │   host)      │  │                          │   │   │
│  │  └──────────────┘  └──────────────┘  └──────────────────────────┘   │   │
│  │                                                                     │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                            │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                           docs/                                     │   │
│  ├─────────────────────────────────────────────────────────────────────┤   │
│  │  workflow/          - Build and test instructions                   │   │
│  │  design/            - Architecture and design docs                  │   │
│  │  design/coreclr/botr - Book of the Runtime (deep internals)         │   │
│  │  coding-guidelines/ - Code style and conventions                    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

## CoreCLR Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                              CoreCLR                                        │
│                        (src/coreclr/)                                       │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    MANAGED CODE LAYER                                │   │
│  │  ┌───────────────────────────────────────────────────────────────┐  │   │
│  │  │              System.Private.CoreLib                            │  │   │
│  │  │         (src/coreclr/System.Private.CoreLib/)                  │  │   │
│  │  │                                                                │  │   │
│  │  │  Lowest-level managed library tightly coupled to the runtime  │  │   │
│  │  │  • System.Object, System.String, System.Array                 │  │   │
│  │  │  • Primitive types, Threading, Reflection                     │  │   │
│  │  │  • GC interaction, RuntimeHelpers                             │  │   │
│  │  └───────────────────────────────────────────────────────────────┘  │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                    EXECUTION ENGINE (vm/)                            │   │
│  │  ┌─────────────────────────────────────────────────────────────┐    │   │
│  │  │                    Virtual Machine                           │    │   │
│  │  │                                                              │    │   │
│  │  │  ┌────────────┐  ┌────────────┐  ┌────────────────────────┐ │    │   │
│  │  │  │   Type     │  │   Type     │  │    Method              │ │    │   │
│  │  │  │   System   │  │   Loader   │  │    Descriptors         │ │    │   │
│  │  │  │            │  │            │  │                        │ │    │   │
│  │  │  │ MethodTable│  │ Assembly   │  │ MethodDesc structures  │ │    │   │
│  │  │  │ EEClass    │  │ loading &  │  │ for method metadata    │ │    │   │
│  │  │  │ TypeDesc   │  │ binding    │  │ and invocation         │ │    │   │
│  │  │  └────────────┘  └────────────┘  └────────────────────────┘ │    │   │
│  │  │                                                              │    │   │
│  │  │  ┌────────────┐  ┌────────────┐  ┌────────────────────────┐ │    │   │
│  │  │  │  Virtual   │  │ Exception  │  │    Threading           │ │    │   │
│  │  │  │   Stub     │  │  Handling  │  │                        │ │    │   │
│  │  │  │  Dispatch  │  │            │  │ Thread management,     │ │    │   │
│  │  │  │            │  │ SEH, stack │  │ synchronization,       │ │    │   │
│  │  │  │ Interface  │  │ unwinding, │  │ ThreadPool, locks      │ │    │   │
│  │  │  │ dispatch   │  │ filters    │  │                        │ │    │   │
│  │  │  └────────────┘  └────────────┘  └────────────────────────┘ │    │   │
│  │  │                                                              │    │   │
│  │  │  ┌────────────┐  ┌────────────┐  ┌────────────────────────┐ │    │   │
│  │  │  │  Interop   │  │ Profiling  │  │    Debugging           │ │    │   │
│  │  │  │            │  │    API     │  │                        │ │    │   │
│  │  │  │ P/Invoke,  │  │            │  │ DAC, SOS, diagnostic   │ │    │   │
│  │  │  │ COM interop│  │ ICorProfiler│  │ server, EventPipe     │ │    │   │
│  │  │  │ marshaling │  │ callbacks  │  │                        │ │    │   │
│  │  │  └────────────┘  └────────────┘  └────────────────────────┘ │    │   │
│  │  └─────────────────────────────────────────────────────────────┘    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│         ┌──────────────────────────┼──────────────────────────┐            │
│         ▼                          ▼                          ▼            │
│  ┌─────────────────┐  ┌─────────────────────────┐  ┌───────────────────┐   │
│  │   JIT (jit/)    │  │      GC (gc/)           │  │  Interpreter      │   │
│  │                 │  │                         │  │  (interpreter/)   │   │
│  │  RyuJIT         │  │  Generational,          │  │                   │   │
│  │  compiler       │  │  mark-sweep-compact     │  │  IL interpreter   │   │
│  │                 │  │  garbage collector      │  │  for debugging    │   │
│  │  ┌───────────┐  │  │                         │  │  and diagnostics  │   │
│  │  │ Frontend  │  │  │  ┌─────────────────┐    │  │                   │   │
│  │  │ • Import  │  │  │  │ Generations     │    │  └───────────────────┘   │
│  │  │ • Inline  │  │  │  │ • Gen0 (Eden)   │    │                          │
│  │  │ • Morph   │  │  │  │ • Gen1          │    │                          │
│  │  └───────────┘  │  │  │ • Gen2          │    │                          │
│  │  ┌───────────┐  │  │  │ • LOH (Large)   │    │                          │
│  │  │ Optimizer │  │  │  │ • POH (Pinned)  │    │                          │
│  │  │ • SSA     │  │  │  └─────────────────┘    │                          │
│  │  │ • CSE     │  │  │                         │                          │
│  │  │ • Loop    │  │  │  ┌─────────────────┐    │                          │
│  │  │ • LSRA    │  │  │  │ GC Modes        │    │                          │
│  │  └───────────┘  │  │  │ • Workstation   │    │                          │
│  │  ┌───────────┐  │  │  │ • Server        │    │                          │
│  │  │ Backend   │  │  │  │ • Concurrent    │    │                          │
│  │  │ • CodeGen │  │  │  │ • Background    │    │                          │
│  │  │ • Emit    │  │  │  └─────────────────┘    │                          │
│  │  └───────────┘  │  │                         │                          │
│  │                 │  │  Handle Tables          │                          │
│  │  Targets:       │  │  Write Barriers         │                          │
│  │  x64, x86,      │  │  Card Tables            │                          │
│  │  ARM64, ARM32,  │  │  Finalization           │                          │
│  │  LoongArch64,   │  │                         │                          │
│  │  RISC-V         │  │                         │                          │
│  └─────────────────┘  └─────────────────────────┘                          │
│                                    │                                        │
│                                    ▼                                        │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │              PLATFORM ABSTRACTION LAYER (pal/)                       │   │
│  │                                                                      │   │
│  │  Cross-platform abstractions for OS services                        │   │
│  │  • Memory management      • File I/O                                │   │
│  │  • Threading primitives   • Exception handling                      │   │
│  │  • Synchronization        • Signal handling                         │   │
│  │                                                                      │   │
│  │  Implementations: Windows, Linux, macOS, FreeBSD                    │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│                         ADDITIONAL COMPONENTS                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │   nativeaot/ │  │     md/      │  │   binder/    │  │    debug/    │   │
│  │              │  │              │  │              │  │              │   │
│  │  Ahead-of-   │  │  Metadata    │  │  Assembly    │  │  Debugger    │   │
│  │  time        │  │  reader/     │  │  binding &   │  │  support,    │   │
│  │  compilation │  │  writer      │  │  resolution  │  │  DAC, SOS    │   │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘   │
│                                                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │   ilasm/     │  │   ildasm/    │  │   hosts/     │  │  utilcode/   │   │
│  │              │  │              │  │              │  │              │   │
│  │  IL          │  │  IL          │  │  Runtime     │  │  Shared      │   │
│  │  assembler   │  │  disassembler│  │  hosts       │  │  utilities   │   │
│  └──────────────┘  └──────────────┘  └──────────────┘  └──────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Code Execution Flow

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        Code Execution Pipeline                            │
└──────────────────────────────────────────────────────────────────────────┘

     ┌─────────────┐
     │  .dll/.exe  │  Managed assembly (CIL + Metadata)
     │  (IL Code)  │
     └──────┬──────┘
            │
            ▼
     ┌─────────────┐
     │   Loader    │  Assembly loading, binding, verification
     │  (binder/)  │
     └──────┬──────┘
            │
            ▼
     ┌─────────────┐
     │    Type     │  Build runtime type representations
     │   Loader    │  (MethodTable, EEClass, TypeDesc)
     └──────┬──────┘
            │
            ▼
     ┌─────────────┐
     │  Method     │  Create MethodDesc for each method
     │  Preparation│
     └──────┬──────┘
            │
            ├─────────────────────┬─────────────────────┐
            ▼                     ▼                     ▼
     ┌─────────────┐       ┌─────────────┐       ┌─────────────┐
     │    JIT      │       │ ReadyToRun  │       │  NativeAOT  │
     │  Compile    │       │  (Precomp)  │       │   (AOT)     │
     │             │       │             │       │             │
     │  IL → x64   │       │  Use cached │       │  Full ahead │
     │  at runtime │       │  native code│       │  of time    │
     └──────┬──────┘       └──────┬──────┘       └──────┬──────┘
            │                     │                     │
            └─────────────────────┴─────────────────────┘
                                  │
                                  ▼
                           ┌─────────────┐
                           │   Execute   │  Native code execution
                           │   Native    │  with GC tracking
                           │    Code     │
                           └──────┬──────┘
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
             ┌─────────────┐             ┌─────────────┐
             │     GC      │             │  Exception  │
             │  Manages    │             │  Handling   │
             │   Memory    │             │             │
             └─────────────┘             └─────────────┘
```

## JIT Compilation Phases (RyuJIT)

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         RyuJIT Compilation Phases                         │
└──────────────────────────────────────────────────────────────────────────┘

  IL Bytecode
       │
       ▼
┌─────────────────┐
│    IMPORTER     │  Convert IL to HIR (High-level IR)
│                 │  • Build basic blocks
│                 │  • Create GenTree nodes
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│    INLINER      │  Inline method calls
│                 │  • Cost/benefit analysis
│                 │  • Recursive inlining
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│     MORPH       │  Tree transformations
│                 │  • Canonicalization
│                 │  • Assertion propagation
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│   FLOWGRAPH    │  Control flow analysis
│                 │  • Loop detection
│                 │  • Block reordering
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│      SSA        │  Static Single Assignment
│                 │  • Def-use chains
│                 │  • Phi functions
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  OPTIMIZATION   │  • Value numbering (VN)
│                 │  • Common subexpression elimination (CSE)
│                 │  • Loop optimizations (hoisting, cloning)
│                 │  • Range check elimination
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│    LOWERING     │  Convert HIR to LIR (Low-level IR)
│                 │  • Target-specific transformations
│                 │  • Address mode formation
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│      LSRA       │  Linear Scan Register Allocation
│                 │  • Build intervals
│                 │  • Allocate registers
│                 │  • Insert spills/reloads
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│    CODEGEN      │  Generate machine code
│                 │  • Instruction selection
│                 │  • Encoding
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│     EMIT        │  Emit final machine code
│                 │  • Relocations
│                 │  • GC info
│                 │  • Debug info
└────────┬────────┘
         │
         ▼
   Native Code
```

## Garbage Collector Architecture

```
┌──────────────────────────────────────────────────────────────────────────┐
│                         GC Heap Structure                                 │
└──────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                        Small Object Heap (SOH)                           │
│  ┌─────────────────┐  ┌─────────────────┐  ┌─────────────────────────┐  │
│  │   Generation 0  │  │   Generation 1  │  │      Generation 2       │  │
│  │     (Eden)      │  │                 │  │                         │  │
│  │                 │  │                 │  │                         │  │
│  │  New allocations│  │  Survived Gen0  │  │  Long-lived objects     │  │
│  │  Most frequent  │  │  collections    │  │  Least frequent GC      │  │
│  │  collection     │  │                 │  │                         │  │
│  └─────────────────┘  └─────────────────┘  └─────────────────────────┘  │
│         │                    │                        │                  │
│         │  survive           │  survive               │                  │
│         └──────────►         └────────────►           │                  │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                      Large Object Heap (LOH)                             │
│                                                                          │
│  Objects ≥ 85,000 bytes                                                 │
│  Collected with Gen2, not compacted by default                          │
└─────────────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────────────┐
│                     Pinned Object Heap (POH)                             │
│                                                                          │
│  Objects that must not move (for interop)                               │
│  Avoids fragmentation from pinning in SOH                               │
└─────────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────────┐
│                          GC Mechanisms                                    │
└──────────────────────────────────────────────────────────────────────────┘

  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
  │   Card Tables   │     │ Write Barriers  │     │  Handle Tables  │
  │                 │     │                 │     │                 │
  │  Track cross-   │     │  Intercept      │     │  Track roots    │
  │  generational   │     │  reference      │     │  from unmanaged │
  │  references     │     │  writes         │     │  code           │
  └─────────────────┘     └─────────────────┘     └─────────────────┘

  ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
  │  Finalization   │     │   Background    │     │    Regions      │
  │     Queue       │     │      GC         │     │   (optional)    │
  │                 │     │                 │     │                 │
  │  Run finalizers │     │  Concurrent     │     │  Region-based   │
  │  on separate    │     │  Gen2 collection│     │  heap for       │
  │  thread         │     │  (Server GC)    │     │  better perf    │
  └─────────────────┘     └─────────────────┘     └─────────────────┘
```

## Key Source Directories Reference

| Directory | Description |
|-----------|-------------|
| `src/coreclr/vm/` | Virtual Machine - execution engine core, type system, threading |
| `src/coreclr/jit/` | RyuJIT compiler - IL to native code compilation |
| `src/coreclr/gc/` | Garbage Collector implementation |
| `src/coreclr/pal/` | Platform Abstraction Layer for cross-platform support |
| `src/coreclr/debug/` | Debugging infrastructure (DAC, DBI, SOS) |
| `src/coreclr/binder/` | Assembly binding and loading |
| `src/coreclr/md/` | Metadata reading and writing |
| `src/coreclr/interop/` | COM and P/Invoke interoperability |
| `src/coreclr/nativeaot/` | Ahead-of-time compilation support |
| `src/coreclr/System.Private.CoreLib/` | Core managed library |
| `src/mono/` | Mono runtime (mobile, WASM) |
| `src/libraries/` | .NET class libraries |
| `src/native/corehost/` | dotnet host executable |

## Further Reading

- [Book of the Runtime (BOTR)](coreclr/botr/README.md) - Deep technical documentation
- [RyuJIT Overview](coreclr/jit/ryujit-overview.md) - JIT compiler details
- [GC Design](coreclr/botr/garbage-collection.md) - Garbage collector internals
- [Type System](coreclr/botr/type-system.md) - Type system architecture
- [Threading](coreclr/botr/threading.md) - Threading model
