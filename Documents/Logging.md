# Logging

## Purpose

Life exposes engine-owned logging through `Life::Log` and `LogSpecification`.

The intent is to provide a single logging surface that is:

- configurable through engine-facing APIs
- safe to initialize and reconfigure from concurrent code paths
- available early in application startup
- independent from consumers mutating `spdlog` globals directly

## Main API Surface

The logging API is intentionally small.

The main entry points are:

- `Log::Init()`
- `Log::Configure(...)`
- `Log::GetSpecification()`
- `Log::GetCoreLogger()`
- `Log::GetClientLogger()`

Most application code should not need to interact with raw logger instances directly. In normal use, the convenience macros are the primary surface:

- `LOG_CORE_TRACE`, `LOG_CORE_INFO`, `LOG_CORE_WARN`, `LOG_CORE_ERROR`, `LOG_CORE_CRITICAL`
- `LOG_TRACE`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR`, `LOG_CRITICAL`

## Configuration Model

`ApplicationSpecification` owns a `Logging` field of type `LogSpecification`.

That makes logging part of the normal application configuration path rather than a separate global setup phase.

Life currently supports one live `ApplicationHost` per process. The logger set is therefore treated as a process-level facility whose authoritative configuration is applied by the active host.

Important fields include:

- `CoreLoggerName`
- `ClientLoggerName`
- `Pattern`
- `CoreLevel`
- `ClientLevel`
- `FlushLevel`
- `EnableConsole`
- `EnableFile`
- `FilePath`
- `MaxFileSize`
- `MaxFileCount`

`ApplicationHost` applies this specification during construction, before window creation and before most lifecycle logging begins. That is the authoritative configuration point for a running application.

## Example

```cpp
Life::ApplicationSpecification specification;
specification.Name = "Runtime";
specification.Logging.Pattern = "%^[%Y-%m-%d %T.%e] [thread %t] %n: %v%$";
specification.Logging.CoreLevel = spdlog::level::info;
specification.Logging.ClientLevel = spdlog::level::trace;
specification.Logging.EnableFile = true;
specification.Logging.FilePath = "logs/runtime.log";
```

## Logger Construction Model

The logging layer builds two logger instances:

- a core logger for engine messages
- a client logger for application-facing messages

Both logger instances are constructed from the same sink set derived from the active `LogSpecification`.

In the current implementation:

- console logging uses `spdlog::sinks::stdout_color_sink_mt`
- file logging uses `spdlog::sinks::rotating_file_sink_mt`
- each sink receives the configured pattern
- each logger receives its own level and flush threshold

This means the engine and client loggers can differ in name and severity level while still writing to the same destinations.

## Lazy Initialization

The logging system supports explicit initialization through `Log::Init()`, but it is also lazy-safe.

If code requests `GetCoreLogger()` or `GetClientLogger()` before explicit initialization, the logging layer initializes itself using the current stored specification.

That behavior is important because it allows early code paths, assertions, and bootstrap diagnostics to obtain a logger without requiring every caller to coordinate a separate setup step.

## Thread-Safety Characteristics

The current implementation protects configuration and first-time initialization with an engine-owned mutex and stores active logger instances through shared-pointer storage.

This gives the following guarantees:

- concurrent calls to `Log::Init()` are safe
- concurrent first access through `GetCoreLogger()` and `GetClientLogger()` is safe
- reconfiguration through `Log::Configure(...)` is serialized
- active sink types are the multi-threaded `spdlog` variants

This is a boundary guarantee, not a promise of globally ordered logging across threads.

## Sink and File Behavior

Several runtime behaviors are worth calling out because they affect operations and debugging.

### Console Output

If `EnableConsole` is true, the logger set includes a color console sink.

### File Output

If `EnableFile` is true, the logger set includes a rotating file sink.

Current behavior:

- `FilePath` must be non-empty when file logging is enabled
- parent directories are created automatically when needed
- file rotation uses `MaxFileSize` and `MaxFileCount`

If file logging is enabled and no file path is provided, logger construction throws.

### No-Sink Fallback

If both console and file output are disabled, the engine falls back to a console sink instead of creating inert loggers.

That keeps the logging system operational even under a misconfigured specification.

## Reconfiguration Behavior

`Log::Configure(...)` rebuilds the logger pair from the supplied specification and replaces the active logger instances.

This is not an in-place mutation of existing logger objects. Reconfiguration creates a fresh sink/logger set and then publishes the new loggers as the active instances.

In practice, that means:

- the current specification becomes the new authoritative configuration
- future logger retrievals observe the new logger pair
- crash diagnostics and other systems that query the current specification see the updated values

Reconfiguration is effectively transactional at the publication boundary. If construction of the replacement logger set throws, the previously active loggers and stored specification remain in place.

This matters most for file-output failures. For example, enabling file logging with an empty `FilePath` fails during replacement logger construction rather than partially mutating the active logging state.

Under the single-host runtime model, the active host is the normal authority for this configuration. Code should not assume logger configuration stacks or automatically restores across nested hosts.

## Relationship to Crash Diagnostics

Crash diagnostics use the logging layer in two ways:

- they attempt to flush both core and client loggers before writing a crash report
- they include selected logging configuration details in the generated report

That coupling is intentional. It improves the odds that the final log messages around a failure are preserved and makes the crash artifact more useful during investigation.

## Recommended Usage

Use `ApplicationSpecification.Logging` as the main integration point.

Recommended practice:

- configure logging through the application specification rather than ad hoc runtime mutation
- use the logging macros for normal call sites
- treat raw logger access as an escape hatch for advanced formatting or sink-aware behavior
- avoid relying on direct `spdlog` global registry manipulation from random engine or application code

## Caveat on Ordering

The logging layer is thread-safe for initialization, retrieval, and reconfiguration boundaries, but message order between threads remains naturally interleaved under concurrency.

Thread-safe logging should not be interpreted as semantically serialized logging.
