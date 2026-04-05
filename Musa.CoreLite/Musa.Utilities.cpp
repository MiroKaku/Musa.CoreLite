#include "Musa.Utilities.h"
#include "Musa.CoreLite.Heap.h"

namespace Musa::Utils
{
#ifdef _KERNEL_MODE
    NTSTATUS MUSA_API GetLoadedModuleBase(
        _Out_ PVOID* ModuleBase,
        _In_  PCWSTR ModuleName
    )
    {
        *ModuleBase = nullptr;

        UNICODE_STRING Name{};
        NTSTATUS Status = RtlInitUnicodeStringEx(&Name, ModuleName);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        /* Lock the list */
        ExEnterCriticalRegionAndAcquireResourceShared(PsLoadedModuleResource);
        {
            /* Loop the loaded module list */
            for (LIST_ENTRY const* NextEntry = PsLoadedModuleList->Flink; NextEntry != PsLoadedModuleList;) {

                /* Get the entry */
                const auto LdrEntry = CONTAINING_RECORD(NextEntry, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

                /* Check if it's the module */
                if (RtlEqualUnicodeString(&Name, &LdrEntry->BaseDllName, TRUE)) {
                    /* Found it */
                    *ModuleBase = LdrEntry->DllBase;
                    break;
                }

                /* Keep looping */
                NextEntry = NextEntry->Flink;
            }
        }
        /* Release the lock */
        ExReleaseResourceAndLeaveCriticalRegion(PsLoadedModuleResource);

        return Status;
    }
#else
    NTSTATUS MUSA_API GetLoadedModuleBase(
        _Out_ PVOID* ModuleBase,
        _In_  PCWSTR ModuleName
    )
    {
        UNICODE_STRING Name{};
        NTSTATUS Status = RtlInitUnicodeStringEx(&Name, ModuleName);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        Status = LdrGetDllHandleEx(LDR_GET_DLL_HANDLE_EX_UNCHANGED_REFCOUNT,
            nullptr, nullptr, &Name, ModuleBase);
        if (!NT_SUCCESS(Status)) {
            return Status;
        }

        return Status;
    }

#endif // _KERNEL_MODE

    NTSTATUS MUSA_API GetKnownDllSectionHandle(
        _Out_ HANDLE* SectionHandle,
        _In_  PCWSTR  DllName,
        _In_  ACCESS_MASK DesiredAccess
    )
    {
        NTSTATUS Status;
        HANDLE   DirectoryHandle = nullptr;

        do {
            UNICODE_STRING KnownDllName;
        #ifndef _WIN64
            Status = RtlInitUnicodeStringEx(&KnownDllName, L"\\KnownDlls32");
        #else
            Status = RtlInitUnicodeStringEx(&KnownDllName, L"\\KnownDlls");
        #endif
            if (!NT_SUCCESS(Status)) {
                break;
            }

            OBJECT_ATTRIBUTES ObjectAttributes;
            InitializeObjectAttributes(&ObjectAttributes, &KnownDllName, OBJ_CASE_INSENSITIVE,
                nullptr, nullptr)

            #ifdef _KERNEL_MODE
                ObjectAttributes.Attributes |= OBJ_KERNEL_HANDLE;
            #endif

            Status = ZwOpenDirectoryObject(&DirectoryHandle, DIRECTORY_QUERY, &ObjectAttributes);
            if (!NT_SUCCESS(Status)) {
            #ifndef _WIN64
                if (Status != STATUS_OBJECT_NAME_NOT_FOUND) {
                    break;
                }

                Status = RtlInitUnicodeStringEx(&KnownDllName, L"\\KnownDlls");
                if (!NT_SUCCESS(Status)) {
                    break;
                }

                Status = ZwOpenDirectoryObject(&DirectoryHandle, DIRECTORY_QUERY, &ObjectAttributes);
                if (!NT_SUCCESS(Status)) {
                    break;
                }
            #else
                break;
            #endif
            }

            UNICODE_STRING SectionName{};
            Status = RtlInitUnicodeStringEx(&SectionName, DllName);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            InitializeObjectAttributes(&ObjectAttributes, &SectionName, OBJ_CASE_INSENSITIVE,
                DirectoryHandle, nullptr)

            #ifdef _KERNEL_MODE
                ObjectAttributes.Attributes |= OBJ_KERNEL_HANDLE;
            #endif

            Status = ZwOpenSection(SectionHandle, DesiredAccess, &ObjectAttributes);
            if (!NT_SUCCESS(Status)) {
                break;
            }

        } while (false);

        if (DirectoryHandle) {
            (void)ZwClose(DirectoryHandle);
        }

        return Status;
    }

#ifdef _KERNEL_MODE
    NTSTATUS MUSA_API MapNtdllImage(
        _Out_ PVOID* ImageBase
    )
    {
        *ImageBase = nullptr;

        NTSTATUS Status;
        HANDLE   SectionHandle = nullptr;
        PVOID    SectionObject = nullptr;
        PVOID    MappedBase    = nullptr;
        HANDLE   FileHandle    = nullptr;

        do {
            // Try \KnownDlls first (available after smss.exe initializes)
            Status = GetKnownDllSectionHandle(&SectionHandle, L"ntdll.dll", SECTION_MAP_READ | SECTION_QUERY);
            if (!NT_SUCCESS(Status)) {
                if (Status != STATUS_OBJECT_NAME_NOT_FOUND) {
                    break;
                }

                // Fallback: open ntdll.dll from disk (boot-start scenario)
                UNICODE_STRING NtdllPath{};
                Status = RtlInitUnicodeStringEx(&NtdllPath, L"\\SystemRoot\\System32\\ntdll.dll");
                if (!NT_SUCCESS(Status)) {
                    break;
                }

                OBJECT_ATTRIBUTES ObjectAttributes;
                InitializeObjectAttributes(&ObjectAttributes, &NtdllPath,
                    OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, nullptr, nullptr)

                IO_STATUS_BLOCK IoStatusBlock{};
                Status = ZwCreateFile(&FileHandle, FILE_READ_DATA | SYNCHRONIZE, &ObjectAttributes,
                    &IoStatusBlock, nullptr, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ,
                    FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT, nullptr, 0);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to open ntdll.dll from disk: 0x%X", Status);
                    break;
                }

                Status = ZwCreateSection(&SectionHandle, SECTION_MAP_READ | SECTION_QUERY,
                    nullptr, nullptr, PAGE_READONLY, SEC_IMAGE, FileHandle);
                if (!NT_SUCCESS(Status)) {
                    MusaLOG("Failed to create ntdll section from file: 0x%X", Status);
                    break;
                }
            }

            Status = ObReferenceObjectByHandle(SectionHandle, SECTION_MAP_READ | SECTION_QUERY,
                *MmSectionObjectType, KernelMode, &SectionObject, nullptr);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            SIZE_T ViewSize = 0;
            Status = MmMapViewInSystemSpace(SectionObject, &MappedBase, &ViewSize);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            *ImageBase = MappedBase;
            MappedBase = nullptr;
        } while (false);

        if (MappedBase) {
            (void)MmUnmapViewInSystemSpace(MappedBase);
        }
        if (SectionObject) {
            ObDereferenceObject(SectionObject);
        }
        if (SectionHandle) {
            (void)ZwClose(SectionHandle);
        }
        if (FileHandle) {
            (void)ZwClose(FileHandle);
        }

        return Status;
    }

    VOID MUSA_API UnmapNtdllImage(
        _In_ PVOID ImageBase
    )
    {
        (void)MmUnmapViewInSystemSpace(ImageBase);
    }
#endif

    NTSTATUS MUSA_API RemapSectionView(
        _Inout_ HANDLE* SectionHandle,
        _In_    PVOID*  ViewBase,
        _In_    SIZE_T* ViewSize
    )
    {
        NTSTATUS Status;
        PVOID    TmpBuffer        = nullptr;
        HANDLE   TmpSectionHandle = nullptr;

        do {
            SYSTEM_BASIC_INFORMATION BasicInfo{};
            Status = ZwQuerySystemInformation(SystemBasicInformation, &BasicInfo,
                sizeof(SYSTEM_BASIC_INFORMATION), nullptr);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            LARGE_INTEGER ViewSizeInt{ .QuadPart = (LONGLONG)ALIGN_UP_BY(*ViewSize, BasicInfo.AllocationGranularity) };
            Status = ZwCreateSection(&TmpSectionHandle, SECTION_ALL_ACCESS, nullptr, &ViewSizeInt,
                PAGE_EXECUTE_READWRITE, SEC_COMMIT, nullptr);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            TmpBuffer = CoreLite::Heap::HeapAllocate((SIZE_T)ViewSizeInt.QuadPart);
            if (TmpBuffer == nullptr) {
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

            RtlCopyMemory(TmpBuffer, *ViewBase, *ViewSize);

            Status = ZwUnmapViewOfSection(ZwCurrentProcess(), *ViewBase);
            if (!NT_SUCCESS(Status)) {
                break;
            }

            Status = ZwMapViewOfSection(TmpSectionHandle, ZwCurrentProcess(), ViewBase,
                0, 0, nullptr, ViewSize,
                ViewUnmap, 0, PAGE_EXECUTE_READWRITE);
            if (!NT_SUCCESS(Status)) {
                *ViewBase = nullptr;
                *ViewSize = 0;
                break;
            }

            RtlCopyMemory(*ViewBase, TmpBuffer, *ViewSize);

            Status = ZwUnmapViewOfSection(ZwCurrentProcess(), *ViewBase);
            if (!NT_SUCCESS(Status)) {
                MUSA_SWAP(TmpSectionHandle, *SectionHandle);
                break;
            }

            Status = ZwMapViewOfSection(TmpSectionHandle, ZwCurrentProcess(), ViewBase,
                0, 0, nullptr, ViewSize,
                ViewUnmap, SEC_NO_CHANGE, PAGE_EXECUTE_READ);
            if (!NT_SUCCESS(Status)) {
                *ViewBase = nullptr;
                *ViewSize = 0;
                break;
            }

            MUSA_SWAP(TmpSectionHandle, *SectionHandle);
        } while (false);

        if (TmpSectionHandle) {
            (void)ZwClose(TmpSectionHandle);
        }

        if (TmpBuffer) {
            CoreLite::Heap::HeapFree(TmpBuffer);
        }

        return Status;
    }

}
