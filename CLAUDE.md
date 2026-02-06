# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is the dotnet/runtime repository - the source code for the .NET runtime, libraries, and shared host installers. It contains three major components:

- **CoreCLR** (`src/coreclr/`): The primary .NET execution engine with JIT compiler, GC, and type system
- **Mono** (`src/mono/`): Lightweight runtime for mobile, WASM, and resource-constrained environments
- **Libraries** (`src/libraries/`): 246+ managed libraries providing .NET functionality (System.*, Microsoft.*)

## Build Commands

Build from the repository root. Use `build.cmd` on Windows, `./build.sh` on Linux/macOS.

```bash
# Full builds by component
./build.sh clr                    # CoreCLR runtime only
./build.sh mono+libs              # Mono runtime + libraries
./build.sh clr+libs               # CoreCLR + libraries (common for library work)
./build.sh clr+libs+host          # Full build

# Configuration flags
./build.sh libs -rc release       # Libraries with release runtime config
./build.sh clr+libs -c Release    # Everything in Release

# WASM/Browser
./build.sh mono+libs -os browser
```

After building, configure PATH: `export PATH="$(pwd)/.dotnet:$PATH"` (or `set PATH=%CD%\.dotnet;%PATH%` on Windows). Verify with `dotnet --version` (should match `sdk.version` in `global.json`).

## Testing Commands

**CoreCLR tests:**
```bash
cd src/tests && ./build.sh && ./run.sh
```

**Libraries - all tests:**
```bash
./build.sh libs.tests -test -rc release
```

**Libraries - single library (recommended approach):**
```bash
cd src/libraries/<LibraryName>
dotnet build
dotnet build /t:test ./tests/<TestProject>.csproj
```

**Run a single test method:**
```bash
dotnet build /t:Test /p:XunitMethodName={FullyQualifiedNamespace}.{ClassName}.{MethodName}
```

**Filter tests by class:**
```bash
dotnet build /t:Test /p:XUnitOptions="-class Test.ClassUnderTests"
```

**WASM tests:**
```bash
./build.sh libs.tests -test -os browser
```

**Host tests:**
```bash
./build.sh host.tests -rc release -lc release -test
```

## Code Style Requirements

Follow `.editorconfig` rules. Key conventions:

- Allman style braces (each brace on new line)
- Four spaces indentation (no tabs)
- Private/internal fields: `_camelCase`; static fields: `s_camelCase`; thread-static: `t_camelCase`
- Always specify visibility (`private`, `public`, etc.) as the first modifier
- Use `is null`/`is not null` instead of `== null`/`!= null`
- Use `nameof()` instead of string literals for member names
- Use file-scoped namespaces and pattern matching where appropriate
- Language keywords over BCL types (`int` not `Int32`)
- PascalCase for constants, methods, and local functions
- Prefer `?.` for null-conditional access (e.g., `scope?.Dispose()`)
- Use `ObjectDisposedException.ThrowIf` where applicable
- Trust C# null annotations; don't add null checks when the type system says a value cannot be null

When editing existing files, match the existing style in that file.

## Testing Guidelines

- Prefer adding tests to existing test files rather than creating new files
- If adding new code files, ensure they are listed in the `.csproj` if other files in that folder are listed
- When running tests, use filters and check test run counts to ensure they actually ran
- Do not leave tests commented out or disabled that were not previously so
- Do not emit "Act", "Arrange", or "Assert" comments in test code

## Architecture Notes

**Build Configurations:**
- **Debug**: Non-optimized, asserts enabled (best for debugging)
- **Checked**: CoreCLR only - optimized with asserts enabled
- **Release**: Optimized, asserts disabled (best for performance)

**Component Relationships:**
- CoreLib (`System.Private.CoreLib`) is tightly coupled to its runtime - must match configurations
- Libraries can be built independently in any configuration
- Use `-rc` (runtime config) and `-lc` (libraries config) to mix configurations
- If you rebuild CoreLib, also build `libs.pretest` subset before running tests

**Recommendations:**
- Working on runtime: Build everything Debug, or libs in Release if not debugging them
- Working on libraries: Build runtime Release, libraries Debug
- Working on CoreLib: Try Release runtime first, fall back to Debug if needed

## Identifying Affected Components

Check file paths to determine what to build/test:
- `src/coreclr/` or `src/tests/` → CoreCLR workflow
- `src/mono/` → Mono workflow
- `src/libraries/` → Libraries workflow
- `src/native/corehost/` or `src/installer/` → Host workflow

## Common Issues

**"Shared framework must be built first"**: Build both runtime and libs together, e.g., `./build.sh clr+libs -rc release`

**"Missing testhost"**: Same solution - build runtime + libs before testing

**Build timeouts**: Full `clr+libs` builds can take 30+ minutes. Only fail if no output for 5+ minutes.

**Warnings as errors**: Set `TreatWarningsAsErrors=false` environment variable to disable during development iteration.

## Key Documentation

- Workflow guide: `docs/workflow/README.md`
- CoreCLR building: `docs/workflow/building/coreclr/README.md`
- Libraries testing: `docs/workflow/testing/libraries/testing.md`
- Book of the Runtime (deep internals): `docs/design/coreclr/botr/`
