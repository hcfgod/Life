# Crash Diagnostics

## Purpose

Life includes a built-in crash-diagnostics layer intended to capture useful information when startup, runtime, or shutdown fails unexpectedly.

The current system serves two related purposes:

- process-level crash handling for unhandled failures
- explicit report generation for handled exceptions and structured diagnostic messages

It is designed as a best-effort reporting facility. It should improve failure visibility, but code should not assume that report generation is always possible under catastrophic process conditions.

## Main API Surface

Crash diagnostics are exposed through `Life::CrashDiagnostics` and `CrashReportingSpecification`.

The main entry points are:

- `Install()`
- `Shutdown()`
- `Configure(...)`
- `GetSpecification()`
- `IsInstalled()`
- `SetApplicationInfo(...)`
- `ReportHandledException(...)`
- `ReportMessage(...)`
- `GetLastReportPath()`

## Configuration Model

`CrashReportingSpecification` currently includes the following fields:

- `Enabled`
- `InstallHandlers`
- `CaptureSignals`
- `CaptureTerminate`
- `CaptureUnhandledExceptions`
- `WriteReport`
- `WriteMiniDump`
- `ReportDirectory`
- `MaxStackFrames`

These values live inside `ApplicationSpecification.CrashReporting`, which makes crash-reporting behavior part of the normal application configuration surface.

Life currently supports one live `ApplicationHost` per process. Crash diagnostics should therefore be treated as a process-level facility whose authoritative application-aware configuration is supplied by the active host.

## Install Timing and Lifecycle

Crash diagnostics are installed in two phases.

### Early Install During Entry Bootstrap

The executable entry path prepares crash diagnostics before `CreateApplicationRunner(...)` is reached. At that point, the engine only knows a generic application name and the raw command line.

This early step exists so bootstrap failures can still produce crash reports.

### Host-Level Rebinding During ApplicationHost Construction

Once `ApplicationHost` has access to the application specification, it updates crash diagnostics with richer information:

- the configured application name
- the normalized command-line vector
- the full crash-reporting policy from `ApplicationSpecification.CrashReporting`, even when it matches the default values

This is the point where crash reporting becomes application-aware rather than bootstrap-generic.

That full reapplication step is intentional. It prevents temporary bootstrap-era configuration from leaking into the active host configuration just because the application later chooses the default crash-reporting policy.

Under the single-host runtime model, code should not assume host-specific crash-diagnostics configuration stacks or automatically restores across nested hosts.

## What Gets Captured

A generated crash report can include:

- timestamp
- application name
- event category
- lifecycle phase
- failure reason
- process ID
- thread ID
- working directory
- reconstructed command line
- signal number on Unix-like platforms
- fault address when available
- Windows exception code when available
- platform metadata from `PlatformDetection`
- active logging configuration details
- textual exception or message details
- stack trace frames
- Windows minidump path when minidump generation succeeds

Relative report directories are resolved to absolute paths before files are written.

## Report File Layout

The text report writer currently produces files with a `.crash.txt` suffix. File names are assembled from:

- sanitized application name
- timestamp token
- process ID
- diagnostic category

On Windows, a matching `.dmp` file may also be written when `WriteMiniDump` is enabled and exception pointers are available.

## Reporting Handled Failures

Crash diagnostics are not limited to fatal, unhandled failures.

### `ReportHandledException(...)`

Use `ReportHandledException(...)` when code catches an exception but still wants to preserve a crash-style artifact for investigation.

The engine uses this path in several authoritative exception boundaries, including:

- executable bootstrap handling through `HandleApplicationBootstrapException(...)` with the `RunApplicationMain` phase
- executable runtime-loop handling through `HandleApplicationRuntimeException(...)` with the `RunApplicationLoop` phase by default
- SDL callback bootstrap handling through `HandleSDLApplicationBootstrapException(...)` with the `SDL_AppInit` phase
- SDL callback runtime handling through `HandleSDLApplicationRuntimeException(...)` with phases such as `SDL_AppIterate` and `SDL_AppEvent`

If the exception is a `Life::Error`, the detailed error string is included in the generated report.

### `ReportMessage(...)`

Use `ReportMessage(...)` when there is no exception object but you still want a structured diagnostic report with a category, message, and phase.

This is useful for operational snapshots, unexpected-but-recoverable states, or integration failures where a report is still valuable.

## Platform Behavior

### Windows

On Windows, the system supports:

- unhandled exception capture
- Windows exception-code reporting
- stack capture through `CaptureStackBackTrace`
- optional minidump generation through `MiniDumpWriteDump`

Minidump writing is best-effort. If minidump creation fails, the text report can still succeed.

### Linux and macOS

On Linux and macOS, the system supports:

- signal-based capture for supported signals
- stack capture through `backtrace` and `backtrace_symbols`
- textual report generation

Signal names are normalized into human-readable labels such as `SIGSEGV`, `SIGABRT`, and `SIGFPE` when available.

## Interaction with Logging

Before writing a report, the crash-reporting path attempts to flush both the core and client loggers.

That behavior helps preserve the most recent log output near the failure boundary. If flushing itself throws, the diagnostics layer suppresses the exception and writes a brief message to `stderr` instead of letting reporting recurse into another failure.

## Interaction with Platform Metadata

The report writer attempts to ensure platform metadata has been initialized before writing the report. When `PlatformDetection` is available, reports include:

- platform name
- architecture
- compiler name and version
- operating system name and version
- executable path
- detected working directory
- build type
- build date and time

This keeps reports useful even when they are collected outside an attached debugger.

## Failure Mode Expectations

Crash reporting is intentionally defensive.

Important expectations:

- report generation may return an empty path if reporting is disabled or file creation fails
- stack traces may be unavailable on some platforms or in some failure modes
- minidump generation is Windows-only and conditional
- catastrophic memory corruption can still prevent useful output

Consumers should treat generated reports as highly valuable when present, but not as a strict guarantee.

## Operational Guidance

Use crash diagnostics to improve investigation quality, not to replace normal error handling.

Recommended practice:

- keep `ApplicationSpecification.CrashReporting` as the authoritative integration point
- use `ReportHandledException(...)` for failures you catch but still need to investigate later
- use `ReportMessage(...)` sparingly for high-value diagnostic snapshots
- keep the report directory writable in local and CI environments
- avoid building additional ad hoc crash-report files elsewhere in the codebase unless there is a clear need

## Design Intent

The current design separates two concerns that often get conflated:

- early process protection
- application-specific reporting context

By installing handlers early and then refining configuration in the host, Life can cover bootstrap failures without sacrificing high-quality application metadata once normal initialization is underway.
