#include <Veil.h>
#include <Musa.CoreLite/Musa.CoreLite.h>


// Logging
#ifdef _DEBUG
#define MusaLOG(fmt, ...) printf("[Musa.CoreLite][%s():%u] " fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define MusaLOG(...)
#endif


namespace Main
{
    extern"C" int wmain(int argc, wchar_t* argv[], wchar_t* envp[])
    {
        UNREFERENCED_PARAMETER(argc);
        UNREFERENCED_PARAMETER(argv);
        UNREFERENCED_PARAMETER(envp);

        NTSTATUS Status = STATUS_SUCCESS;

        __try {
            Status = MusaCoreLiteStartup();
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to startup MusaCoreLite: 0x%08lX", Status);
                __leave;
            }

            MusaLOG("Test ZwGetCurrentProcessorNumber() return %lu",
                ZwGetCurrentProcessorNumber());
        }
        __finally {
            (void)MusaCoreLiteShutdown();
        }

        return Status;
    }

}
