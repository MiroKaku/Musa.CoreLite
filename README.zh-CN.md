# [Musa.CoreLite](https://github.com/MiroKaku/Musa.CoreLite)

[![Actions Status](https://github.com/MiroKaku/Musa.CoreLite/workflows/CI/badge.svg)](https://github.com/MiroKaku/Musa.CoreLite/actions)
[![Downloads](https://img.shields.io/nuget/dt/Musa.CoreLite?logo=NuGet&logoColor=blue)](https://www.nuget.org/packages/Musa.CoreLite/)
[![LICENSE](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/MiroKaku/Musa.CoreLite/blob/main/LICENSE)
![Visual Studio](https://img.shields.io/badge/Visual%20Studio-2022-purple.svg)
![Windows](https://img.shields.io/badge/Windows-10+-orange.svg)
![Platform](https://img.shields.io/badge/Windows-X86%7CX64%7CARM64-%23FFBCD9)

* [English](https://github.com/MiroKaku/Musa.CoreLite/blob/main/README.md)

Musa.CoreLite 是从 Musa.Core 项目中将对Zw系统例程功能独立出来的轻量级核心库，为 Windows 平台开发者提供完整的 Zw* 系统调用支持。

该项目旨在解决 Zw* 系统例程导出符号不完整的问题，使开发者能够在用户模式和内核模式下直接调用所有 Zw* 系统例程而无需担心符号缺失的问题。

## 使用方法
右键单击该项目并选择“管理 NuGet 包”，然后搜索 Musa.CoreLite 并选择适合你的版本，最后单击“安装”。

> NuGet 包依赖 [Musa.Veil](https://github.com/MiroKaku/Musa.Veil)，你可以直接包含 `<Veil.h>`

### 仅头文件模式
在你的 `.vcxproj` 文件里面添加下面代码，在这个模式下不会自动引入lib文件。

```XML
  <PropertyGroup>
    <MusaCoreOnlyHeader>true</MusaCoreOnlyHeader>
  </PropertyGroup>
```

### 代码示例
https://github.com/MiroKaku/Musa.CoreLite/blob/main/Musa.CoreLite.TestForDriver/Musa.CoreLite.TestForDriver.cpp?plain=1

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
```

## 许可证
本项目基于 MIT 许可证开源。详情见 [LICENSE](LICENSE)。

## 贡献
欢迎贡献代码！提交 issue 或 pull request 前请阅读 [行为准则](CODE_OF_CONDUCT.md)。

## 感谢
* 感谢：实现方案由 @[xiaobfly](https://github.com/xiaobfly) 提供。
