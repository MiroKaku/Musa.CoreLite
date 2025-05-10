#pragma once
#ifndef _MUSA_CORELITE_
#define _MUSA_CORELITE_

#if !defined(_AMD64_) && !defined(_X86_) && !defined(_ARM64_)
#error Unsupported architecture
#endif

//
// Public
//

#define MUSA_API __stdcall

//
// MusaCoreLite
//

EXTERN_C_START

#ifdef _KERNEL_MODE
extern PVOID MusaCoreLiteNtBase;
#endif
extern PVOID MusaCoreLiteNtdllBase;

NTSTATUS MUSA_API MusaCoreLiteStartup();
NTSTATUS MUSA_API MusaCoreLiteShutdown();

PVOID    MUSA_API MusaCoreLiteGetSystemRoutine(
    _In_z_ const char* Name
);

PVOID    MUSA_API MusaCoreLiteGetSystemRoutineByNameHash(
    _In_ size_t NameHash
);

EXTERN_C_END

#endif // _MUSA_CORELITE_
