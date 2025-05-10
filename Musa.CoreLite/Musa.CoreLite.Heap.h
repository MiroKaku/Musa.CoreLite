#pragma once

namespace Musa::CoreLite::Heap
{
    NTSTATUS MUSA_API HeapSetup();

    NTSTATUS MUSA_API HeapTeardown();

    PVOID MUSA_API HeapAllocate(
        _In_ SIZE_T Size
    );

    VOID  MUSA_API HeapFree(
        _In_opt_ PVOID Address
    );
}
