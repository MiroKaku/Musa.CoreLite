#pragma once

namespace Musa::Utils::PEParser
{
    NTSTATUS NTAPI ImageEnumerateExports(
        _In_     NTSTATUS(CALLBACK* Callback)(uint32_t Ordinal, const char* Name, const void* Address, void* Context),
        _In_opt_ void* Context,
        _In_     void* BaseOfImage,
        _In_     bool  MappedAsImage
    );

    PIMAGE_SECTION_HEADER NTAPI ImageRvaToSection(
        _In_ PIMAGE_NT_HEADERS NtHeaders,
        _In_ PVOID BaseOfImage,
        _In_ ULONG Rva
    );

}
