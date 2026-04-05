# AGENTS.md — Musa.CoreLite

## What This Is
Windows kernel/user-mode dual-target static library for direct syscall invocation. Resolves Zw* routines by parsing ntdll (user) or ntoskrnl (kernel) export tables at runtime, builds AVL lookup tables indexed by name-hash and syscall index. Ships as static lib + precompiled stubs (.obj).

## Architecture

```
Musa.CoreLite.cpp (orchestrator)
  ├─ Heap           — kernel: ExAllocatePoolZero(NonPagedPool, tag 'asuM')
  │                   user: RtlCreateHeap
  ├─ Utilities      — GetLoadedModuleBase, RemapSectionView, Fnv1aHash,
  │                   FastEncode/DecodePointer (__security_cookie)
  │   └─ PEParser   — ImageEnumerateExports (callback: STATUS_SUCCESS=found,
  │                    STATUS_CALLBACK_BYPASS=continue)
  └─ SystemCall     — builds dual RTL_AVL_TABLE (ByIndex + ByName)
      └─ Stubs      — 3000+ lines of macro-generated Zw* wrappers
```

All implementation lives in `Musa.CoreLite/` (12 source files). Multiple .vcxproj projects reference this shared source via `$(SourcesDirectory)`.

## Public API (5 functions + 1 global)
| Symbol | Purpose | Mode |
|--------|---------|------|
| `MusaCoreLiteStartup()` | Init: heap -> module bases -> syscall table | both |
| `MusaCoreLiteShutdown()` | Teardown: syscall table -> unmap -> heap | both |
| `MusaCoreLiteGetSystemRoutine(name)` | Resolve Zw* by name string | both |
| `MusaCoreLiteGetSystemRoutineByNameHash(hash)` | Resolve Zw* by FNV-1a hash | both |
| `MusaCoreLiteGetNtdllBase()` | Get ntdll user-space base (lazy init in kernel) | both |
| `MusaCoreLiteNtdllBase` | ntdll base address | both |

## Projects in Solution (Musa.CoreLite.slnx)
| Project | Type | Notes |
|---------|------|-------|
| StaticLibrary | User-mode lib (MD) | Default variant; CustomPublish copies to Publish/ |
| StaticLibraryWithMT | User-mode lib (MT) | /MT runtime; separate lib/obj output |
| StaticLibraryForDriver | Kernel-mode lib (WDK) | MileProjectUseKernelMode=true |
| Test | GoogleTest exe | gmock 1.11.0 NuGet; depends on StaticLibrary |
| TestForDriver | WDM driver | DriverEntry/DriverUnload demo; excluded from x86 |

## Build
- **SDK**: Mile.Project.Configurations 1.0.1917 (via global.json)
- **One-liner**: `InitializeVisualStudioEnvironment.cmd && BuildAllTargets.cmd`
- **BuildAllTargets.proj**: Restore + parallel build, 6 configs (Debug/Release x x86/x64/ARM64)
- **Output**: `Output/Binaries/<Config>/<Platform>/` (test exe here)
- **Publish**: `Publish/` dir has headers, libs, stubs, config props/targets
- **NuGet dep**: Musa.Veil 1.7.0 (central management via Directory.Packages.Cpp.props)

## Linking Strategy (Musa.CoreLite.Config.targets)
Consumer links are auto-injected via BeforeTargets=Link:
- **User MD**: `Musa.CoreLite.StaticLibrary.lib` + `SystemCall.Stubs.obj`
- **User MT**: `Musa.CoreLite.StaticLibraryWithMT.lib` + `SystemCallWithMT.Stubs.obj`
- **Kernel**: `Musa.CoreLite.StaticLibraryForDriver.lib` + `SystemCallForDriver.Stubs.obj` + `Cng.lib` + `/INTEGRITYCHECK`
- **Header-only**: Set `<MusaCoreOnlyHeader>true</MusaCoreOnlyHeader>` to skip lib injection

## CI (.github/workflows/CI.yaml)
- **Trigger**: push to main, PR to main, tag `v*`
- **Build**: `BuildAllTargets.cmd` on windows-latest
- **Test**: Runs `Musa.CoreLite.Test.exe` for Debug/Release x x86/x64
- **Release** (tag only): NuGet pack+push (secrets.NUGET_TOKEN), GitHub Release with zip

## Testing
- **Framework**: GoogleTest (gmock 1.11.0 NuGet)
- **Fixture**: `MusaCoreLiteTest` — SetUpTestSuite calls Startup, TearDownTestSuite calls Shutdown
- **8 TEST_F cases**: NtdllBase non-null, known/unknown name resolution, known/unknown hash resolution, callable stub, multiple routines, name-vs-hash consistency
- **Naming**: `Feature_Condition_Expected` (e.g. `GetSystemRoutine_KnownName_ReturnsNonNull`)
- **Kernel test**: TestForDriver is a WDM driver (not GoogleTest) — loads/unloads library in DriverEntry/DriverUnload
- **Adding tests**: Add `TEST_F(MusaCoreLiteTest, ...)` to `Musa.CoreLite.Test.cpp`. Startup/Shutdown handled by fixture. Use `Fnv1aHash()` helper for hash tests.

## Conventions
- **Naming**: Public API prefix `MusaCoreLite*`; internal namespaces `Musa::CoreLite`, `Musa::Utils`; macros `MUSA_*`
- **Include guard**: `#pragma once` + `#ifndef _MUSA_CORELITE_` (dual style)
- **Formatting**: .clang-format (Microsoft base, IndentWidth=4, ColumnLimit=120, PointerAlignment=Left, SortIncludes=Never)
- **Editor**: .editorconfig (spaces, indent_size=4, utf-8-bom for C/C++, utf-16le for .rc)
- **PCH**: Each project dir has its own `universal.h` as precompiled header
- **No DLL exports**: Static-only library; no __declspec(dllexport)

## Key Conditional Compilation Macros
| Macro | Effect |
|-------|--------|
| `_KERNEL_MODE` | Switches heap, module resolution, syscall parsing, OBJ_KERNEL_HANDLE |
| `_X86_` / `_AMD64_` / `_ARM64_` | Architecture-specific syscall opcode extraction |
| `_WIN64` | KnownDlls path selection (KnownDlls32 vs KnownDlls) |
| `DBG` | Enables name string copy in syscall table entries |
| `MusaCoreOnlyHeader` | MSBuild: skips lib/obj link injection when true |

## Anti-Patterns to Avoid
- Never define `_KERNEL_MODE` in user-mode projects — use project configs
- Never call Startup/Shutdown concurrently or more than once
- Never call Shutdown at IRQL > APC_LEVEL (it's in PAGE section)
- Never assign `MmMapViewInSystemSpace` address to `MusaCoreLiteNtdllBase` — it expects the user-space address from `TransferAddress`
- Stub default-return (STATUS_NOT_SUPPORTED) silently masks missing symbols — always check return values
- pragma warning suppressions (6101, 28101, 28167, 4117) in universal.h files hide static analysis findings

## Child AGENTS.md
- `Musa.CoreLite/AGENTS.md` — Source module internals, gotchas, IRQL constraints
