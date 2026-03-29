#pragma once

namespace Musa::Utils::PEParser
{
    // Callback return value convention for ImageEnumerateExports:
    //   STATUS_SUCCESS (NT_SUCCESS)   -> stop enumeration, treat as "found"
    //   STATUS_CALLBACK_BYPASS        -> continue to next export
    //   Any other failure status      -> stop enumeration, propagate error
    // If all callbacks return STATUS_CALLBACK_BYPASS, final result is STATUS_SUCCESS.
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
