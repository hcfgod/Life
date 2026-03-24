# Life Engine Documents

This folder contains implementation-facing documentation for the engine's runtime architecture, operational behavior, and integration surfaces.

The intent is to explain how the current systems fit together and where application or engine code is expected to plug in, without forcing readers to reconstruct design intent from headers alone.

## Recommended Reading Order

- `ApplicationArchitecture.md` for the canonical startup path, ownership model, and service boundaries.
- `EventThreadingInvariants.md` for the event pipeline and thread-safety assumptions that shape runtime behavior.
- `Logging.md` for the authoritative logging configuration model.
- `CrashDiagnostics.md` for crash-reporting lifecycle, output, and operational guidance.
- `ErrorHandling.md` for the structured error model, result types, and error utilities.
- `PlatformSupport.md` for host and target platform expectations.

## Documents

- `ApplicationArchitecture.md` - startup flow, ownership boundaries, service registry behavior, and the authoritative application loop.
- `EventThreadingInvariants.md` - event ordering, runtime ownership, and thread-safety boundaries for the current engine architecture.
- `Logging.md` - engine logging configuration, sink behavior, reconfiguration, and integration guidance.
- `CrashDiagnostics.md` - crash-reporting configuration, install timing, report contents, and platform-specific behavior.
- `ErrorHandling.md` - the engine error model, `Result<T>` conventions, assertions, verification, and system error mapping.
- `PlatformSupport.md` - supported host and target platforms, Windows/Linux/macOS architecture notes, and CI/build expectations.
