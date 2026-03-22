# Event and Threading Invariants

## Why This Exists

Life now has explicit runtime ownership and a canonical application loop path. These invariants matter because windowing, SDL event pumping, and lifecycle shutdown all depend on predictable ordering.

## Core Runtime Ownership

`ApplicationRuntime` owns platform runtime responsibilities.

Current SDL-backed behavior is:

- the runtime owns SDL subsystem lifetime
- the runtime owns platform event polling
- `Window` owns native window state and handle lifetime, not global SDL lifetime

This separation keeps the window abstraction focused and avoids hiding process-wide runtime state inside a window object.

## Canonical Loop Authority

`ApplicationRunner` is the authoritative loop path.

That means:

- normal app execution flows through `RunApplicationMain(...)`
- SDL callback execution flows through the same runner state and iteration helpers
- `Application::Startup()` is only a thin convenience entrypoint over the same loop behavior

Any future changes to frame timing, queued events, or shutdown semantics should be made in the runner path first.

## Event Ordering

`Application::HandleEvent(...)` currently processes events in this order:

1. `OnEvent(...)`
2. `EventBus` subscribers
3. built-in engine handlers such as window-close shutdown

This ordering is intentional.

Implications:

- application overrides get first chance to inspect or mark an event handled
- subscribed systems observe the post-`OnEvent` event state
- built-in shutdown only occurs if earlier stages did not already mark the event handled

## External Event Pump Path

When using the SDL callback entrypoint / external event pump mode:

- foreign thread/event callbacks queue translated events into runner-owned pending storage
- queue access is protected by `ApplicationRunnerState::EventMutex`
- queued events are dispatched on the application iteration path
- runtime polling is skipped when `UseExternalEventPump` is enabled

This preserves a single dispatch point even when events originate outside the normal polling loop.

## Main-Thread Assumptions

The current engine should be treated as main-thread-oriented for lifecycle and event consumption.

Practical rules:

- create and run the application on the main thread
- treat runtime polling and event dispatch as main-thread work
- do not call `Application::HandleEvent(...)` concurrently from arbitrary worker threads
- if an external source needs to inject events, queue them through the runner path rather than mutating application state directly

These rules align with SDL's main-thread/event-thread expectations, especially on macOS.

## What Is Thread-Safe Today

- runner pending-event queue insertion via `QueueApplicationEvent(...)`
- pending-event extraction during runner iteration
- logging initialization/configuration boundaries in `Life::Log`

## What Is Not Promised Thread-Safe

The engine does not currently promise that the following are safe for arbitrary concurrent access:

- `Application` lifecycle methods
- direct event dispatch into `Application::HandleEvent(...)`
- event subscriber registration/unregistration while another thread is simultaneously dispatching through the same application-owned event bus
- window/runtime manipulation from worker threads

## Recommended Future Direction

If the engine grows a larger multithreaded job/runtime model, keep these invariants explicit:

- main-thread ownership for platform and windowing concerns
- queue-based cross-thread handoff for engine events and commands
- one authoritative application iteration point that drains cross-thread work deterministically
