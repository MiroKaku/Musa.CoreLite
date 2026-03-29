# [Musa.CoreLite](https://github.com/MiroKaku/Musa.CoreLite)

[![Actions Status](https://github.com/MiroKaku/Musa.CoreLite/workflows/CI/badge.svg)](https://github.com/MiroKaku/Musa.CoreLite/actions)
[![Downloads](https://img.shields.io/nuget/dt/Musa.CoreLite?logo=NuGet&logoColor=blue)](https://www.nuget.org/packages/Musa.CoreLite/)
[![LICENSE](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/MiroKaku/Musa.CoreLite/blob/main/LICENSE)
![Visual Studio](https://img.shields.io/badge/Visual%20Studio-2022+-purple.svg)
![Windows](https://img.shields.io/badge/Windows-10+-orange.svg)
![Platform](https://img.shields.io/badge/Windows-X86%7CX64%7CARM64-%23FFBCD9)

* [English](https://github.com/MiroKaku/Musa.CoreLite/blob/main/README.md)

Musa.CoreLite 是从 Musa.Core 项目中独立出来的轻量级核心库，专注于 Zw* 系统例程调用，为 Windows 开发者提供完整的 Zw* 系统调用支持。

该项目旨在解决 Zw* 系统例程导出符号不完整的问题，使开发者能够在用户模式和内核模式下直接调用所有 Zw* 系统例程而无需担心符号缺失的问题。

## 使用方法
右键单击该项目并选择“管理 NuGet 包”，然后搜索 Musa.CoreLite 并选择适合你的版本，最后单击“安装”。

> NuGet 包依赖 [Musa.Veil](https://github.com/MiroKaku/Musa.Veil)，你可以直接包含 `<Veil.h>`。

### 仅头文件模式
在你的 `.vcxproj` 文件中添加以下代码，在该模式下不会自动引入 lib 文件。

```XML
  <PropertyGroup>
    <MusaCoreOnlyHeader>true</MusaCoreOnlyHeader>
  </PropertyGroup>
```

### 代码示例

#### 内核模式 (Driver)

[Musa.CoreLite.TestForDriver.cpp](Musa.CoreLite.TestForDriver/Musa.CoreLite.TestForDriver.cpp)

```cpp
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

// 在 DriverUnload 中：
(void)MusaCoreLiteShutdown();
```

#### 用户模式

```cpp
#include <Veil.h>
#include <Musa.CoreLite/Musa.CoreLite.h>

int main()
{
    NTSTATUS Status = MusaCoreLiteStartup();
    if (!NT_SUCCESS(Status)) {
        return Status;
    }

    // 调用任意 Zw* 系统例程
    auto ZwGetCurrentProcessorNumber =
        reinterpret_cast<decltype(&::ZwGetCurrentProcessorNumber)>(
            MusaCoreLiteGetSystemRoutine("ZwGetCurrentProcessorNumber"));

    if (ZwGetCurrentProcessorNumber) {
        printf("Current processor number: %lu\n", ZwGetCurrentProcessorNumber());
    }

    (void)MusaCoreLiteShutdown();
    return 0;
}
```

## API 使用约定

- **参数合法性由调用方负责。** 出于性能考虑，库不对输入参数进行校验。传入无效指针或不匹配的哈希值将导致未定义行为。
- `MusaCoreLiteStartup()` 必须在调用其他 API 之前恰好调用一次，且不可并发调用。`MusaCoreLiteShutdown()` 同理。
- `MusaCoreLiteGetSystemRoutine` / `MusaCoreLiteGetSystemRoutineByNameHash` 在 `MusaCoreLiteStartup()` 成功返回后可安全并发调用。

## 许可证
本项目基于 MIT 许可证开源。详情见 [LICENSE](LICENSE)。

## 贡献
欢迎贡献代码！提交 issue 或 pull request 前请阅读 [行为准则](CODE_OF_CONDUCT.md)。

## 感谢
* 感谢：实现方案由 [@xiaobfly](https://github.com/xiaobfly) 提供。

