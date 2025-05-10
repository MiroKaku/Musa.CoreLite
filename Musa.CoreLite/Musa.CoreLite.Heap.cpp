#include "Musa.CoreLite.Heap.h"

namespace Musa::CoreLite::Heap
{
#ifdef _KERNEL_MODE
    constexpr ULONG MUSA_HEAP_TAG = 'asuM';

    NTSTATUS HeapSetup()
    {
        ExInitializeDriverRuntime(DrvRtPoolNxOptIn);
        return STATUS_SUCCESS;
    }

    NTSTATUS HeapTeardown()
    {
        return STATUS_SUCCESS;
    }

    PVOID HeapAllocate(_In_ SIZE_T Size)
    {
        return ExAllocatePoolZero(NonPagedPool, Size, MUSA_HEAP_TAG);
    }

    VOID HeapFree(_In_opt_ PVOID Address)
    {
        if (Address != nullptr) {
            ExFreePoolWithTag(Address, MUSA_HEAP_TAG);
        }
    }
#else
    static void* MusaCoreLiteHeap = nullptr;

    NTSTATUS MUSA_API HeapSetup()
    {
        MusaCoreLiteHeap = RtlCreateHeap(HEAP_GROWABLE | HEAP_NO_SERIALIZE, nullptr,
            0, 0, nullptr, nullptr);
        if (MusaCoreLiteHeap == nullptr) {
            return STATUS_INSUFFICIENT_RESOURCES;
        }
        return STATUS_SUCCESS;
    }

    NTSTATUS MUSA_API HeapTeardown()
    {
        if (MusaCoreLiteHeap) {
            MusaCoreLiteHeap = RtlDestroyHeap(MusaCoreLiteHeap);
        }
        return STATUS_SUCCESS;
    }

    PVOID MUSA_API HeapAllocate(_In_ SIZE_T Size)
    {
        return RtlAllocateHeap(MusaCoreLiteHeap, HEAP_ZERO_MEMORY, Size);
    }

    VOID MUSA_API HeapFree(_In_opt_ PVOID Address)
    {
        if (Address != nullptr) {
            RtlFreeHeap(MusaCoreLiteHeap, 0, Address);
        }
    }
#endif // _KERNEL_MODE

}
