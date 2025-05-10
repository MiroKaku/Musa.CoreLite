#include "Musa.CoreLite.h"
#include "Musa.CoreLite.Heap.h"
#include "Musa.CoreLite.SystemCall.h"
#include "Musa.Utilities.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, MusaCoreLiteStartup)
#pragma alloc_text(PAGE, MusaCoreLiteShutdown)
#endif

using namespace Musa;
using namespace Musa::CoreLite;

extern"C"
{
    #ifdef _KERNEL_MODE
    PVOID MusaCoreLiteNtBase = nullptr;
    #endif
    PVOID MusaCoreLiteNtdllBase       = nullptr;
    PVOID MusaCoreLiteNtdllBaseSecure = nullptr;

#ifdef _KERNEL_MODE
    NTSTATUS MUSA_API MusaCoreLiteStartup()
    {
        NTSTATUS Status;
        HANDLE   NtdllSectionHandle = nullptr;

        do {
            Status = Heap::HeapSetup();
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to setup heap, 0x%08lX", Status);
                break;
            }

            Status = Utils::GetLoadedModuleBase(&MusaCoreLiteNtBase, L"ntoskrnl.exe");
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to get ntoskrnl base, 0x%08lX", Status);
                break;
            }

            Status = Utils::GetKnownDllSectionHandle(&NtdllSectionHandle, L"ntdll.dll", SECTION_MAP_READ | SECTION_QUERY);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to get ntdll section handle: 0x%X", Status);
                break;
            }

            SECTION_IMAGE_INFORMATION SectionImageInfo{};
            Status = ZwQuerySection(NtdllSectionHandle, SectionImageInformation,
                &SectionImageInfo, sizeof(SectionImageInfo), nullptr);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to query ntdll section information: 0x%X", Status);
                break;
            }
            MusaCoreLiteNtdllBase = SectionImageInfo.TransferAddress;

            Status = SystemCallSetup();
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to setup system call, 0x%08lX", Status);
                break;
            }

            MusaLOG("MusaCoreLite initialized successfully");
        } while (false);

        if (NtdllSectionHandle) {
            (void)ZwClose(NtdllSectionHandle);
        }

        return Status;
    }
#else
    NTSTATUS MUSA_API MusaCoreLiteStartup()
    {
        NTSTATUS Status;

        do {
            Status = Heap::HeapSetup();
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to setup heap, 0x%08lX", Status);
                break;
            }

            Status = Utils::GetLoadedModuleBase(&MusaCoreLiteNtdllBase, L"ntdll.dll");
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to get ntdll base, 0x%08lX", Status);
                return STATUS_NOT_FOUND;
            }

            HANDLE NtdllSectionHandle = nullptr;
            do {
                Status = Utils::GetKnownDllSectionHandle(&NtdllSectionHandle, L"ntdll.dll",
                    SECTION_MAP_READ | SECTION_MAP_EXECUTE);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to get ntdll section handle: 0x%X", Status);
                    break;
                }

                SIZE_T ViewSize = 0;
                Status = ZwMapViewOfSection(NtdllSectionHandle, ZwCurrentProcess(), &MusaCoreLiteNtdllBaseSecure,
                    0, 0, nullptr, &ViewSize, ViewUnmap, 0, PAGE_EXECUTE_READ);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to map ntdll section: 0x%X", Status);
                    break;
                }

                Status = Utils::RemapSectionView(&NtdllSectionHandle, &MusaCoreLiteNtdllBaseSecure, &ViewSize);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to remap ntdll section: 0x%X", Status);
                    break;
                }

            } while (false);
            if (NtdllSectionHandle) {
                (void)ZwClose(NtdllSectionHandle);
            }

            Status = SystemCallSetup();
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to setup system call, 0x%08lX", Status);
                return Status;
            }

            MusaLOG("MusaCoreLite initialized successfully");
        } while (false);

        return Status;
    }
#endif

    NTSTATUS MUSA_API MusaCoreLiteShutdown()
    {
        PAGED_CODE();

        NTSTATUS Status = SystemCallTeardown();
        if (!NT_SUCCESS(Status)) {
            MusaLOG("Failed to teardown system call, 0x%08lX", Status);
            return Status;
        }

        if (MusaCoreLiteNtdllBaseSecure) {
            Status = ZwUnmapViewOfSection(ZwCurrentProcess(), MusaCoreLiteNtdllBaseSecure);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to unmap ntdll section: 0x%08lX", Status);
                return Status;
            }
            MusaCoreLiteNtdllBaseSecure = nullptr;
        }

        Status = Heap::HeapTeardown();
        if (!NT_SUCCESS(Status)) {
            MusaLOG("Failed to teardown heap, 0x%08lX", Status);
            return Status;
        }

        MusaLOG("MusaCoreLite shutdown successfully");
        return Status;
    }

    PVOID MUSA_API MusaCoreLiteGetSystemRoutine(
        _In_z_ const char* Name
    )
    {
        return GetSystemRoutineAddress(Name);
    }

    PVOID MUSA_API MusaCoreLiteGetSystemRoutineByNameHash(
        _In_ size_t NameHash
    )
    {
        return GetSystemRoutineAddressByNameHash(nullptr, NameHash);
    }
}
