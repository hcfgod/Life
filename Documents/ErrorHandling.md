# Error Handling

## Purpose

Life uses a structured error model so failures carry stable identity, severity, and context across runtime code, tooling, tests, and crash reporting.

The system is intentionally flexible rather than doctrinaire. Different subsystems have different control-flow needs, but they should still speak the same failure language.

The current model supports three complementary styles:

- throwing `Life::Error` or a derived type
- returning `Result<T>` or `Result<void>`
- using shared engine helpers such as assertions, verification, and system-error translation

The design goal is not to force every subsystem into a single pattern. It is to ensure that failures remain classifiable, loggable, and diagnosable even when they cross subsystem or exception boundaries.

## Core Types

### `ErrorCode`

`ErrorCode` is the engine's classification layer. It includes general-purpose codes and grouped families for specific domains such as:

- system and file I/O
- platform support and permissions
- graphics and windowing
- audio and input
- resources and configuration
- events, memory, and threading
- security, performance, debugging, and hot reload

The numeric layout is grouped by domain so categories remain easy to scan and extend.

### `ErrorSeverity`

`ErrorSeverity` describes operational importance rather than failure source.

Current levels are:

- `Info`
- `Warning`
- `Error`
- `Critical`
- `Fatal`

Severity affects how `Error::LogError(...)` emits the error and whether detailed information is additionally logged.

### `Error`

`Life::Error` extends `std::exception` with structured metadata.

An `Error` stores:

- `ErrorCode`
- message text
- source location information
- severity
- contextual metadata such as thread ID and timestamp
- optional platform/system information
- optional additional key-value data
- optional translated system error code

The `what()` result is derived from the formatted string representation and remains suitable for ordinary exception reporting.

## Context and Formatting

The constructor captures source-location data and builds initial runtime context.

Depending on engine state, an error can include:

- file, line, column, and function name
- thread identifier
- timestamp
- platform information if `PlatformDetection` has already been initialized
- coarse system information such as CPU count and memory

For richer diagnostics, callers can add structured metadata through `AddContext(...)` and related setters.

## String Surfaces

Two string views of an error are especially important.

### `ToString()`

`ToString()` produces a concise, single-line operational summary. This is the form primarily used for standard logging.

### `ToDetailedString()`

`ToDetailedString()` produces a multi-line diagnostic report including severity, numeric code, location, system-error information, collected context, and additional data.

This richer form is especially useful when:

- an error is critical or fatal
- a crash report needs detailed exception text
- tests or diagnostics want a stable, human-readable failure description

## Derived Error Types

The engine includes convenience derived types for common domains, including:

- `SystemError`
- `PlatformError`
- `GraphicsError`
- `ResourceError`
- `ConfigError`
- `MemoryError`
- `ThreadError`

These do not introduce new behavior. Their value is semantic clarity at throw sites and easier intent-reading during debugging.

## Result Types

`Result<T>` and `Result<void>` provide the engine's explicit success-or-failure path.

They can hold either:

- a success value
- a `Life::Error`

Useful operations include:

- `IsSuccess()`
- `IsFailure()`
- `GetValue()`
- `GetValuePtr()`
- `GetValueOr(...)`
- `GetValueOrThrow()`
- `GetError()`
- `GetErrorPtr()`

Use `Result<T>` when the caller is expected to branch intentionally on the outcome and continue operating in a controlled way. This is especially appropriate at file, platform, serialization, and integration boundaries where failure is a normal possibility rather than a broken invariant.

## Error-Handling Utilities

The `Life::ErrorHandling` namespace supplies shared behavior around errors.

### Global Error Handler

A process-wide error handler can be installed through:

- `SetErrorHandler(...)`
- `GetErrorHandler()`

By default, the handler is `DefaultErrorHandler(...)`.

The default behavior is:

- log the error through `Error::LogError(...)`
- break into the debugger for fatal errors when supported by the platform layer

Because the current runtime model is process-oriented, the error handler should also be treated as process-oriented configuration. It is not intended as a per-system policy switch.

`ScopedErrorHandlerOverride` provides an RAII mechanism for temporary overrides. In practice this is mainly useful in tests and tooling, where an expected failure should not pollute normal logs or debugger behavior.

### Assertions and Verification

The engine exposes:

- `Assert(...)`
- `Verify(...)`
- `LIFE_ASSERT(...)`
- `LIFE_VERIFY(...)`

Current behavior differs in an intentional way:

- failed assertions produce an `ErrorCode::Unknown` error with `Critical` severity
- failed verifications produce an `ErrorCode::InvalidState` error with `Error` severity
- both routes pass the error through the active global error handler and then throw

A useful rule of thumb is:

- use assertions for conditions that indicate a broken invariant
- use verification for conditions that represent an invalid runtime state that still belongs in normal error-reporting paths

### Try Wrappers

`ErrorHandling::Try(...)` and `TryVoid(...)` convert exception-throwing code into `Result<T>` or `Result<void>`.

These helpers catch:

- `Life::Error`
- standard exceptions
- unknown exceptions

Non-`Life::Error` exceptions are translated into `ErrorCode::Unknown` results.

## Error Catalog Utilities

The catalog helpers in `ErrorHandling` make the enum practical to consume.

Available helpers include:

- `GetErrorCodeString(...)`
- `GetErrorCodeDescription(...)`
- `GetErrorCodeSeverity(...)`

Together, these provide a stable mapping from enum value to display name, human-readable description, and default severity.

## System Error Translation

The engine also includes a small portability layer for native system errors.

Available helpers include:

- `GetLastSystemError()`
- `GetSystemErrorString(...)`
- `ConvertSystemError(...)`

Current behavior is platform-specific:

- Windows uses `GetLastError()` and `FormatMessageA(...)`
- Linux and macOS use `errno` and standard error-category translation

This allows platform-facing code to preserve native failure information while still converting it into the engine's structured error taxonomy.

## Convenience Macros

The header defines convenience macros for common patterns, including:

- `LIFE_THROW_ERROR(...)`
- `LIFE_THROW_SYSTEM_ERROR(...)`
- `LIFE_THROW_PLATFORM_ERROR(...)`
- `LIFE_THROW_GRAPHICS_ERROR(...)`
- `LIFE_THROW_RESOURCE_ERROR(...)`
- `LIFE_THROW_CONFIG_ERROR(...)`
- `LIFE_THROW_MEMORY_ERROR(...)`
- `LIFE_THROW_THREAD_ERROR(...)`
- `LIFE_TRY(...)`
- `LIFE_TRY_VOID(...)`
- `LIFE_RETURN_IF_ERROR(...)`
- `LIFE_RETURN_IF_ERROR_VOID(...)`

These are convenience tools, not a substitute for thoughtful API design. Use them when they improve clarity at the call site.

## Recommended Usage Guidelines

A few conventions keep error behavior consistent across the engine.

### Prefer `ErrorCode` That Matches the Failure Domain

Choose the most specific code that accurately describes the failure. Specific codes improve logs, diagnostics, and later automation.

### Use Severity Intentionally

Do not treat severity as a synonym for category. For example, a configuration failure and a memory-corruption failure are both errors, but they do not carry the same operational urgency.

### Add Context When the Call Site Knows More

If a subsystem knows resource names, identifiers, paths, or external-state details, attach them as context rather than burying everything in one message string.

### Choose Between Exceptions and `Result<T>` Deliberately

As a general rule:

- use exceptions when failure should unwind immediately through the current control flow
- use `Result<T>` when the caller is expected to branch explicitly on success vs failure

Both approaches are valid inside the current engine. The important part is preserving `Life::Error` information instead of degrading into opaque failures, ad hoc booleans, or message-only logging.

### Keep the Global Error Handler Stable in Production Paths

The scoped handler override exists mainly for tests and tooling. Production code should usually rely on the default handler and logging configuration rather than swapping global error behavior dynamically.

### Treat Assertions as Invariant Failures

`LIFE_ASSERT(...)` and `Assert(...)` are for conditions that indicate the engine or application has reached a state that should not be possible if the surrounding code is correct.

That is a stronger statement than ordinary runtime failure. Assertions should not become a substitute for input validation, file-format checks, or recoverable platform errors.

### Treat Verification as Recoverable Failure Reporting

`LIFE_VERIFY(...)` and `Verify(...)` are better suited to cases where execution cannot continue normally at the current call site, but the condition still belongs to the engine's standard error-reporting path rather than to an invariant break.

That distinction helps logs and debugger behavior stay meaningful.

## Relationship to Crash Diagnostics

`Life::Error` integrates naturally with crash diagnostics.

When `CrashDiagnostics::ReportHandledException(...)` receives a caught exception that is actually a `Life::Error`, the report writer includes the detailed error string rather than just `what()` output. That means structured engine errors remain useful even after they cross an exception boundary at bootstrap, runtime-loop, or integration-level catch sites.

## Design Intent

The error layer exists to make failures easier to classify, inspect, log, and translate across engine boundaries.

It is not trying to be a full reliability framework or a substitute for sound subsystem design. Its purpose is narrower and more practical: give the codebase a shared vocabulary for failure so that logs, tests, runtime handling, and crash reports all describe the same event in compatible terms.
