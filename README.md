# Life Engine

Life is a custom C++ engine workspace focused on building a clean, professional engine foundation with explicit ownership, strong lifecycle boundaries, and practical cross-platform tooling.

## What This Repository Is

The repository is organized around three primary targets:

- `Engine` - the core engine library containing application lifecycle, windowing, events, memory helpers, logging, and platform abstractions.
- `Runtime` - the executable app that consumes `Engine`; treat it as the current sandbox/runtime host for engine development.
- `Test` - the doctest-based validation target for engine behavior, including lifecycle and event-flow coverage.

The current platform layer is SDL-backed, with the runtime owning SDL subsystem lifetime and event polling while window objects stay focused on window state and native handles.

## Current Goals

The current direction of the project is to establish a solid engine core before expanding outward. In practice that means:

- keeping ownership and lifetime boundaries explicit
- maintaining a clean application/runtime API
- preserving deterministic event flow and shutdown behavior
- building reliable Windows, Linux, and macOS workflows
- strengthening CI and test coverage as engine systems mature

## Supported Platforms

The workspace currently has first-class build support for:

- Windows x64 via Visual Studio 2022 / MSBuild
- Windows arm64 via Visual Studio 2022 / MSBuild
- Linux x64 via Clang and GNU Make (`gmake2` generation)
- Linux arm64 via Clang and GNU Make (`gmake2` generation) on arm64 hosts
- macOS x64 via Clang and GNU Make (`gmake2` generation)
- macOS arm64 / Apple Silicon via Clang and GNU Make (`gmake2` generation)

SDL3, `spdlog`, `nlohmann/json`, and `doctest` are brought in through Git submodules. Premake is used to generate project files, and the setup scripts can bootstrap local copies of Premake and CMake when they are not already installed.

Current CI validation remains focused on Windows x64, Linux x64, macOS x64, and macOS arm64. Windows arm64 and Linux arm64 are available as local/manual architecture-aware build paths.

## Building the Workspace

### Windows

1. Run setup and generate Visual Studio files:

   ```bat
   Setup.bat vs2022
   ```

2. Build the workspace:

   ```powershell
   ./Scripts/CI/build_windows.ps1 -Configuration Debug
   ```

3. Run the tests:

   ```powershell
   ./Scripts/CI/run_windows_tests.ps1 -Configuration Debug
   ```

To target Windows ARM64 explicitly:

```powershell
Setup.bat vs2022 --arch=arm64
./Scripts/CI/build_windows.ps1 -Configuration Debug -Platform ARM64
./Scripts/CI/run_windows_tests.ps1 -Configuration Debug -Platform ARM64
```

### Linux

1. Make scripts executable if needed:

   ```bash
   chmod +x Setup.sh Scripts/CI/*.sh
   ```

2. Generate the workspace:

   ```bash
   ./Setup.sh gmake2
   ```

3. Build the workspace:

   ```bash
   ./Scripts/CI/build_make.sh Debug clean all
   ```

4. Run the tests:

   ```bash
   ./Scripts/CI/run_tests.sh Debug
   ```

To target Linux arm64 on an arm64 host explicitly:

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all
./Scripts/CI/run_tests.sh Debug
```

If `premake5` is not already installed on Linux arm64, `Setup.sh` now bootstraps Premake automatically from source and caches the resulting binary under `Scripts/Premake/linux/arm64`.

Linux cross-compilation between `x64` and `arm64` is also supported when a matching GNU cross toolchain is available. Typical examples:

```bash
./Setup.sh gmake2 --arch=arm64
./Scripts/CI/build_make.sh Debug clean all --arch=arm64
```

```bash
./Setup.sh gmake2 --arch=x64
./Scripts/CI/build_make.sh Debug clean all --arch=x64
```

If the toolchain is not on the default prefix for the target, set one of:

- `LIFE_LINUX_CROSS_PREFIX`
- `CC` / `CXX` / `AR` / `RANLIB`
- `LIFE_LINUX_CMAKE_TOOLCHAIN_FILE`
- `LIFE_LINUX_SYSROOT`

Cross-compiling does not imply same-host execution of the produced target binary. Use `./Scripts/CI/run_tests.sh Debug --arch=...` only when the target architecture is runnable on the current machine, either natively or through your configured emulation layer.

### macOS

1. Make scripts executable if needed:

   ```bash
   chmod +x Setup.sh Scripts/CI/*.sh
   ```

2. Generate the workspace:

   ```bash
   ./Setup.sh gmake2
   ```

3. Build the workspace:

   ```bash
   ./Scripts/CI/build_make.sh Debug clean all
   ```

4. Run the tests:

   ```bash
   ./Scripts/CI/run_tests.sh Debug
   ```

To target Apple Silicon explicitly:

```bash
./Setup.sh gmake2 --arch=arm64
```

To target Intel explicitly:

```bash
./Setup.sh gmake2 --arch=x64
```

## What `Runtime` Is For

`Runtime` is the current executable entrypoint for exercising the engine in a real app context. It is the place to wire up engine systems, validate runtime behavior, and evolve the public engine-facing API from a consumer's point of view.

## What `Test` Is For

`Test` exists to validate engine behavior independently of the runtime executable. It is the right place for fast checks of core systems such as events, application lifecycle, shutdown behavior, and regression coverage for engine refactors.

## Tooling Notes

- `Setup.bat` and `Setup.sh` initialize submodules, resolve/download Premake and CMake if necessary, build SDL3, and generate project files.
- `Setup.bat` accepts `--arch=x64` and `--arch=arm64`; `Setup.sh` does the same for native Unix hosts.
- On Linux arm64 hosts, `Setup.sh` bootstraps Premake from source when a preinstalled `premake5` is not available.
- CI caches downloaded toolchains and reusable SDL build/install outputs to reduce repeated setup work.
- Logging is configured through `ApplicationSpecification.Logging`, giving the engine a thread-safe, configurable logging surface backed by `spdlog` multi-threaded sinks.
- Generated artifacts stay out of source control; the repository tracks source, scripts, configuration, and documentation rather than build outputs.

## Documents

- `Documents/PlatformSupport.md` - target platforms, Windows/Linux/macOS architecture notes, and build/CI expectations.
- `Documents/Logging.md` - logging configuration, threading behavior, and integration guidance.
- `Documents/EventThreadingInvariants.md` - runtime ownership, event ordering, and thread-safety boundaries.

## Repository Layout

```text
Engine/    Core engine library
Runtime/   Runtime executable / sandbox app
Test/      Engine tests
Documents/ Engine architecture and integration documentation
Scripts/   Setup, CI, bootstrap, and build helpers
Vendor/    Git submodules and third-party dependencies
```
