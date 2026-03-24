# Event and Threading Invariants

## Purpose

Life has a canonical application loop, a host-owned runtime model, and a single event-routing path for normal application execution.

These invariants matter because windowing, SDL event pumping, frame iteration, and shutdown behavior all depend on predictable sequencing. If event and threading assumptions drift, the engine becomes harder to reason about and significantly easier to break during refactors.

## Canonical Loop Authority

`ApplicationRunner` is the authoritative loop path.

In the normal executable path, control flows through:

1. `main(...)` in `Core/EntryPoint.h`
2. `RunApplicationMain(...)`
3. `CreateApplicationRunner(...)`
4. `CreateApplicationHost(...)`
5. repeated calls to `RunApplicationRunnerIteration(...)`

In the SDL callback path, control still flows through the same runner state and iteration helpers via `SDLEntryPoint.h`.

This means the runner remains the source of truth for:

- queued event dispatch
- runtime polling
- timestep computation
- iteration shutdown behavior
- loop teardown

Any future changes to frame timing, queued events, or shutdown semantics should be made in the runner path first.

## Runtime and Window Ownership

`ApplicationRuntime` owns platform runtime responsibilities.

In the current SDL-backed model:

- the runtime owns platform event polling
- the runtime owns process-level platform runtime behavior
- `Window` owns native window state and handle lifetime
- `Window` does not own global SDL lifetime

This separation keeps the window abstraction focused and avoids hiding process-wide runtime state inside a window object.

## Event Routing Authority

Event routing is host-driven.

At runtime:

- the runner or external callback path obtains or queues events
- `ApplicationHost::HandleEvent(...)` forwards the event to `ApplicationEventRouter`
- `ApplicationEventRouter::Route(...)` performs the actual application-facing dispatch sequence

That makes `ApplicationEventRouter` the authoritative ordering point for application event delivery.

## Event Ordering

`ApplicationEventRouter::Route(...)` currently processes an event in this order:

1. `application.OnEvent(event)`
2. `EventBus` subscriber dispatch
3. built-in engine handlers such as `WindowCloseEvent` shutdown

The built-in window-close handler only requests shutdown if the event has not already been marked handled.

This ordering is intentional.

Practical implications:

- application overrides get first chance to inspect or mark an event handled
- subscribed systems observe the event after application-level inspection
- built-in shutdown remains available as a fallback rather than pre-empting application logic

## Application Lifecycle Boundary

`Application` is not the loop owner.

The public methods:

- `Initialize()`
- `RunFrame(float timestep)`
- `HandleEvent(Event&)`
- `Shutdown()`
- `Finalize()`

are context- or host-routed surfaces. The actual lifecycle state is host-owned and context-bound.

In practice:

- initialization is driven by `ApplicationHost::Initialize()`
- frame execution is driven by runner iteration through the host
- shutdown is requested through the context/host path
- finalization is driven by host teardown

This boundary is important because it prevents application code from becoming an accidental second loop owner.

## Normal Polling Path

When external pumping is not enabled, runner iteration behaves as follows:

1. dispatch any pending queued events
2. poll runtime events from `ApplicationRuntime`
3. stop early if shutdown was requested
4. compute frame timestep
5. invoke one frame through the host

The important invariant is that queued events and polled runtime events both converge on the same host-routed dispatch path.

## External Event Pump Path

When using the SDL callback entrypoint in `SDLEntryPoint.h`, event production changes but event dispatch authority does not.

Current behavior is:

- `SDL_AppInit(...)` creates an `ApplicationRunnerState` with `UseExternalEventPump` enabled
- `SDL_AppEvent(...)` translates `SDL_Event` into an engine event and queues it through `QueueApplicationEvent(...)`
- queue insertion is protected by `ApplicationRunnerState::EventMutex`
- `SDL_AppIterate(...)` runs the same `RunApplicationRunnerIteration(...)` used by the normal runner path
- runtime polling is skipped when external pumping is enabled

This preserves one dispatch point even when event production originates outside the normal runtime poll loop.

## Main-Thread Assumptions

The current engine should be treated as main-thread-oriented for lifecycle and event consumption.

Practical rules:

- create and run the application on the main thread
- treat runtime polling and event dispatch as main-thread work
- treat host initialization and finalization as main-thread operations
- do not call `Application::HandleEvent(...)` concurrently from arbitrary worker threads
- if an external source needs to inject events, queue them through the runner path rather than mutating application state directly

These rules align with SDL's platform expectations, especially on macOS.

## What Is Thread-Safe Today

The engine currently provides explicit safety only for a limited set of boundaries.

Those include:

- runner pending-event queue insertion via `QueueApplicationEvent(...)`
- pending-event extraction and swap during runner iteration
- logging initialization and reconfiguration boundaries in `Life::Log`
- mutex-protected service lookup and registration operations inside `ServiceRegistry`

This is useful, but it should not be mistaken for broad application-wide concurrency support.

## What Is Not Promised Thread-Safe

The engine does not currently promise that the following are safe for arbitrary concurrent access:

- `Application` lifecycle methods
- direct event dispatch into `Application::HandleEvent(...)`
- event subscriber registration or unregistration while another thread is dispatching through the same application-owned event bus
- window or runtime manipulation from worker threads
- assuming shutdown/finalization can race freely with event delivery

If code needs to cross thread boundaries, the intended model is queue-based handoff into the runner, not direct cross-thread mutation of application state.

## Shutdown Invariant

Shutdown is stateful, not exceptional.

Today, the built-in `WindowCloseEvent` path requests shutdown through the application/host boundary. Once the host is no longer running, the runner stops dispatching further events and stops frame iteration.

That means shutdown behavior depends on the same authoritative loop state used for normal execution rather than a special side channel.

## Guidance for Future Multithreading Work

If the engine grows a broader multithreaded job or task model, these invariants should remain explicit:

- main-thread ownership for platform and windowing concerns
- queue-based cross-thread handoff for engine events and commands
- one authoritative application iteration point that drains cross-thread work deterministically
- one explicit routing order for application, subscriber, and built-in handlers

The safest way to evolve the runtime is to add more structured handoff into the runner, not to let unrelated threads directly participate in application lifecycle sequencing.
