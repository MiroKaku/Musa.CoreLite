# [Musa.CoreLite](https://github.com/MiroKaku/Musa.CoreLite)

[![Actions Status](https://github.com/MiroKaku/Musa.CoreLite/workflows/CI/badge.svg)](https://github.com/MiroKaku/Musa.CoreLite/actions)
[![Downloads](https://img.shields.io/nuget/dt/Musa.CoreLite?logo=NuGet&logoColor=blue)](https://www.nuget.org/packages/Musa.CoreLite/)
[![LICENSE](https://img.shields.io/badge/license-MIT-blue.svg)](https://github.com/MiroKaku/Musa.CoreLite/blob/main/LICENSE)
![Visual Studio](https://img.shields.io/badge/Visual%20Studio-2022-purple.svg)
![Windows](https://img.shields.io/badge/Windows-10+-orange.svg)
![Platform](https://img.shields.io/badge/Windows-X86%7CX64%7CARM64-%23FFBCD9)

* [中文](https://github.com/MiroKaku/Musa.CoreLite/blob/main/README.zh-CN.md)

Musa.CoreLite is a lightweight core library extracted from the Musa.Core project, providing complete support for calling all Zw* system routines for Windows developers.

This project aims to solve the problem of incomplete export symbols for Zw* system routines, allowing developers to directly call all Zw* system routines in both user mode and kernel mode without worrying about missing symbols.

## Usage
Right-click your project, select "Manage NuGet Packages", search for Musa.CoreLite, choose the appropriate version, and click "Install".

> The NuGet package depends on [Musa.Veil](https://github.com/MiroKaku/Musa.Veil). You can directly include `<Veil.h>`

### Header-only Mode
Add the following code to your `.vcxproj` file. In this mode, the lib file will not be automatically linked.

```XML
  <PropertyGroup>
    <MusaCoreOnlyHeader>true</MusaCoreOnlyHeader>
  </PropertyGroup>
```

### Code Example
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

## License
This project is licensed under the MIT License. See [LICENSE](LICENSE) for details.

## Contributing
Contributions are welcome! Please read the [Code of Conduct](CODE_OF_CONDUCT.md) before submitting issues or pull requests.

## Acknowledgements
* Thanks: The implementation was provided by @[xiaobfly](https://github.com/xiaobfly).
