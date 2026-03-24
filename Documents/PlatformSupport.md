# Platform Support

## Purpose

Life currently targets desktop platforms through a single architecture-aware build model rather than separate project forks per platform.

The workspace uses:

- Premake for project generation
- SDL3 as the platform/runtime backend
- Windows setup and CI paths based on Visual Studio and MSBuild
- Linux setup and CI paths based on `gmake2` and make-based builds
- macOS setup paths that support both `gmake2` and `xcode4`, with CI currently exercising the `gmake2` path

This document describes what is supported today, what is validated in CI, and how architecture selection is implemented in the current scripts.

## Supported Desktop Targets

Life currently has first-class build paths for the following platform and architecture combinations:

- Windows x64
- Windows arm64
- Linux x64
- Linux arm64
- macOS x64
- macOS arm64

Support here means the repository contains architecture-aware setup, dependency resolution, build output layout, and test/runtime path handling for those targets.

## Architecture Selection Model

Architecture selection is centralized rather than duplicated per platform.

In `premake5.lua`:

- `--arch=x64` and `--arch=arm64` are the supported architecture options
- architecture is normalized from common aliases such as `amd64`, `x86_64`, and `aarch64`
- if no architecture is explicitly requested, the build defaults to the host architecture when the host is arm64 and otherwise defaults to `x64`

That same architecture decision flows into:

- generated build output paths
- SDL3 build and install directories
- Windows MSBuild platform selection
- Unix build and test helper resolution

The result is one authoritative build graph with architecture as a parameter rather than separate hand-maintained project trees.

## Build Output Conventions

Architecture is part of both generated output and SDL install layout.

Examples:

- `Build/windows-arm64/Debug/Runtime`
- `Build/linux-arm64/Debug/Test`
- `Build/macosx-arm64/Debug/Runtime`
- `Build/macosx-x64/Debug/Test`
- `Vendor/SDL3/Install/windows/arm64/Release/lib`
- `Vendor/SDL3/Install/linux/arm64/Release/lib`
- `Vendor/SDL3/Install/macos/arm64/Release/lib`
- `Vendor/SDL3/Install/macos/x64/Release/lib`

This naming convention is important because the test and packaging scripts resolve runtime artifacts and SDL dependencies through these architecture-specific paths.

## Host Setup Paths

### Windows

`Setup.bat` resolves the requested `--arch=` value, builds SDL3 into `Vendor/SDL3/Build/windows/<arch>/<config>` and `Vendor/SDL3/Install/windows/<arch>/<config>`, then generates Visual Studio projects with Premake.

For SDL3 builds, Windows maps:

- `x64` to CMake architecture `x64`
- `arm64` to CMake architecture `ARM64`

Typical Windows x64 flow:

```powershell
Setup.bat vs2022 --arch=x64
./Scripts/CI/build_windows.ps1 -Configuration Debug -Platform x64
./Scripts/CI/run_windows_tests.ps1 -Configuration Debug -Platform x64
```

Typical Windows arm64 flow:

```powershell
Setup.bat vs2022 --arch=arm64
./Scripts/CI/build_windows.ps1 -Configuration Debug -Platform ARM64
./Scripts/CI/run_windows_tests.ps1 -Configuration Debug -Platform ARM64
```

Current expectation is Visual Studio 2022 with the matching target toolchain installed.

### Linux

`Setup.sh` resolves the target architecture, builds SDL3 into architecture-specific Linux directories, and generates project files for the requested Premake action. In CI and most documented command-line examples, the action is `gmake2`.

Linux also supports cross-compilation when the host architecture differs from the requested target architecture.

Typical Linux native flow:

```bash
./Setup.sh gmake2 --arch=x64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

Typical Linux arm64 native-host flow:

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

Typical Linux cross-compilation flow:

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all --arch=arm64
```

Current cross-compilation behavior:

- the setup and build scripts look for GNU-style prefixes such as `aarch64-linux-gnu-*` and `x86_64-linux-gnu-*`
- if the default prefix is not correct, `LIFE_LINUX_CROSS_PREFIX` can override it
- `CC`, `CXX`, `AR`, and `RANLIB` can also be set explicitly
- `LIFE_LINUX_CMAKE_TOOLCHAIN_FILE` can be provided for CMake-driven target configuration
- `LIFE_LINUX_SYSROOT` can be provided when an explicit sysroot is required

Cross-compiling a binary does not imply that the same host can execute it. Test execution still requires a compatible runtime environment, either native or emulated.

### macOS

`Setup.sh` supports both `x64` and `arm64` on macOS and passes the selected architecture into the SDL3 CMake build through `CMAKE_OSX_ARCHITECTURES`.

One subtle point is worth documenting explicitly:

- if no Premake action is supplied on macOS, `Setup.sh` defaults to `xcode4`
- CI and many local examples still use `gmake2` explicitly

Both paths are supported by the current setup script, but the make-based path is the one exercised by the build and test helper scripts shown below.

Typical macOS Intel flow:

```bash
./Setup.sh gmake2 --arch=x64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

Typical macOS Apple Silicon flow:

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

## Tool Bootstrap Behavior

The setup scripts are also architecture-aware when bootstrapping build tools.

Current behavior includes:

- `Setup.bat` can download Windows copies of Premake and CMake when they are not already installed
- `Setup.sh` selects host-appropriate CMake archives on Linux and macOS
- on Linux arm64 hosts, `Setup.sh` bootstraps Premake from source and caches the resulting binary under `Scripts/Premake/linux/arm64`

This matters because arm64 support in the repository is not limited to compiler flags. The dependency and tool bootstrap paths are architecture-aware as well.

## CI Validation Boundary

The repository distinguishes between supported build paths and continuously validated paths.

### Current CI Coverage

Current workflow coverage includes:

- Windows x64 quality gates and test execution
- Linux x64 quality gates and test execution
- macOS x64 quality gates on Intel runners
- macOS x64 test execution on Intel runners
- macOS arm64 test execution on Apple Silicon runners
- Windows x64 release packaging
- Linux x64 release packaging
- macOS x64 release packaging
- macOS arm64 release packaging

### Current Non-CI-Validated Paths

The following paths are supported locally by the repository but are not currently part of the active CI matrix:

- Windows arm64
- Linux arm64

That distinction is important. These targets are real supported setup/build paths, but they currently depend on local validation rather than dedicated hosted runners.

## Design Rules

New platform work should preserve the current build model:

- one Premake architecture selection surface
- one SDL install layout convention
- one setup path per host platform
- one set of build and test helpers that resolve architecture from the same naming model

Avoid introducing platform-specific copies of project configuration when an architecture parameter is sufficient. The current repository structure is deliberately trying to keep platform growth additive rather than fragmented.
