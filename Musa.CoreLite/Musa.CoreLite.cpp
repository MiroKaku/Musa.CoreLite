#include "Musa.CoreLite.h"
#include "Musa.CoreLite.Heap.h"
#include "Musa.CoreLite.SystemCall.h"
#include "Musa.Utilities.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE, MusaCoreLiteStartup)
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
        PAGED_CODE();

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

            // Attempt to get ntdll user-space TransferAddress via KnownDlls.
            // This may fail during boot-start (KnownDlls not yet populated);
            // MusaCoreLiteNtdllBase will remain null, which is acceptable.
            {
                NTSTATUS NtdllStatus = Utils::GetKnownDllSectionHandle(
                    &NtdllSectionHandle, L"ntdll.dll", SECTION_MAP_READ | SECTION_QUERY);
                if (NT_SUCCESS(NtdllStatus)) {
                    SECTION_IMAGE_INFORMATION SectionImageInfo{};
                    NtdllStatus = ZwQuerySection(NtdllSectionHandle, SectionImageInformation,
                        &SectionImageInfo, sizeof(SectionImageInfo), nullptr);
                    if (NT_SUCCESS(NtdllStatus)) {
                        MusaCoreLiteNtdllBase = SectionImageInfo.TransferAddress;
                    }
                    else {
                        MusaLOG("Failed to query ntdll section information: 0x%X", NtdllStatus);
                    }
                }
            }

            Status = SystemCallSetup();
            if (!NT_SUCCESS(Status)) {
                if (Status != STATUS_RETRY) {
                    MusaLOG("Failed to setup system call, 0x%08lX", Status);
                }
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
                break;
            }

            // Map and remap ntdll section using a local pointer to avoid TOCTOU:
            // MusaCoreLiteNtdllBaseSecure is only assigned after remap succeeds,
            // so concurrent readers never see a stale or partially-remapped address.
            HANDLE NtdllSectionHandle = nullptr;
            PVOID  NtdllSecureView    = nullptr;
            do {
                Status = Utils::GetKnownDllSectionHandle(&NtdllSectionHandle, L"ntdll.dll",
                    SECTION_MAP_READ | SECTION_MAP_EXECUTE);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to get ntdll section handle: 0x%X", Status);
                    break;
                }

                SIZE_T ViewSize = 0;
                Status = ZwMapViewOfSection(NtdllSectionHandle, ZwCurrentProcess(), &NtdllSecureView,
                    0, 0, nullptr, &ViewSize, ViewUnmap, 0, PAGE_EXECUTE_READ);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to map ntdll section: 0x%X", Status);
                    break;
                }

                Status = Utils::RemapSectionView(&NtdllSectionHandle, &NtdllSecureView, &ViewSize);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to remap ntdll section: 0x%X", Status);
                    break;
                }

                // Publish the fully-remapped view atomically.
                MusaCoreLiteNtdllBaseSecure = NtdllSecureView;
                NtdllSecureView = nullptr;

            } while (false);
            if (NtdllSecureView) {
                (void)ZwUnmapViewOfSection(ZwCurrentProcess(), NtdllSecureView);
            }
            if (NtdllSectionHandle) {
                (void)ZwClose(NtdllSectionHandle);
            }

            Status = SystemCallSetup();
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to setup system call, 0x%08lX", Status);
                break;
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

    #ifndef _KERNEL_MODE
        if (MusaCoreLiteNtdllBaseSecure) {
            Status = ZwUnmapViewOfSection(ZwCurrentProcess(), MusaCoreLiteNtdllBaseSecure);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to unmap ntdll section: 0x%08lX", Status);
                return Status;
            }
            MusaCoreLiteNtdllBaseSecure = nullptr;
        }
    #endif

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

    PVOID MUSA_API MusaCoreLiteGetNtdllBase()
    {
    #ifdef _KERNEL_MODE
        if (MusaCoreLiteNtdllBase == nullptr) {
            HANDLE SectionHandle = nullptr;

            NTSTATUS Status = Utils::GetKnownDllSectionHandle(
                &SectionHandle, L"ntdll.dll", SECTION_MAP_READ | SECTION_QUERY);
            if (NT_SUCCESS(Status)) {
                SECTION_IMAGE_INFORMATION SectionImageInfo{};
                Status = ZwQuerySection(SectionHandle, SectionImageInformation,
                    &SectionImageInfo, sizeof(SectionImageInfo), nullptr);
                if (NT_SUCCESS(Status)) {
                    MusaCoreLiteNtdllBase = SectionImageInfo.TransferAddress;
                }
            }

            if (SectionHandle) {
                (void)ZwClose(SectionHandle);
            }
        }
    #endif
        return MusaCoreLiteNtdllBase;
    }
}
