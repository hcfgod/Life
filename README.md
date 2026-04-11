# Life Engine

Life is a custom C++ engine focused on building a clean, professional engine foundation with explicit ownership, strong lifecycle boundaries, practical cross-platform tooling, and a host-owned runtime architecture.

## What This Repository Is

The repository is organized around four primary targets:

- `Engine` - the core engine library containing application lifecycle, services, layers, overlays, input, logging, crash diagnostics, platform abstractions, rendering systems, camera management, and tooling integration points.
- `Runtime` - the executable app that consumes `Engine`; treat it as the current game/runtime sandbox and integration sample for engine development.
- `Editor` - the dedicated editor application target built on the same engine architecture, with docking UI and an offscreen scene-surface path.
- `Test` - the doctest-based validation target for engine behavior, including lifecycle, event-flow, graphics, tooling, and regression coverage.

The current platform layer is SDL-backed, with the runtime owning SDL subsystem lifetime and event polling while window objects stay focused on window state and native handles. The active rendering/tooling path is Vulkan-backed through NVRHI, with higher-level engine services layered above the backend device.

## Current Engine Capabilities

Building on the core engine library, the repository now provides a broader engine foundation that includes a range of features and services.

- `ApplicationRunner` and `ApplicationHost` provide the authoritative loop, lifecycle sequencing, and host-owned service model.
- `ServiceRegistry` is host-owned and exposed through application-facing accessors so runtime systems can be consumed without falling back to globals.
- `LayerStack` is host-owned and integrated into update, render, and event routing, with distinct layer and overlay ordering semantics.
- `InputSystem` is host-owned and provides raw-state polling, action-based input, rebinding, and SDL-fed device state.
- `GraphicsDevice` is the backend abstraction, with `Renderer` as the general rendering service, `Renderer2D` as the built-in quad renderer, `SceneRenderer2D` as the higher-level scene submission seam, and `SceneSurface` as the engine-owned offscreen tooling surface.
- `Camera` and `CameraManager` provide named camera ownership, orthographic and perspective projections, primary-camera selection, per-camera clear settings, aspect-ratio updates, and viewport handling.
- `ImGuiSystem` is host-owned and provides the current tooling bridge for docking UI, input capture, and texture-backed editor panels.
- `Runtime` demonstrates scene-facing rendering, input actions, multi-camera usage, overlays, and host-owned engine services in a normal app context.
- `Editor` demonstrates a dedicated tooling app with docking panels, stats, hierarchy/inspector-style UI, and an offscreen scene surface rendered through engine services.
- Logging, crash diagnostics, structured error handling, and platform/runtime metadata are integrated as first-class engine systems rather than ad hoc utilities.

## Current Goals

The current direction of the project is to keep turning the repository into a solid custom engine rather than a narrow engine-core prototype. In practice that means:

- keeping ownership and lifetime boundaries explicit
- maintaining a clean application/runtime API
- preserving deterministic event flow and shutdown behavior
- growing rendering, tooling, and input features on top of stable host-owned systems
- keeping runtime and editor applications as real consumers of the engine API
- building reliable Windows, Linux, and macOS workflows
- strengthening CI and test coverage as engine systems mature

## Supported Platforms

The workspace currently has first-class build support for:

- Windows x64 via Visual Studio 2022 / MSBuild
- Windows arm64 via Visual Studio 2022 / MSBuild
- Linux x64 via Clang and GNU Make (`gmake2` generation)
- Linux arm64 via Clang and GNU Make (`gmake2` generation) on arm64 hosts
- macOS x64 via Clang, with setup paths supporting both `gmake2` and `xcode4`
- macOS arm64 / Apple Silicon via Clang, with setup paths supporting both `gmake2` and `xcode4`

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

   If no action is supplied on macOS, `Setup.sh` currently defaults to `xcode4`. The documented examples here use `gmake2` because that is the path exercised by the build and test helper scripts.

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

`Runtime` is the current executable entrypoint for exercising the engine in a real app context. It is the place to wire up engine systems, validate gameplay-facing runtime behavior, and evolve the public engine-facing API from a consumer's point of view.

At the moment it serves as a practical integration sample for:

- layer attachment and teardown
- action-based input
- host-owned camera registration and switching
- camera-driven 2D scene submission through `SceneRenderer2D`, backed by `Renderer2D` textured-quad batching
- resize-driven camera aspect-ratio updates
- runtime overlays and diagnostics logging

## What `Editor` Is For

`Editor` is the dedicated tooling-facing application for the engine.

It exercises the same host-owned architecture as the runtime while focusing on editor-style workflows rather than gameplay/runtime presentation.

At the moment it serves as a practical integration sample for:

- host-owned `ImGuiSystem` initialization and docking UI
- overlay-driven editor shell composition
- offscreen scene-surface rendering into the editor `Scene` panel through `SceneRenderer2D` and `SceneSurface`
- camera inspection and perspective editor-camera ownership through `CameraManager`
- tooling input capture layered on top of the normal event and input pipeline

## What `Test` Is For

`Test` exists to validate engine behavior independently of the runtime and editor executables. It is the right place for fast checks of core systems such as events, application lifecycle, shutdown behavior, graphics-service fallback behavior, tooling integration seams, and regression coverage for engine refactors.

## Tooling Notes

- `Setup.bat` and `Setup.sh` initialize submodules, resolve/download Premake and CMake if necessary, build SDL3, and generate project files.
- `Setup.bat` accepts `--arch=x64` and `--arch=arm64`; `Setup.sh` does the same for native Unix hosts.
- On Linux arm64 hosts, `Setup.sh` bootstraps Premake from source when a preinstalled `premake5` is not available.
- CI caches downloaded toolchains and reusable SDL build/install outputs to reduce repeated setup work.
- On Windows, both `Runtime` and `Editor` compile the current `Renderer2D` GLSL shaders into SPIR-V output under their target `Assets/Shaders` folders when the Vulkan SDK is available.
- Logging is configured through `ApplicationSpecification.Logging`, giving the engine a thread-safe, configurable logging surface backed by `spdlog` multi-threaded sinks.
- Generated artifacts stay out of source control; the repository tracks source, scripts, configuration, and documentation rather than build outputs.

## Documents

- `Documents/README.md` - index and reading order for the implementation-facing documentation set.
- `Documents/ApplicationArchitecture.md` - the canonical startup path, ownership model, service registry boundaries, and authoritative loop structure.
- `Documents/EntryPointsAndBootstrap.md` - executable entry, SDL callback bootstrap, runner state, exception phases, and teardown boundaries.
- `Documents/LayersAndOverlays.md` - the host-owned layer model, overlay ordering, traversal rules, and attach/detach semantics.
- `Documents/Rendering.md` - rendering ownership, service boundaries, `GraphicsDevice`, `Renderer`, `Renderer2D`, `SceneRenderer2D`, scene surfaces, cameras, and current Vulkan/NVRHI behavior.
- `Documents/EditorAndTooling.md` - the dedicated editor app, host-owned `ImGuiSystem`, tooling event/frame integration, cached editor services, and the current scene-surface path.
- `Documents/InputSystem.md` - host-owned input architecture, action assets, rebinding, and frame semantics.
- `Documents/EventThreadingInvariants.md` - runtime ownership, event ordering, and thread-safety boundaries.
- `Documents/Logging.md` - logging configuration, sink behavior, reconfiguration, and integration guidance.
- `Documents/CrashDiagnostics.md` - crash-reporting lifecycle, report contents, handled-exception reporting, and platform-specific behavior.
- `Documents/ErrorHandling.md` - the structured error model, `Result<T>` conventions, assertions, verification, and system-error translation.
- `Documents/PlatformRuntime.md` - runtime platform metadata, SDL runtime ownership, and low-level platform utilities.
- `Documents/PlatformSupport.md` - target platforms, Windows/Linux/macOS architecture notes, and build/CI expectations.

## Repository Layout

```text
Engine/    Core engine library
Runtime/   Runtime executable / sandbox app
Editor/    Editor executable / tooling app
Test/      Engine tests
Documents/ Engine architecture and integration documentation
Scripts/   Setup, CI, bootstrap, and build helpers
Vendor/    Git submodules and third-party dependencies
```
