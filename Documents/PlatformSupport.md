# Platform Support

## Current Status

Life currently has first-class build paths for the following desktop platform combinations:

- Windows x64
- Windows arm64
- Linux x64
- Linux arm64
- macOS x64
- macOS arm64 (Apple Silicon)

The workspace uses Premake for project generation and SDL3 as the current platform/runtime backend.

## Validation Boundary

Current CI validation covers:

- Windows x64
- Linux x64
- macOS x64
- macOS arm64

Windows arm64 and Linux arm64 are currently local/manual support paths rather than CI-validated matrix targets.

## Architecture Support Model

Architecture support is implemented through the core build surface rather than treated as separate forks:

- `premake5.lua` resolves a target architecture and supports `--arch=x64` and `--arch=arm64`.
- `Setup.bat` resolves `--arch=` for Windows SDL builds and generates architecture-specific install/output paths.
- `Setup.sh` resolves the effective target architecture, builds SDL into architecture-specific install directories, passes `CMAKE_OSX_ARCHITECTURES` for macOS SDL builds, and bootstraps Premake from source on Linux arm64 when needed.
- Unix build/test helper scripts resolve `arm64` outputs and SDL install trees for both Linux and macOS.
- Windows build/test helpers resolve `x64` and `arm64` outputs.
- CI keeps existing Windows/Linux jobs explicitly pinned to `x64` while macOS validates both Intel and Apple Silicon.

## Build Output Conventions

Architecture is part of generated output and SDL install layout.

Examples:

- `Build/windows-arm64/Debug/Runtime`
- `Build/linux-arm64/Debug/Test`
- `Build/macosx-arm64/Debug/Runtime`
- `Build/macosx-x64/Debug/Test`
- `Vendor/SDL3/Install/windows/arm64/Release/lib`
- `Vendor/SDL3/Install/linux/arm64/Release/lib`
- `Vendor/SDL3/Install/macos/arm64/Release/lib`
- `Vendor/SDL3/Install/macos/x64/Release/lib`

## Windows ARM64

Windows ARM64 support uses the same Premake architecture model and Visual Studio/MSBuild path as Windows x64.

Typical local flow:

```powershell
Setup.bat vs2022 --arch=arm64
./Scripts/CI/build_windows.ps1 -Configuration Debug -Platform ARM64
./Scripts/CI/run_windows_tests.ps1 -Configuration Debug -Platform ARM64
```

Current expectation is Visual Studio 2022 with ARM64 toolchain support installed.

## Linux ARM64

Linux ARM64 support works as both a native-host path and a Linux cross-compilation target.

Typical local flow:

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

Typical Linux cross-compilation flow from `x64 -> arm64` or `arm64 -> x64`:

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all --arch=arm64
```

Important limitations:

- Linux cross-compilation requires a matching target toolchain. By default the setup/build helpers look for GNU-style prefixes such as `aarch64-linux-gnu-*` or `x86_64-linux-gnu-*`.
- If the target toolchain is not on the default prefix, set `LIFE_LINUX_CROSS_PREFIX`, `CC` / `CXX` / `AR` / `RANLIB`, or `LIFE_LINUX_CMAKE_TOOLCHAIN_FILE` explicitly.
- `LIFE_LINUX_SYSROOT` may also be provided when the cross build needs an explicit sysroot.
- If `premake5` is not already installed on Linux arm64, `Setup.sh` bootstraps Premake from source automatically and caches the resulting binary under `Scripts/Premake/linux/arm64`.
- CMake bootstrap is architecture-aware on Linux and selects the appropriate host archive for `x64` vs `arm64`.
- Running cross-compiled Linux binaries still requires a compatible runtime environment on the host, either native or emulated.

## Local Build Examples

### macOS Apple Silicon

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

### macOS Intel

```bash
./Setup.sh gmake2 --arch=x64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

## CI Coverage

Current CI intent is:

- Windows x64 test coverage
- Linux x64 test coverage
- macOS Intel test coverage
- macOS Apple Silicon test coverage
- Windows x64 release packaging
- Linux x64 release packaging
- macOS Intel release packaging
- macOS Apple Silicon release packaging

Linux quality-gate workflows remain x64 in CI, and Windows quality gates remain x64 in CI. If arm64 runners become part of the matrix later, the same `--arch=` path should be reused instead of introducing separate project configurations.

## Design Rule

New platform work should preserve a single authoritative build graph:

- one Premake architecture selection model
- one SDL install layout convention
- one setup path per host platform

Avoid adding platform-specific copies of project configuration when an architecture parameter is sufficient.
