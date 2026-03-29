#pragma once

// Internal API Contract:
//   - Callers are responsible for ensuring parameter validity.
//   - Name pointers must remain valid for the duration of the call.
//   - NameHash must match Fnv1aHash(Name, strlen(Name)) when both are provided.

namespace Musa::CoreLite
{
    _Must_inspect_result_
    _IRQL_requires_max_(APC_LEVEL)
    NTSTATUS MUSA_API SystemCallSetup();

    _Must_inspect_result_
    _IRQL_requires_max_(APC_LEVEL)
    NTSTATUS MUSA_API SystemCallTeardown();

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PVOID MUSA_API GetSystemRoutineAddress(
        _In_ const char* Name
    );

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PVOID MUSA_API GetSystemRoutineAddressByNameHash(
        _In_opt_ const char* Name,
        _In_ size_t NameHash
    );
}
