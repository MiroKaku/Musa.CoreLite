#pragma once

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
