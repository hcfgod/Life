# Platform Runtime and Utilities

## Purpose

Life separates build-platform support from runtime platform services.

`PlatformSupport.md` describes target platforms, setup flows, and CI coverage. This document covers the runtime-facing platform surfaces used by the engine at execution time:

- `PlatformDetection`
- `PlatformInfo`
- `PlatformUtils`
- `ApplicationRuntime`
- the current SDL-backed platform runtime implementation

The goal is to make it clear which layer owns platform metadata, which layer owns runtime event polling, and which helpers exist as low-level portability utilities.

## PlatformDetection

`PlatformDetection` is the engine's runtime metadata surface for the current process environment.

Main entry points include:

- `Initialize()`
- `GetPlatformInfo()`
- `RefreshCapabilities()`
- `IsInitialized()`
- convenience predicates such as `IsWindows()`, `IsLinux()`, `IsMacOS()`, `IsX64()`, and `IsARM64()`

### Initialization model

`GetPlatformInfo()` is lazy-safe. If platform detection has not been initialized yet, it initializes on first access.

In the canonical application path, `ApplicationHost` initializes platform detection eagerly during host construction, after logging and crash diagnostics have been configured from `ApplicationSpecification` and before the window is created.

That eager host-side initialization is the authoritative runtime path. Lazy access remains useful for diagnostics, tests, and utility code that needs metadata outside the normal host lifecycle.

### Failure behavior

If initialization fails, `PlatformDetection::Initialize()` logs the failure, translates it into a structured `PlatformError`, and throws.

That makes platform-detection failures visible in both logging and error-reporting paths instead of silently degrading into missing metadata.

## PlatformInfo Contents

`PlatformInfo` currently aggregates several categories of metadata:

- platform kind and human-readable platform name
- architecture kind and human-readable architecture name
- compiler kind, name, and version
- operating system name, version, and build string
- system capabilities such as CPU features and memory totals
- graphics API capability flags
- process-related paths such as executable path, working directory, user-data path, temp path, and system path
- build metadata such as build date, build time, build type, and build version

This data is used by diagnostics, error reporting, crash reporting, logging, and general runtime introspection.

## RefreshCapabilities

`RefreshCapabilities()` currently refreshes dynamic capability information rather than re-running the entire static platform-detection pipeline.

In practice, it updates:

- detected system capabilities
- detected graphics API availability

This makes it the right choice for code that needs to refresh capability snapshots without rebuilding every descriptive string in `PlatformInfo`.

## ApplicationRuntime Boundary

`ApplicationRuntime` owns process-level runtime behavior for the active platform backend.

Its core responsibilities are intentionally small:

- create the platform `Window`
- poll platform events through `PollEvent()`

This boundary keeps window ownership separate from broader runtime ownership. A `Window` owns native window state and handles, while `ApplicationRuntime` owns the runtime-facing platform behavior that surrounds the window.

## Current SDL Runtime Implementation

The current `CreatePlatformApplicationRuntime()` implementation returns an SDL-backed runtime.

Important behaviors in the current SDL layer:

- runtime construction acquires SDL video initialization
- runtime destruction releases SDL video initialization
- SDL video lifetime is reference-counted at the process level
- window creation is delegated to an SDL-backed `Window` implementation
- event polling loops over `SDL_PollEvent(...)` until a translatable engine event is produced or no events remain

That ref-counted SDL runtime lifetime is important. It prevents the process-level SDL video subsystem from being tied implicitly to a single window object while still ensuring that repeated runtime acquisition and release remain well-defined.

## PlatformUtils

`PlatformUtils` is the low-level portability toolbox for engine code that needs common platform services without depending directly on platform-specific APIs at each call site.

### Path helpers

Current path helpers include:

- `GetPathSeparator()`
- `NormalizePath(...)`
- `JoinPath(...)`
- `GetDirectoryName(...)`
- `GetFileName(...)`
- `GetFileExtension(...)`

These are thin convenience wrappers around platform-aware path handling rather than a separate filesystem abstraction layer.

### Environment helpers

Current environment helpers include:

- `GetEnvironmentVariable(...)`
- `SetEnvironmentVariable(...)`

Current behavior is intentionally explicit:

- missing environment variables return `std::nullopt`
- setting a variable returns `bool` success or failure rather than throwing

### Process, thread, and timing helpers

Current helpers include:

- `GetCurrentProcessId()`
- `GetCurrentThreadId()`
- `Sleep(...)`
- `GetHighResolutionTime()`
- `GetSystemTime()`

These are intended as small portability shims for engine infrastructure, diagnostics, and timing-sensitive utility code.

### Dynamic library helpers

Current helpers include:

- `LoadLibrary(...)`
- `GetProcAddress(...)`
- `FreeLibrary(...)`

These are thin wrappers over the native platform loader. Missing libraries or symbols are surfaced as null pointers rather than translated into engine exceptions automatically.

### Memory helpers

Current helpers include:

- `AllocateAligned(...)`
- `FreeAligned(...)`

Aligned allocation failure returns `nullptr`, matching the low-level character of this utility surface.

### Console and debugger helpers

Current helpers include:

- `SetConsoleColor(...)`
- `ResetConsoleColor()`
- `IsConsoleAvailable()`
- `BreakIntoDebugger()`
- `OutputDebugString(...)`

These helpers are primarily intended for diagnostics, testing, and low-level operational tooling.

## Operational Guidance

A few guidelines help keep the platform layer coherent:

- use `PlatformDetection` for structured runtime metadata, not scattered ad hoc platform queries
- use `ApplicationRuntime` for runtime event polling and platform window creation, not `Window` as a hidden process-runtime owner
- use `PlatformUtils` for low-level portability seams where a thin wrapper is enough
- prefer higher-level engine abstractions when they already model the ownership boundary you need

If a new platform-facing feature has engine-wide architectural impact, it usually belongs in `PlatformDetection` or `ApplicationRuntime`. If it is just a narrow portability helper, it usually belongs in `PlatformUtils`.
