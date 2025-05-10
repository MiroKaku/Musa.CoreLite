#include "Musa.Utilities.PEParser.h"

#ifndef _KERNEL_MODE
typedef const IMAGE_EXPORT_DIRECTORY* PCIMAGE_EXPORT_DIRECTORY;
#endif

namespace Musa::Utils::PEParser
{
    NTSTATUS NTAPI ImageEnumerateExports(
        _In_     NTSTATUS(CALLBACK* Callback)(uint32_t Ordinal, const char* Name, const void* Address, void* Context),
        _In_opt_ void* Context,
        _In_     void* BaseOfImage,
        _In_     bool  MappedAsImage
    )
    {
        NTSTATUS Status = STATUS_SUCCESS;

        ULONG DataEntrySize   = 0;
        const void* DataEntry = RtlImageDirectoryEntryToData(BaseOfImage, MappedAsImage,
            IMAGE_DIRECTORY_ENTRY_EXPORT, &DataEntrySize);
        if (DataEntry == nullptr) {
            Status = STATUS_INVALID_IMAGE_FORMAT;
            return Status;
        }

        const auto ExportEntry    = static_cast<PCIMAGE_EXPORT_DIRECTORY>(DataEntry);
        const auto AddressOfNames = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(BaseOfImage) + ExportEntry->AddressOfNames);
        const auto AddressOfFuncs = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(BaseOfImage) + ExportEntry->AddressOfFunctions);
        const auto AddressOfOrdis = reinterpret_cast<uint16_t*>(static_cast<uint8_t*>(BaseOfImage) + ExportEntry->AddressOfNameOrdinals);

        for (uint32_t Ordinal = 0; Ordinal < ExportEntry->NumberOfFunctions; ++Ordinal) {
            if (0 == AddressOfFuncs[Ordinal]) {
                continue;
            }

            const char*     ExportName    = nullptr;
            const uint32_t  ExportOrdinal = Ordinal + ExportEntry->Base;
            const void*     ExportAddress = static_cast<void*>(static_cast<uint8_t*>(BaseOfImage) + AddressOfFuncs[Ordinal]);

            for (uint32_t Idx = 0u; Idx <= ExportEntry->NumberOfNames; ++Idx) {
                if (Ordinal == AddressOfOrdis[Idx]) {
                    ExportName = reinterpret_cast<const char*>(static_cast<uint8_t*>(BaseOfImage) + AddressOfNames[Idx]);
                    break;
                }
            }

            Status = Callback(ExportOrdinal, ExportName, ExportAddress, Context);
            if (NT_SUCCESS(Status)) {
                break;
            }
            if (Status != STATUS_CALLBACK_BYPASS) {
                break;
            }
        }
        if (Status == STATUS_CALLBACK_BYPASS) {
            Status = STATUS_SUCCESS;
        }

        return Status;
    }

    PIMAGE_SECTION_HEADER NTAPI ImageRvaToSection(
        _In_ PIMAGE_NT_HEADERS NtHeaders,
        _In_ PVOID BaseOfImage,
        _In_ ULONG Rva
    )
    {
        UNREFERENCED_PARAMETER(BaseOfImage);

        auto NtSection = IMAGE_FIRST_SECTION(NtHeaders);
        for (auto Idx = 0; Idx < NtHeaders->FileHeader.NumberOfSections; Idx++) {
            if (Rva >= NtSection->VirtualAddress &&
                Rva < NtSection->VirtualAddress + NtSection->SizeOfRawData
                ) {
                return NtSection;
            }
            ++NtSection;
        }

        return nullptr;
    }
}
