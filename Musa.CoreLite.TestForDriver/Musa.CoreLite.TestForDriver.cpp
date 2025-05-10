// unnecessary, fix ReSharper's code analysis.
#pragma warning(suppress: 4117)
#define _KERNEL_MODE 1

#include <Veil.h>
#include <Musa.CoreLite/Musa.CoreLite.h>

// Logging
#ifdef _DEBUG
#define MusaLOG(fmt, ...) DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, \
    "[Musa.Core][%s():%u] " fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define MusaLOG(...)
#endif


EXTERN_C DRIVER_INITIALIZE DriverEntry;
EXTERN_C DRIVER_UNLOAD     DriverUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#endif


namespace Main
{
    EXTERN_C VOID DriverUnload(
        _In_ PDRIVER_OBJECT DriverObject
    )
    {
        PAGED_CODE();
        UNREFERENCED_PARAMETER(DriverObject);

        (void)MusaCoreLiteShutdown();
    }

    EXTERN_C NTSTATUS DriverEntry(
        _In_ PDRIVER_OBJECT  DriverObject,
        _In_ PUNICODE_STRING RegistryPath
    )
    {
        PAGED_CODE();
        UNREFERENCED_PARAMETER(DriverObject);
        UNREFERENCED_PARAMETER(RegistryPath);

        NTSTATUS Status;

        do {
            DriverObject->DriverUnload = Main::DriverUnload;

            Status = MusaCoreLiteStartup();
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to initialize MusaCoreLite, 0x%08lX", Status);
                break;
            }

            MusaLOG("Test ZwGetCurrentProcessorNumber() return %lu",
                ZwGetCurrentProcessorNumber());
        } while (false);

        if (!NT_SUCCESS(Status)) {
            Main::DriverUnload(DriverObject);
        }

        return Status;
    }
}
