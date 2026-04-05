// 4117: macro redefinition of _KERNEL_MODE — required for non-WDK builds to enable kernel-mode code paths
#pragma warning(suppress: 4117)
#define _KERNEL_MODE 1

#include <Veil.h>
#include <Musa.CoreLite/Musa.CoreLite.h>

// Logging
#define MusaLOG(fmt, ...) DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, \
    "[Musa.Core][%s():%u] " fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)

#define KTEST_EXPECT(expr, name)                                              \
    do {                                                                      \
        ++TestsRun;                                                           \
        if (expr) {                                                           \
            MusaLOG("[PASS] %s", name);                                       \
        }                                                                     \
        else {                                                                \
            ++TestsFailed;                                                    \
            MusaLOG("[FAIL] %s", name);                                       \
        }                                                                     \
    } while (false)


namespace
{
    constexpr size_t Fnv1aHash(const char* Buffer, size_t Count) noexcept
    {
#if defined(_WIN64)
        constexpr size_t FnvOffsetBasis = 14695981039346656037ULL;
        constexpr size_t FnvPrime       = 1099511628211ULL;
#else
        constexpr size_t FnvOffsetBasis = 2166136261U;
        constexpr size_t FnvPrime       = 16777619U;
#endif
        auto Value = FnvOffsetBasis;
        for (size_t idx = 0; idx < Count; ++idx) {
            Value ^= static_cast<size_t>(Buffer[idx]);
            Value *= FnvPrime;
        }
        return Value;
    }

    template<size_t Size>
    constexpr size_t Fnv1aHash(const char(&Buffer)[Size]) noexcept
    {
        return Fnv1aHash(Buffer, Size - 1);
    }
}


EXTERN_C DRIVER_INITIALIZE DriverEntry;
EXTERN_C DRIVER_UNLOAD     DriverUnload;

#ifdef ALLOC_PRAGMA
#pragma alloc_text(INIT, DriverEntry)
#pragma alloc_text(PAGE, DriverUnload)
#endif


namespace Main
{
    static void RunTests()
    {
        ULONG TestsRun    = 0;
        ULONG TestsFailed = 0;

        MusaLOG("=== MusaCoreLite Kernel Test Suite ===");

        KTEST_EXPECT(MusaCoreLiteGetNtdllBase() != nullptr,
            "GetNtdllBase_ReturnsNonNull");

        KTEST_EXPECT(MusaCoreLiteGetSystemRoutine("ZwQuerySystemTime") != nullptr,
            "GetSystemRoutine_KnownName_ReturnsNonNull");

        {
            ULONG ProcessorNumber = ZwGetCurrentProcessorNumber();
            MusaLOG("ZwGetCurrentProcessorNumber() = %lu", ProcessorNumber);
            KTEST_EXPECT(true,
                "GetSystemRoutine_ZwGetCurrentProcessorNumber_Callable");
        }

        KTEST_EXPECT(
            MusaCoreLiteGetSystemRoutine("ZwThisFunctionDefinitelyDoesNotExist") == nullptr,
            "GetSystemRoutine_UnknownName_ReturnsNull");

        {
            constexpr size_t Hash = Fnv1aHash("ZwQuerySystemTime");
            KTEST_EXPECT(MusaCoreLiteGetSystemRoutineByNameHash(Hash) != nullptr,
                "GetSystemRoutineByNameHash_KnownHash_ReturnsNonNull");
        }

        {
            constexpr size_t Hash = Fnv1aHash("ZwThisFunctionDefinitelyDoesNotExist");
            KTEST_EXPECT(MusaCoreLiteGetSystemRoutineByNameHash(Hash) == nullptr,
                "GetSystemRoutineByNameHash_UnknownHash_ReturnsNull");
        }

        {
            static const char* const RoutineNames[] = {
                "ZwClose",
                "ZwCreateFile",
                "ZwQueryInformationProcess",
            };
            for (ULONG i = 0; i < RTL_NUMBER_OF(RoutineNames); ++i) {
                PVOID Address = MusaCoreLiteGetSystemRoutine(RoutineNames[i]);
                KTEST_EXPECT(Address != nullptr, RoutineNames[i]);
            }
        }

        {
            const char* Name = "ZwQuerySystemTime";
            constexpr size_t Hash = Fnv1aHash("ZwQuerySystemTime");
            PVOID AddressByName = MusaCoreLiteGetSystemRoutine(Name);
            PVOID AddressByHash = MusaCoreLiteGetSystemRoutineByNameHash(Hash);
            KTEST_EXPECT(AddressByName == AddressByHash,
                "GetSystemRoutine_ConsistentWithByNameHash");
        }

        MusaLOG("=== Results: %lu/%lu passed ===",
            TestsRun - TestsFailed, TestsRun);

        if (TestsFailed > 0) {
            MusaLOG("%lu test(s) FAILED", TestsFailed);
        }
    }

    static VOID NTAPI BootDriverReinitialize(
        _In_ PDRIVER_OBJECT DriverObject,
        _In_opt_ PVOID Context,
        _In_ ULONG Count
    )
    {
        UNREFERENCED_PARAMETER(Context);

        NTSTATUS Status = MusaCoreLiteStartup();
        if (Status == STATUS_RETRY) {
            MusaLOG("MusaCoreLite still not ready (attempt %lu), re-registering", Count);
            IoRegisterBootDriverReinitialization(DriverObject, BootDriverReinitialize, nullptr);
            return;
        }

        if (!NT_SUCCESS(Status)) {
            MusaLOG("MusaCoreLite reinitialization failed: 0x%08lX", Status);
            return;
        }

        MusaLOG("MusaCoreLite reinitialization succeeded (attempt %lu)", Count);
        RunTests();
    }

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
        UNREFERENCED_PARAMETER(RegistryPath);

        NTSTATUS Status;

        do {
            DriverObject->DriverUnload = Main::DriverUnload;

            Status = MusaCoreLiteStartup();
            if (Status == STATUS_RETRY) {
                MusaLOG("MusaCoreLite deferred (boot-start), registering reinitialization");
                IoRegisterBootDriverReinitialization(DriverObject, BootDriverReinitialize, nullptr);
                Status = STATUS_SUCCESS;
                break;
            }
            if (!NT_SUCCESS(Status)) {
                MusaLOG("Failed to initialize MusaCoreLite, 0x%08lX", Status);
                break;
            }

            RunTests();
        } while (false);

        if (!NT_SUCCESS(Status)) {
            Main::DriverUnload(DriverObject);
        }

        return Status;
    }
}
