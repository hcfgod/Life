# Application Architecture

## Purpose

Life is organized around a small set of runtime types with deliberately explicit ownership. The engine does not treat startup, platform runtime, window lifetime, event routing, and application callbacks as interchangeable concerns. Each has a specific owner and a clear place in the application lifecycle.

This document describes the authoritative startup path and the roles of the core runtime objects.

## Canonical Startup Path

For a normal executable entrypoint, startup flows through the following path:

1. `main(...)` in `Core/EntryPoint.h`
2. `Life::RunApplicationMain(...)`
3. `CreateApplicationRunner(...)`
4. `CreateApplicationHost(...)`
5. `ApplicationHost` construction and shared-system setup
6. `ApplicationHost::Initialize()`
7. repeated runner iterations until shutdown
8. `ApplicationHost::Finalize()` and runner teardown

This matters because runtime behavior is defined by the runner and host, not by ad hoc calls into `Application`.

## High-Level Responsibilities

### `ApplicationRunner`

`ApplicationRunner` is the authoritative loop controller.

It is responsible for:

- installing early crash diagnostics before host creation
- building the runner state used by the normal loop and the external event-pump path
- draining queued events
- polling the platform runtime when external pumping is not enabled
- computing frame timestep values
- ensuring host finalization during teardown

If frame timing, loop structure, or cross-thread event injection behavior changes in the future, the runner path is the first place to inspect.

### `ApplicationHost`

`ApplicationHost` is the owner of process-facing engine state for a running application instance.

It owns:

- the `Application` instance
- the `ApplicationRuntime`
- the platform `Window`
- the bound `ApplicationContext`
- the `ApplicationEventRouter`
- the authoritative `ServiceRegistry`

The host also becomes the point where application specification values become operational. In practice that means logging is configured here, crash diagnostics are re-bound to application-specific settings here, and platform detection is initialized here before the window is created.

### `Application`

`Application` is the consumer-facing runtime object. It supplies the engine-facing lifecycle hooks and event overrides that application code implements.

The important boundary is that `Application` does not own the loop. It is driven by the host and runner through:

- `OnInit()`
- `OnUpdate(float timestep)`
- `OnEvent(Event&)`
- `OnShutdown()`
- `OnHostInitialize()` / `OnHostRunFrame()` / `OnHostFinalize()` as host-facing internal sequencing points

### `ApplicationRuntime`

`ApplicationRuntime` owns platform runtime behavior. In the current SDL-backed implementation, this includes process-wide runtime initialization and platform event polling.

That separation is intentional. Window objects are kept focused on window state and native handles rather than hiding process-wide runtime ownership.

### `Window`

`Window` owns platform window state and the native handle for the active application window.

The host creates the window after logging, crash diagnostics, and platform metadata have been brought up from the application specification.

## Ownership Model

The engine currently uses a host-owned model.

That means:

- one `ApplicationHost` owns one active `Application`
- the host owns and binds the runtime context
- the host constructs and exposes the service registry used by application-facing accessors
- the host is responsible for finalization and shared-system release

This model avoids ambiguous lifetime questions such as whether a global subsystem belongs to the application object, the window, or the entrypoint.

## Service Registry Model

The authoritative service container is `ServiceRegistry`.

During host construction, built-in engine services are registered and then pushed into the global registry stack. This makes the current host registry available through both explicit context access and the global `GetServices()` convenience function.

Built-in registrations currently include:

- `ApplicationHost`
- `Application`
- `ApplicationContext`
- `ApplicationEventRouter`
- `ApplicationRuntime`
- `Window`
- `JobSystem`
- `Async::AsyncIO`

Practical guidance:

- prefer `Application::GetService<T>()` or `ApplicationContext::GetService<T>()` when you already have an application/context reference
- use global `GetServices()` as a convenience layer, not as a substitute for clear ownership
- avoid assuming the fallback global registry contains meaningful services when no host is active

## Lifecycle Sequence

At a high level, the normal lifecycle is:

1. crash diagnostics are installed early in the runner
2. the application instance is created
3. the host configures logging from `ApplicationSpecification.Logging`
4. the host sets application-specific crash-diagnostics metadata and optional crash-reporting overrides
5. platform detection is initialized
6. the platform window is created
7. shared engine systems such as the job system and async I/O are acquired
8. services are registered and the context is bound
9. `ApplicationHost::Initialize()` marks the host running and invokes application initialization
10. each runner iteration dispatches queued events, polls runtime events if needed, and runs one frame update
11. shutdown clears the running state
12. finalization invokes host/application teardown and releases shared systems

## Event Flow in Context

Event dispatch is intentionally host-routed.

The runner and runtime collect events, but `ApplicationHost::HandleEvent(...)` delegates to `ApplicationEventRouter`, which then routes the event into the application.

Within the application-facing pipeline, the current ordering is:

1. `Application::OnEvent(...)`
2. event-bus subscribers
3. built-in engine handlers such as window-close shutdown

This ordering gives application code first inspection rights while still preserving built-in behavior when earlier stages do not mark the event handled.

## External Event Pump Mode

The runner also supports an external event-pump mode for SDL callback-style integration.

In that mode:

- foreign callbacks queue translated events into runner-owned pending storage
- queue access is guarded by `ApplicationRunnerState::EventMutex`
- queued events are dispatched during the same authoritative iteration path used by the normal loop
- runtime polling is skipped because the external source is already feeding events in

The design goal is one dispatch point, even when event production comes from a different integration style.

## Logging and Crash Diagnostics Boundaries

Two related systems are configured around the host boundary.

### Logging

`ApplicationSpecification.Logging` is the authoritative logging input surface. The host configures logging before window creation and before most lifecycle logs are emitted.

### Crash Diagnostics

Crash diagnostics are installed early in the runner so bootstrap failures can still be reported. Once the host has the application specification, crash diagnostics are updated with:

- the real application name
- the command line
- any non-default `CrashReportingSpecification`

This two-phase setup is deliberate. Early install protects startup, while host configuration makes reports application-aware.

## Design Rules for Future Changes

When extending the runtime architecture, preserve the following rules unless there is a strong reason to change them explicitly:

- keep one authoritative loop path
- keep host ownership of runtime-facing shared state
- keep platform runtime ownership separate from window ownership
- keep application callbacks consumer-facing rather than loop-owning
- keep service registration tied to host lifetime
- keep lifecycle sequencing explicit and testable

When a new system needs broad access, first decide whether it is truly host-owned, runtime-owned, or application-owned. The answer should determine where it is created and how it is exposed.
