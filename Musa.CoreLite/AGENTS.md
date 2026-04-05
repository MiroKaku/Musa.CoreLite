# AGENTS.md — Musa.CoreLite Source Modules

Parent: [../AGENTS.md](../AGENTS.md) — project overview, build, linking, conventions

## Module Map (12 files)

| File | Lines | Role |
|------|-------|------|
| `Musa.CoreLite.h` | ~50 | Public header: API prototypes, `MUSA_API` macro, architecture gate |
| `Musa.CoreLite.cpp` | ~240 | Orchestrator: Startup/Shutdown sequences, GetNtdllBase, global variable definitions |
| `Musa.CoreLite.SystemCall.h` | 28 | Internal syscall API with IRQL annotations |
| `Musa.CoreLite.SystemCall.cpp` | 771 | Core: syscall table build, AVL lookup, opcode extraction |
| `Musa.CoreLite.SystemCall.Stubs.cpp` | 3043 | Macro-generated Zw* wrappers (`_MUSA_DEFINE_STUB` / `_VEIL_DEFINE_IAT_SYMBOL`) |
| `Musa.CoreLite.Heap.h` | 16 | Internal heap API |
| `Musa.CoreLite.Heap.cpp` | 64 | Dual heap impl: kernel ExAllocatePoolZero / user RtlCreateHeap |
| `Musa.Utilities.h` | ~90 | Fnv1aHash, FastEncode/DecodePointer, RemapSectionView, MapNtdllImage/UnmapNtdllImage (kernel) |
| `Musa.Utilities.cpp` | ~310 | GetLoadedModuleBase, GetKnownDllSectionHandle, RemapSectionView, MapNtdllImage (KnownDlls → disk fallback) |
| `Musa.Utilities.PEParser.h` | 23 | PE export enumeration API |
| `Musa.Utilities.PEParser.cpp` | 82 | ImageEnumerateExports, ImageRvaToSection |
| `Musa.CoreLite.Nothing.cpp` | 0 | Empty file (linker placeholder) |

## Init/Teardown Sequence

```
Startup:  HeapSetup → GetLoadedModuleBase(ntoskrnl/ntdll) → RemapSectionView → SystemCallSetup
          Kernel: ntdll TransferAddress acquisition is non-fatal (boot-start may not have KnownDlls)
Shutdown: SystemCallTeardown → ZwUnmapViewOfSection(secure copy, user-mode only) → HeapTeardown
```

- Startup is `#pragma alloc_text(PAGE, ...)` — safe to call from BootDriverReinitialize callback (INIT would be freed after DriverEntry returns)
- Shutdown is `#pragma alloc_text(PAGE, ...)` — requires IRQL <= APC_LEVEL

## IRQL Constraints

| Function | Max IRQL | Section |
|----------|----------|---------|
| `MusaCoreLiteStartup` | APC_LEVEL | PAGE |
| `MusaCoreLiteShutdown` | APC_LEVEL | PAGE |
| `SystemCallSetup` | APC_LEVEL | .text |
| `SystemCallTeardown` | APC_LEVEL | .text |
| `GetSystemRoutineAddress` | DISPATCH_LEVEL | .text |
| `GetSystemRoutineAddressByNameHash` | DISPATCH_LEVEL | .text |

## Syscall Table Internals

- **Two AVL tables**: `SyscallTableByIndex` (key: syscall index) and `SyscallTableByName` (key: FNV-1a hash of name)
- **Ready flag**: `volatile LONG SyscallTableReady` — set via `InterlockedExchange`, read via `ReadAcquire`
- **Entry struct** (`MUSA_SYSCALL_LIST_ENTRY`): index, name hash, encoded routine pointer; `DBG` builds also store name string
- **Pointer obfuscation**: Routine pointers stored via `FastEncodePointer` (XOR with rotated `__security_cookie`)
- **Lookup fallback**: If AVL lookup misses, falls back to `RtlFindExportedRoutineByName` on ntdll/ntoskrnl base

## Opcode Extraction (architecture-specific)

Syscall index extracted from machine code at export addresses:
- **x86/x64**: `mov eax, imm32` at function entry — extract 4-byte immediate
- **ARM64**: `SVC #imm16` — extract 16-bit immediate from instruction encoding
- Special case: `ZwQuerySystemTime` has a known gap in syscall index sequence

## User-Mode Security (RemapSectionView)

ntdll remapped via `ZwOpenSection` → temp buffer copy → `ZwCreateSection` + `ZwMapViewOfSection` for tamper resistance. `MusaCoreLiteNtdllBaseSecure` published only after success; falls back to `MusaCoreLiteNtdllBase`.

## Kernel-Mode Module Resolution

- `ExEnterCriticalRegionAndAcquireResourceShared(PsLoadedModuleResource)` to walk loaded module list
- **SystemCallSetup**: uses `Utils::MapNtdllImage` to get ntdll mapped into system space for export enumeration. `MapNtdllImage` tries KnownDlls first; if unavailable (boot-start), falls back to `ZwCreateFile(\SystemRoot\System32\ntdll.dll)` + `ZwCreateSection(SEC_IMAGE)` + `MmMapViewInSystemSpace`
- **MusaCoreLiteNtdllBase** (user-space address): obtained from `ZwQuerySection(SectionImageInformation)` → `TransferAddress` during Startup. In boot-start scenarios where KnownDlls is absent, this remains null and can be lazy-initialized later via `MusaCoreLiteGetNtdllBase()`
- **MusaCoreLiteNtBase** (ntoskrnl base): internal-only global (not in public header), accessed via `EXTERN_C` forward declaration in SystemCall.cpp

## Stub Generation Pattern

`_MUSA_DEFINE_STUB(ReturnT, DefaultT, Name, Params, Args)` — resolves at first call, returns `DefaultT` if unresolved. x86 stubs use decorated names (`_Name@stacksize`) via `_VEIL_DEFINE_IAT_SYMBOL`. Compiled to separate .obj per project variant.

## Gotchas

- **Boot-start drivers**: `\KnownDlls` does not exist until smss.exe populates it. `MapNtdllImage` falls back to disk (`\SystemRoot\System32\ntdll.dll`). `MusaCoreLiteNtdllBase` may be null after Startup — use `MusaCoreLiteGetNtdllBase()` for lazy init
- `ReadAcquire(&SyscallTableReady)` definition comes from Musa.Veil headers, not this repo — verify acquire semantics match `InterlockedExchange` pairing
- Concurrent lookup during teardown is unsafe — no mutex/refcount protects AVL table deletion
- Stub default-return silently masks unresolved symbols; non-NTSTATUS stubs may return 0/NULL which looks like success
- `__security_cookie` must be provided by the linker (MSVC CRT normally supplies it)
- `DBG` name strings are allocated from heap during setup and freed during teardown — lifetime tied to table
- `ImageEnumerateExports` callback uses STATUS_SUCCESS to mean "found, stop" and STATUS_CALLBACK_BYPASS to mean "continue" — inverted from typical NTSTATUS semantics
