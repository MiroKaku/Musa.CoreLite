#include <Veil.h>
#include <Musa.CoreLite/Musa.CoreLite.h>

#include <gtest/gtest.h>


// Logging
#ifdef _DEBUG
#define MusaLOG(fmt, ...) printf("[Musa.CoreLite][%s():%u] " fmt "\n", __FUNCTION__, __LINE__, ## __VA_ARGS__)
#else
#define MusaLOG(...)
#endif


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


// ---------------------------------------------------------------------------
// Test fixture: manages MusaCoreLite lifecycle
// ---------------------------------------------------------------------------

class MusaCoreLiteTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        NTSTATUS Status = MusaCoreLiteStartup();
        ASSERT_EQ(Status, (NTSTATUS)STATUS_SUCCESS)
            << "MusaCoreLiteStartup failed: 0x" << std::hex << Status;
    }

    static void TearDownTestSuite()
    {
        NTSTATUS Status = MusaCoreLiteShutdown();
        EXPECT_EQ(Status, (NTSTATUS)STATUS_SUCCESS)
            << "MusaCoreLiteShutdown failed: 0x" << std::hex << Status;
    }
};


// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(MusaCoreLiteTest, GetSystemRoutine_KnownName_ReturnsNonNull)
{
    PVOID Address = MusaCoreLiteGetSystemRoutine("ZwQuerySystemTime");
    EXPECT_NE(Address, nullptr)
        << "Expected ZwQuerySystemTime to resolve to a valid address";
}

TEST_F(MusaCoreLiteTest, GetSystemRoutine_ZwGetCurrentProcessorNumber_Callable)
{
    ULONG ProcessorNumber = ZwGetCurrentProcessorNumber();
    MusaLOG("ZwGetCurrentProcessorNumber() = %lu", ProcessorNumber);
    (void)ProcessorNumber;
    SUCCEED();
}

TEST_F(MusaCoreLiteTest, GetSystemRoutine_UnknownName_ReturnsNull)
{
    PVOID Address = MusaCoreLiteGetSystemRoutine(
        "ZwThisFunctionDefinitelyDoesNotExist");
    EXPECT_EQ(Address, nullptr)
        << "Expected unknown routine to resolve to nullptr";
}

TEST_F(MusaCoreLiteTest, GetSystemRoutineByNameHash_KnownHash_ReturnsNonNull)
{
    constexpr size_t Hash = Fnv1aHash("ZwQuerySystemTime");
    PVOID Address = MusaCoreLiteGetSystemRoutineByNameHash(Hash);
    EXPECT_NE(Address, nullptr)
        << "Expected ZwQuerySystemTime hash to resolve to a valid address";
}

TEST_F(MusaCoreLiteTest, GetSystemRoutineByNameHash_UnknownHash_ReturnsNull)
{
    constexpr size_t Hash = Fnv1aHash("ZwThisFunctionDefinitelyDoesNotExist");
    PVOID Address = MusaCoreLiteGetSystemRoutineByNameHash(Hash);
    EXPECT_EQ(Address, nullptr)
        << "Expected unknown hash to resolve to nullptr";
}

TEST_F(MusaCoreLiteTest, GetSystemRoutine_MultipleKnownRoutines)
{
    static const char* const RoutineNames[] = {
        "ZwClose",
        "ZwCreateFile",
        "ZwQueryInformationProcess",
    };

    for (const auto* Name : RoutineNames) {
        PVOID Address = MusaCoreLiteGetSystemRoutine(Name);
        EXPECT_NE(Address, nullptr)
            << Name << " should resolve to a valid address";
    }
}

TEST_F(MusaCoreLiteTest, GetSystemRoutine_ConsistentWithByNameHash)
{
    const char* Name = "ZwQuerySystemTime";
    constexpr size_t Hash = Fnv1aHash("ZwQuerySystemTime");

    PVOID AddressByName = MusaCoreLiteGetSystemRoutine(Name);
    PVOID AddressByHash = MusaCoreLiteGetSystemRoutineByNameHash(Hash);

    EXPECT_EQ(AddressByName, AddressByHash)
        << "ByName and ByNameHash should resolve to the same address";
}


// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int wmain(int argc, wchar_t* argv[])
{
    std::vector<std::string> args;
    std::vector<char*> cargs;
    args.reserve(argc);
    cargs.reserve(argc);

    for (int i = 0; i < argc; ++i) {
        int len = WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        args.emplace_back(len - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, argv[i], -1, args.back().data(), len, nullptr, nullptr);
        cargs.push_back(args.back().data());
    }

    int cargc = static_cast<int>(cargs.size());
    char** cargv = cargs.data();

    ::testing::InitGoogleTest(&cargc, cargv);
    return RUN_ALL_TESTS();
}
