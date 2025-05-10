#include "Musa.CoreLite.SystemCall.h"
#include "Musa.CoreLite.Heap.h"
#include "Musa.Utilities.h"
#include "Musa.Utilities.PEParser.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////
//                                              X86
//  ntdll.dll
// 
// .text:77F04A10                                               public _ZwCreateFile@44
// .text:77F04A10                               _ZwCreateFile@44 proc near
// .text:77F04A10 B8 42 00 00 00                                mov     eax, 42h ; Index
//                   ^^ ^^ ^^ ^^
// .text:77F04A15 BA 00 03 FE 7F                                mov     edx, 7FFE0300h
// .text:77F04A1A FF 12                                         call    dword ptr [edx]
// .text:77F04A1C C2 2C 00                                      retn    2Ch
// .text:77F04A1C                               _ZwCreateFile@44 endp
//
//  ntoskrnl.exe
// 
// .text:00433228                                               public _ZwCreateFile@44
// .text:00433228                               _ZwCreateFile@44 proc near
// .text:00433228 B8 42 00 00 00                                mov     eax, 42h ; Index
//                   ^^ ^^ ^^ ^^
// .text:0043322D 8D 54 24 04                                   lea     edx, [esp+4]
// .text:00433231 9C                                            pushf
// .text:00433232 6A 08                                         push    8
// .text:00433234 E8 65 23 00 00                                call    _KiSystemService
// .text:00433239 C2 2C 00                                      retn    2Ch
// .text:00433239                               _ZwCreateFile@44 endp
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
//                                              X64
//  ntdll.dll
// 
// .text:000000018009F890                                       public ZwCreateFile
// .text:000000018009F890                       ZwCreateFile    proc near
// .text:000000018009F890
// .text:000000018009F890 4C 8B D1                              mov     r10, rcx
// .text:000000018009F893 B8 55 00 00 00                        mov     eax, 55h        ; Index
//                           ^^ ^^ ^^ ^^
// .text:000000018009F898 F6 04 25 08 03 FE 7F 01               test    byte ptr ds:7FFE0308h, 1
// .text:000000018009F8A0 75 03                                 jnz     short loc_18009F8A5
// .text:000000018009F8A2 0F 05                                 syscall                 ; Low latency system call
// .text:000000018009F8A4 C3                                    retn
// .text:000000018009F8A5                       loc_18009F8A5:
// .text:000000018009F8A5 CD 2E                                 int     2Eh             ; DOS 2+ internal - EXECUTE COMMAND
// .text:000000018009F8A5                                                               ; DS:SI -> counted CR-terminated command string
// .text:000000018009F8A7 C3                                    retn
// .text:000000018009F8A7                       ZwCreateFile    endp
//
//  ntoskrnl.exe
// 
// .text:00000001404254B0                                       public ZwCreateFile
// .text:00000001404254B0                       ZwCreateFile    proc near
// .text:00000001404254B0
// .text:00000001404254B0 48 8B C4                              mov     rax, rsp
// .text:00000001404254B3 FA                                    cli
// .text:00000001404254B4 48 83 EC 10                           sub     rsp, 10h
// .text:00000001404254B8 50                                    push    rax
// .text:00000001404254B9 9C                                    pushfq
// .text:00000001404254BA 6A 10                                 push    10h
// .text:00000001404254BC 48 8D 05 CD 8A 00 00                  lea     rax, KiServiceLinkage
// .text:00000001404254C3 50                                    push    rax
// .text:00000001404254C4 B8 55 00 00 00                        mov     eax, 55h        ; Index
//                           ^^ ^^ ^^ ^^
// .text:00000001404254C9 E9 B2 7A 01 00                        jmp     KiServiceInternal
// .text:00000001404254CE C3                                    retn
// .text:00000001404254CE                       ZwCreateFile    endp
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
//                                              ARM64
//  ntdll.dll
//
//
// .text:000000018000F8C0                                       EXPORT ZwCreateFile
// .text:000000018000F8C0                       ZwCreateFile
// .text:000000018000F8C0 A1 0A 00 D4                           SVC             0x55 ; Index
// .text:000000018000F8C4 C0 03 5F D6                           RET
// .text:000000018000F8C4                       ; End of function ZwCreateFile
//
//  ntoskrnl.exe
// 
// .text:0000000140200830                                       EXPORT ZwCreateFile
// .text:0000000140200830                       ZwCreateFile
// .text:0000000140200830 B0 0A 80 D2                           MOV             X16, #0x55 ; Index
// .text:0000000140200834 B3 2D 00 14                           B               KiServiceInternal
// .text:0000000140200834                       ; End of function ZwCreateFile

EXTERN_C PVOID MusaCoreLiteNtdllBaseSecure;

namespace Musa::CoreLite
{
    RTL_AVL_TABLE SyscallTableByName{};
    RTL_AVL_TABLE SyscallTableByIndex{};

    VEIL_DECLARE_STRUCT(MUSA_SYSCALL_LIST_ENTRY)
    {
        uint32_t    Index;
        size_t      NameHash;
        const void* Address;
     #if defined(DBG)
        const char* Name;
     #endif
    };

    namespace SyscallTableRoutines
    {
        RTL_AVL_COMPARE_ROUTINE  CompareByIndex;
        RTL_AVL_COMPARE_ROUTINE  CompareByNameHash;
        RTL_AVL_ALLOCATE_ROUTINE Allocate;
        RTL_AVL_FREE_ROUTINE     Free;
    }

#ifdef _KERNEL_MODE
    _Must_inspect_result_
    _IRQL_requires_max_(APC_LEVEL)
    NTSTATUS MUSA_API SystemCallSetup()
    {
        NTSTATUS Status;
        HANDLE   SectionHandle    = nullptr;
        PVOID    SectionObject    = nullptr;
        PVOID    ImageBaseOfNtdll = nullptr;

        RtlInitializeGenericTableAvl(&SyscallTableByName,
            &SyscallTableRoutines::CompareByNameHash,
            &SyscallTableRoutines::Allocate,
            &SyscallTableRoutines::Free,
            nullptr);

        RtlInitializeGenericTableAvl(&SyscallTableByIndex,
            &SyscallTableRoutines::CompareByIndex,
            &SyscallTableRoutines::Allocate,
            &SyscallTableRoutines::Free,
            nullptr);

        do {
            // get ntdll.dll base
            Status = Utils::GetKnownDllSectionHandle(&SectionHandle, L"ntdll.dll", SECTION_MAP_READ | SECTION_QUERY);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to get ntdll section handle: 0x%X", Status);
                break;
            }

            Status = ObReferenceObjectByHandle(SectionHandle, SECTION_MAP_READ | SECTION_QUERY,
                *MmSectionObjectType, KernelMode, &SectionObject, nullptr);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to reference ntdll section object: 0x%X", Status);
                break;
            }

            SIZE_T ViewSize = 0;
            Status = MmMapViewInSystemSpace(SectionObject, &ImageBaseOfNtdll, &ViewSize);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to map ntdll section: 0x%X", Status);
                break;
            }

            // dump ntdll exports
            Status = Utils::PEParser::ImageEnumerateExports([](const uint32_t Ordinal, const char* Name, const void* Address, void* Context)->NTSTATUS
            {
                UNREFERENCED_PARAMETER(Ordinal);
                UNREFERENCED_PARAMETER(Context);

                NTSTATUS Status = STATUS_SUCCESS;

                if (Name) {
                    if (*reinterpret_cast<const unsigned short*>(Name) == 'wZ') {
                        const auto OpCodeBase = static_cast<const uint8_t*>(Address);
                        const auto NameLength = strlen(Name);

                        MUSA_SYSCALL_LIST_ENTRY Entry;
                        Entry.NameHash = Utils::Fnv1aHash(Name, NameLength);
                        Entry.Address  = Utils::FastEncodePointer(static_cast<void*>(nullptr));

                        #if defined(_X86_)
                        // B8 42 00 00 00   mov eax, 42h
                        Entry.Index = (*reinterpret_cast<const uint32_t*>(OpCodeBase + 1)) & 0xFFFF;
                        #elif defined(_AMD64_)
                        // 4C 8B D1         mov r10, rcx
                        // B8 55 00 00 00   mov eax, 55h
                        Entry.Index = (*reinterpret_cast<const uint32_t*>(OpCodeBase + 4)) & 0xFFFF;
                        #elif defined(_ARM64_)
                        // A1 0A 00 D4      SVC 0x55 ; imm16(5~20 bit)
                        Entry.Index = (*reinterpret_cast<const uint32_t*>(OpCodeBase) >> 5) & 0xFFFF;
                        #endif

                        #if defined(DBG)
                        Entry.Name = static_cast<char*>(Heap::HeapAllocate(NameLength + sizeof("")));
                        if (Entry.Name == nullptr) {
                            Status = STATUS_INSUFFICIENT_RESOURCES;
                            return Status;
                        }
                        strcpy_s(const_cast<char*>(Entry.Name), NameLength + _countof(""), Name);
                        #endif

                        if (!RtlInsertElementGenericTableAvl(&SyscallTableByIndex, &Entry,
                                                             sizeof(MUSA_SYSCALL_LIST_ENTRY), nullptr)) {
                            #if defined(DBG)
                            Heap::HeapFree(const_cast<char*>(Entry.Name));
                            #endif

                            Status = STATUS_INSUFFICIENT_RESOURCES;
                            return Status;
                        }
                    }
                }
                return Status;
            }, nullptr, ImageBaseOfNtdll, true);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to enumerate ntdll exports: 0x%X", Status);
                break;
            }

            // Fix 'ZwQuerySystemTime' Index
            uint32_t IndexOfPrevious          = 0;
            uint32_t IndexOfZwQuerySystemTime = static_cast<uint32_t>(~0);

            for (auto Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, TRUE));
                 Entry;
                 Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(
                     RtlEnumerateGenericTableAvl(&SyscallTableByIndex, FALSE))) {
                if (IndexOfZwQuerySystemTime == static_cast<uint32_t>(~0)) {
                    // Fix 'ZwQuerySystemTime' Index - Step 1
                    if (Entry->Index - IndexOfPrevious == 2) {
                        IndexOfZwQuerySystemTime = IndexOfPrevious + 1;
                    }
                    else {
                        IndexOfPrevious = Entry->Index;
                    }
                }
                else {
                    // Fix 'ZwQuerySystemTime' Index - Step 2
                    if (Entry->NameHash == Utils::Fnv1aHash("ZwQuerySystemTime")) {
                        IndexOfPrevious = Entry->Index; // Re-insert later
                    }
                }
            }

            // Fix 'ZwQuerySystemTime' Index - Step 3: Re-insert 'ZwQuerySystemTime'
            if (IndexOfZwQuerySystemTime != static_cast<uint32_t>(~0)) {
                MUSA_SYSCALL_LIST_ENTRY Entry{};
                Entry.Index = IndexOfPrevious;

                if (const auto MatchEntry = RtlLookupElementGenericTableAvl(&SyscallTableByIndex, &Entry)) {
                    Entry       = *static_cast<PCMUSA_SYSCALL_LIST_ENTRY>(MatchEntry);
                    Entry.Index = IndexOfZwQuerySystemTime;

                    #if defined(DBG)
                    static_cast<PMUSA_SYSCALL_LIST_ENTRY>(MatchEntry)->Name = nullptr;
                    RtlDeleteElementGenericTableAvl(&SyscallTableByIndex, MatchEntry);
                    #endif

                    if (!RtlInsertElementGenericTableAvl(&SyscallTableByIndex, &Entry, sizeof(MUSA_SYSCALL_LIST_ENTRY), nullptr)) {
                        #if defined(DBG)
                        Heap::HeapFree(const_cast<char*>(Entry.Name));
                        #endif

                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        MusaLOG("Failed to re-insert ZwQuerySystemTime: 0x%X", Status);
                        break;
                    }
                }
            }

            // dump NtOS Zw routines.
            const auto BaseOfZwClose = reinterpret_cast<const uint8_t*>(
                RtlFindExportedRoutineByName(MusaCoreLiteNtBase, "ZwClose"));
            if (BaseOfZwClose == nullptr) {
                Status = STATUS_NOT_FOUND;
                MusaLOG("Failed to find ZwClose: 0x%X", Status);
                break;
            }

            #if defined(_X86_)
            size_t SizeOfZwRoutine = 0;
            constexpr size_t OffsetOfZwIndex = 1;
            constexpr size_t OffsetOfTemplate = 5;

            const auto ZwCodeTemplate = *reinterpret_cast<const uint64_t*>(BaseOfZwClose + OffsetOfTemplate);
            for (size_t Idx = OffsetOfTemplate + 1; Idx < 0x100; ++Idx) {
                if (*reinterpret_cast<const uint64_t*>(BaseOfZwClose + Idx) == ZwCodeTemplate) {
                    SizeOfZwRoutine = Idx - OffsetOfTemplate;
                    break;
                }
            }
            #elif defined(_AMD64_)
            size_t           SizeOfZwRoutine  = 0;
            size_t           OffsetOfZwIndex  = 0;
            constexpr size_t OffsetOfTemplate = 0;

            const auto ZwCodeTemplate = *reinterpret_cast<const uint64_t*>(BaseOfZwClose + OffsetOfTemplate);
            for (size_t Idx = OffsetOfTemplate + 1; Idx < 0x100; ++Idx) {
                if (*reinterpret_cast<const uint64_t*>(BaseOfZwClose + Idx) == ZwCodeTemplate) {
                    SizeOfZwRoutine = Idx - OffsetOfTemplate;
                    break;
                }
            }

            for (size_t Idx = 0; Idx < 0x100; ++Idx) {
                if (*(BaseOfZwClose + SizeOfZwRoutine - (Idx + 0)) == 0xE9 && // jmp imm32
                    *(BaseOfZwClose + SizeOfZwRoutine - (Idx + 5)) == 0xB8) {
                    // mov rax, imm32

                    OffsetOfZwIndex = SizeOfZwRoutine - (Idx + 5) + 1;
                    break;
                }
            }
            #elif defined(_ARM64_)
            size_t SizeOfZwRoutine = 0;

            // 10 00 80 D2  0xD2800010   MOV X16, #n  imm16(5~20)
            // 00 00 00 14  0x14000000   B   #offset  imm26(0~25)
            constexpr auto ZwCodeTemplate     = 0x14000000D2800010ull;
            constexpr auto ZwCodeTemplateMask = 0xFC000000FFE0001Full;
            for (size_t Idx = 1; Idx < 0x100; ++Idx) {
                if ((*reinterpret_cast<const uint64_t*>(BaseOfZwClose + Idx) & ZwCodeTemplateMask) == ZwCodeTemplate) {
                    SizeOfZwRoutine = Idx;
                    break;
                }
            }
            #endif

            PVOID ImageBaseOfNtos = nullptr;
            RtlPcToFileHeader(const_cast<uint8_t*>(BaseOfZwClose), &ImageBaseOfNtos);

            auto NtHeaderOfNtos = RtlImageNtHeader(ImageBaseOfNtos);
            if (NtHeaderOfNtos == nullptr) {
                Status = STATUS_INVALID_IMAGE_FORMAT;
                MusaLOG("Failed to get ntoskrnl header: 0x%X", Status);
                break;
            }

            auto SectionOfNtos = Utils::PEParser::ImageRvaToSection(NtHeaderOfNtos, ImageBaseOfNtos,
                static_cast<ULONG>(BaseOfZwClose - static_cast<const uint8_t*>(ImageBaseOfNtos)));
            if (SectionOfNtos == nullptr) {
                Status = STATUS_INVALID_IMAGE_FORMAT;
                MusaLOG("Failed to get ntoskrnl section: 0x%X", Status);
                break;
            }

            const auto CodeSectionSize = SectionOfNtos->Misc.VirtualSize;
            const auto CodeSectionBase = static_cast<const uint8_t*>(ImageBaseOfNtos) + SectionOfNtos->VirtualAddress;

            const uint8_t* FirstZwRoutine = nullptr;
            for (size_t Idx = 0; Idx < CodeSectionSize; ++Idx) {
                #if defined(_X86_) || defined(_AMD64_)
                if (*reinterpret_cast<const uint64_t*>(CodeSectionBase + Idx) == ZwCodeTemplate) {
                    FirstZwRoutine = CodeSectionBase + Idx - OffsetOfTemplate;
                    break;
                }
                #elif defined(_ARM64_)
                if ((*reinterpret_cast<const uint64_t*>(CodeSectionBase + Idx) & ZwCodeTemplateMask) == ZwCodeTemplate) {
                    FirstZwRoutine = CodeSectionBase + Idx;
                    break;
                }
                #endif
            }

            if (FirstZwRoutine == nullptr) {
                Status = STATUS_NOT_FOUND;
                MusaLOG("Failed to find Zw routine: 0x%X", Status);
                break;
            }

            const auto CountOfZwRoutine = RtlNumberGenericTableElementsAvl(&SyscallTableByIndex);
            for (size_t Idx = 0; Idx < CountOfZwRoutine; ++Idx) {
                const auto ZwRoutine = FirstZwRoutine + (Idx * SizeOfZwRoutine);

                MUSA_SYSCALL_LIST_ENTRY Entry{};

                #if defined(_X86_) || defined(_AMD64_)
                if (*reinterpret_cast<const uint64_t*>(ZwRoutine + OffsetOfTemplate) != ZwCodeTemplate) {
                    break;
                }
                Entry.Index = *reinterpret_cast<const uint32_t*>(ZwRoutine + OffsetOfZwIndex);
                #elif defined(_ARM64_)
                if ((*reinterpret_cast<const uint64_t*>(ZwRoutine) & ZwCodeTemplateMask) != ZwCodeTemplate) {
                    break;
                }
                // B0 0A 80 D2      MOV X16, #0x55 ; imm16(5~20 bit)
                Entry.Index = (*reinterpret_cast<const uint32_t*>(ZwRoutine) >> 5) & 0xFFFF;
                #endif

                if (const auto MatchEntry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(
                    RtlLookupElementGenericTableAvl(&SyscallTableByIndex, &Entry))) {
                    MatchEntry->Address = Utils::FastEncodePointer(ZwRoutine);
                }
            }

            // SyscallTableByIndex copy to SyscallTableByName
            for (auto Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, TRUE));
                 Entry;
                 Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, FALSE))) {

                MUSA_SYSCALL_LIST_ENTRY NewEntry = *Entry;

                #if defined(DBG)
                if (Entry->Name) {
                    const auto NameLength = strlen(Entry->Name);

                    NewEntry.Name = static_cast<char*>(Heap::HeapAllocate(NameLength + sizeof("")));
                    if (NewEntry.Name == nullptr) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        break;
                    }
                    strcpy_s(const_cast<char*>(NewEntry.Name), NameLength + _countof(""), Entry->Name);
                }
                #endif

                if (!RtlInsertElementGenericTableAvl(&SyscallTableByName, &NewEntry, sizeof(MUSA_SYSCALL_LIST_ENTRY), nullptr)) {
                    #if defined(DBG)
                    Heap::HeapFree(const_cast<char*>(NewEntry.Name));
                    #endif

                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }
            }
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to copy SyscallTableByIndex to SyscallTableByName: 0x%X", Status);
                break;
            }

            #if defined(DBG)
            for (auto Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, TRUE));
                 Entry;
                 Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, FALSE))) {

                MusaLOG("0x%04X -> 0x%p, %s", Entry->Index, Utils::FastDecodePointer(Entry->Address), Entry->Name);
            }
            #endif
        }
        while (false);

        if (ImageBaseOfNtdll) {
            (void)MmUnmapViewInSystemSpace(ImageBaseOfNtdll);
        }

        if (SectionObject) {
            ObDereferenceObject(SectionObject);
        }

        if (SectionHandle) {
            (void)ZwClose(SectionHandle);
        }

        return Status;
    }
#else
    _Must_inspect_result_
    _IRQL_requires_max_(APC_LEVEL)
    NTSTATUS MUSA_API SystemCallSetup()
    {
        NTSTATUS Status;

        RtlInitializeGenericTableAvl(&SyscallTableByName,
            &SyscallTableRoutines::CompareByNameHash,
            &SyscallTableRoutines::Allocate,
            &SyscallTableRoutines::Free,
            nullptr);

        RtlInitializeGenericTableAvl(&SyscallTableByIndex,
            &SyscallTableRoutines::CompareByIndex,
            &SyscallTableRoutines::Allocate,
            &SyscallTableRoutines::Free,
            nullptr);

        do {
            // dump ntdll exports
            Status = Utils::PEParser::ImageEnumerateExports([](const uint32_t Ordinal, const char* Name, const void* Address, void* Context)->NTSTATUS
            {
                UNREFERENCED_PARAMETER(Ordinal);
                UNREFERENCED_PARAMETER(Context);

                NTSTATUS Status = STATUS_SUCCESS;

                if (Name) {
                    if (*reinterpret_cast<const unsigned short*>(Name) == 'wZ') {
                        const auto OpCodeBase = static_cast<const uint8_t*>(Address);
                        const auto NameLength = strlen(Name);

                        MUSA_SYSCALL_LIST_ENTRY Entry;
                        Entry.NameHash = Utils::Fnv1aHash(Name, NameLength);
                        Entry.Address  = Utils::FastEncodePointer(Address);

                        #if defined(_X86_)
                        // B8 42 00 00 00   mov eax, 42h
                        Entry.Index = (*reinterpret_cast<const uint32_t*>(OpCodeBase + 1)) & 0xFFFF;
                        #elif defined(_AMD64_)
                        // 4C 8B D1         mov r10, rcx
                        // B8 55 00 00 00   mov eax, 55h
                        Entry.Index = (*reinterpret_cast<const uint32_t*>(OpCodeBase + 4)) & 0xFFFF;
                        #elif defined(_ARM64_)
                        // A1 0A 00 D4      SVC 0x55 ; imm16(5~20 bit)
                        Entry.Index = (*reinterpret_cast<const uint32_t*>(OpCodeBase) >> 5) & 0xFFFF;
                        #endif

                        #if defined(DBG)
                        Entry.Name = static_cast<char*>(Heap::HeapAllocate(NameLength + sizeof("")));
                        if (Entry.Name == nullptr) {
                            Status = STATUS_INSUFFICIENT_RESOURCES;
                            return Status;
                        }
                        strcpy_s(const_cast<char*>(Entry.Name), NameLength + _countof(""), Name);
                        #endif

                        if (!RtlInsertElementGenericTableAvl(&SyscallTableByIndex, &Entry,
                                                             sizeof(MUSA_SYSCALL_LIST_ENTRY), nullptr)) {

                            #if defined(DBG)
                            Heap::HeapFree(const_cast<char*>(Entry.Name));
                            #endif

                            Status = STATUS_INSUFFICIENT_RESOURCES;
                            return Status;
                        }
                    }
                }
                return Status;
            }, nullptr, MusaCoreLiteNtdllBaseSecure ? MusaCoreLiteNtdllBaseSecure : MusaCoreLiteNtdllBase, true);
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to enumerate ntdll exports: 0x%X", Status);
                break;
            }

            uint32_t IndexOfPrevious = 0;
            uint32_t IndexOfZwQuerySystemTime = static_cast<uint32_t>(~0);

            // SyscallTableByIndex copy to SyscallTableByName
            for (auto Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, TRUE));
                Entry;
                Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, FALSE))) {

                MUSA_SYSCALL_LIST_ENTRY NewEntry = *Entry;
                if (IndexOfZwQuerySystemTime == static_cast<uint32_t>(~0)) {
                    // Fix 'ZwQuerySystemTime' Index - Step 1
                    if (NewEntry.Index - IndexOfPrevious == 2) {
                        IndexOfZwQuerySystemTime = IndexOfPrevious + 1;
                    }
                    else {
                        IndexOfPrevious = NewEntry.Index;
                    }
                }
                else {
                    // Fix 'ZwQuerySystemTime' Index - Step 2
                    if (NewEntry.NameHash == Utils::Fnv1aHash("ZwQuerySystemTime")) {
                        NewEntry.Index = IndexOfZwQuerySystemTime;
                        IndexOfPrevious = Entry->Index; // Re-insert later
                    }
                }

                #if defined(DBG)
                if (Entry->Name) {
                    const auto NameLength = strlen(Entry->Name);

                    NewEntry.Name = static_cast<char*>(Heap::HeapAllocate(NameLength + sizeof("")));
                    if (NewEntry.Name == nullptr) {
                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        break;
                    }
                    strcpy_s(const_cast<char*>(NewEntry.Name), NameLength + _countof(""), Entry->Name);
                }
                #endif

                if (!RtlInsertElementGenericTableAvl(&SyscallTableByName, &NewEntry, sizeof(MUSA_SYSCALL_LIST_ENTRY), nullptr)) {
                    #if defined(DBG)
                    Heap::HeapFree(const_cast<char*>(NewEntry.Name));
                    #endif

                    Status = STATUS_INSUFFICIENT_RESOURCES;
                    break;
                }
            }
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to copy SyscallTableByIndex to SyscallTableByName: 0x%X", Status);
                break;
            }

            // Fix 'ZwQuerySystemTime' Index - Step 3: Re-insert 'ZwQuerySystemTime'
            if (IndexOfZwQuerySystemTime != static_cast<uint32_t>(~0)) {
                MUSA_SYSCALL_LIST_ENTRY Entry{};
                Entry.Index = IndexOfPrevious;

                if (const auto MatchEntry = RtlLookupElementGenericTableAvl(&SyscallTableByIndex, &Entry)) {
                    Entry       = *static_cast<PCMUSA_SYSCALL_LIST_ENTRY>(MatchEntry);
                    Entry.Index = IndexOfZwQuerySystemTime;

                    #if defined(DBG)
                    static_cast<PMUSA_SYSCALL_LIST_ENTRY>(MatchEntry)->Name = nullptr;
                    RtlDeleteElementGenericTableAvl(&SyscallTableByIndex, MatchEntry);
                    #endif

                    if (!RtlInsertElementGenericTableAvl(&SyscallTableByIndex, &Entry, sizeof(MUSA_SYSCALL_LIST_ENTRY),
                        nullptr)) {
                        #if defined(DBG)
                        Heap::HeapFree(const_cast<char*>(Entry.Name));
                        #endif

                        Status = STATUS_INSUFFICIENT_RESOURCES;
                        MusaLOG("Failed to re-insert ZwQuerySystemTime: 0x%X", Status);
                        break;
                    }
                }
            }

            #if defined(DBG)
            for (auto Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, TRUE));
                 Entry;
                 Entry = static_cast<PMUSA_SYSCALL_LIST_ENTRY>(RtlEnumerateGenericTableAvl(&SyscallTableByIndex, FALSE))) {

                MusaLOG("0x%04X -> 0x%p, %s", Entry->Index, Utils::FastDecodePointer(Entry->Address), Entry->Name);
            }
            #endif

        } while (false);

        return Status;
    }
#endif // _KERNEL_MODE

    _Must_inspect_result_
    _IRQL_requires_max_(APC_LEVEL)
    NTSTATUS MUSA_API SystemCallTeardown()
    {
        NTSTATUS Status = STATUS_SUCCESS;

        do {
            for (auto Entry = RtlGetElementGenericTableAvl(&SyscallTableByName, 0);
                Entry;
                Entry = RtlGetElementGenericTableAvl(&SyscallTableByName, 0)) {

                RtlDeleteElementGenericTableAvl(&SyscallTableByName, Entry);
            }

            for (auto Entry = RtlGetElementGenericTableAvl(&SyscallTableByIndex, 0);
                Entry;
                Entry = RtlGetElementGenericTableAvl(&SyscallTableByIndex, 0)) {

                RtlDeleteElementGenericTableAvl(&SyscallTableByIndex, Entry);
            }

        } while (false);

        return Status;
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PVOID MUSA_API GetSystemRoutineAddress(
        _In_ const char* Name
    )
    {
        return GetSystemRoutineAddressByNameHash(Name, Utils::Fnv1aHash(Name, strlen(Name)));
    }

    _IRQL_requires_max_(DISPATCH_LEVEL)
    PVOID MUSA_API GetSystemRoutineAddressByNameHash(
        _In_opt_ const char* Name,
        _In_ size_t          NameHash
    )
    {
        MUSA_SYSCALL_LIST_ENTRY Entry{.NameHash = NameHash};
        if (const auto Matched = static_cast<PCMUSA_SYSCALL_LIST_ENTRY>(RtlLookupElementGenericTableAvl(
            &SyscallTableByName, &Entry))) {
            return const_cast<void*>(Utils::FastDecodePointer(Matched->Address));
        }

        if (Name == nullptr) {
            return nullptr;
        }

        #ifdef _KERNEL_MODE
        return RtlFindExportedRoutineByName(MusaCoreLiteNtBase, Name);
        #else
        return RtlFindExportedRoutineByName(MusaCoreLiteNtdllBase, Name);
        #endif
    }
}


namespace Musa::CoreLite::SyscallTableRoutines
{
    RTL_GENERIC_COMPARE_RESULTS NTAPI CompareByIndex(
        _In_ RTL_AVL_TABLE* Table,
        _In_ PVOID First,
        _In_ PVOID Second
    )
    {
        UNREFERENCED_PARAMETER(Table);

        const auto Entry1 = static_cast<PCMUSA_SYSCALL_LIST_ENTRY>(First);
        const auto Entry2 = static_cast<PCMUSA_SYSCALL_LIST_ENTRY>(Second);

        return Entry1->Index == Entry2->Index
            ? GenericEqual
            : (Entry1->Index > Entry2->Index ? GenericGreaterThan : GenericLessThan);
    }

    RTL_GENERIC_COMPARE_RESULTS NTAPI CompareByNameHash(
        _In_ RTL_AVL_TABLE* Table,
        _In_ PVOID First,
        _In_ PVOID Second
    )
    {
        UNREFERENCED_PARAMETER(Table);

        const auto Entry1 = static_cast<PCMUSA_SYSCALL_LIST_ENTRY>(First);
        const auto Entry2 = static_cast<PCMUSA_SYSCALL_LIST_ENTRY>(Second);

        return Entry1->NameHash == Entry2->NameHash
            ? GenericEqual
            : (Entry1->NameHash > Entry2->NameHash ? GenericGreaterThan : GenericLessThan);
    }

    PVOID NTAPI Allocate(
        _In_ RTL_AVL_TABLE* Table,
        _In_ ULONG Size
    )
    {
        UNREFERENCED_PARAMETER(Table);
        return Heap::HeapAllocate(Size);
    }

    VOID NTAPI Free(
        _In_ RTL_AVL_TABLE* Table,
        _In_ __drv_freesMem(Mem) _Post_invalid_ PVOID Buffer
    )
    {
        UNREFERENCED_PARAMETER(Table);

        #if defined(DBG)
        const auto Entry = reinterpret_cast<PMUSA_SYSCALL_LIST_ENTRY>(static_cast<uint8_t*>(Buffer) + sizeof(
            RTL_BALANCED_LINKS));
        if (Entry->Name) {
            Heap::HeapFree(const_cast<char*>(Entry->Name));
        }
        #endif

        Heap::HeapFree(Buffer);
    }
}
