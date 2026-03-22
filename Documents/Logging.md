# Logging

## Overview

Life exposes engine-owned logging through `Life::Log` and `Life::LogSpecification`.

The goal is to keep logging:

- configurable through the engine API
- safe for concurrent use
- available early in application bootstrap
- independent from application code reaching directly into `spdlog` internals

## Configuration Surface

`ApplicationSpecification` now owns a `Logging` field of type `LogSpecification`.

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

This lets an application configure logging before the first engine lifecycle logs are emitted.

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

## Threading Characteristics

The logging layer is protected by an engine-owned mutex during configuration and lazy initialization.

This gives the following guarantees:

- concurrent calls to `Log::Init()` are safe
- concurrent first access through `GetCoreLogger()` / `GetClientLogger()` is safe
- reconfiguration through `Log::Configure(...)` is serialized
- the active logger instances use `spdlog` multi-threaded sink types

## Operational Notes

- Logger construction is lazy-safe; requesting a logger initializes the backend if necessary.
- If file logging is enabled, parent directories are created automatically.
- If both console and file sinks are disabled, the engine falls back to a console sink rather than leaving logging inert.
- Reconfiguration replaces the registered engine loggers with new logger instances built from the current specification.

## Usage Guidance

Use `ApplicationSpecification.Logging` as the main integration point.

Prefer this over ad hoc calls into `spdlog::set_pattern`, `spdlog::set_level`, or direct global registry mutation from random engine/application code.

## Caveat

The engine logging layer is thread-safe for logging and reconfiguration boundaries, but message ordering between threads is still naturally interleaved by concurrency. Thread-safe does not mean globally serialized semantic ordering.
