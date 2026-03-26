# Entry Points and Bootstrap

## Purpose

Life supports two entry integration styles:

- a normal executable `main(...)` path through `Core/EntryPoint.h`
- an SDL callback path through `Core/SDLEntryPoint.h`

Both paths are intentionally routed into the same `ApplicationRunner` state and iteration helpers. The engine does not maintain separate loop implementations for executable startup versus SDL callback integration.

This document describes the bootstrap responsibilities, exception boundaries, and teardown behavior of those entry surfaces.

## Main API Surface

The current bootstrap and entry surface is centered on the following functions and types:

- `PrepareApplicationBootstrapDiagnostics(...)`
- `CreateApplicationRunner(...)`
- `RunApplicationRunnerIteration(...)`
- `QueueApplicationEvent(...)`
- `DestroyApplicationRunner(...)`
- `RunApplicationMain(...)`
- `HandleApplicationBootstrapException(...)`
- `HandleApplicationRuntimeException(...)`
- `HandleSDLApplicationBootstrapException(...)`
- `HandleSDLApplicationRuntimeException(...)`
- `ApplicationRunnerState`

## Normal Executable Entry Path

`Core/EntryPoint.h` provides the standard executable bootstrap path.

Current flow:

1. `main(...)` calls `SDL_SetMainReady()`
2. `main(...)` calls `Life::RunApplicationMain(...)`
3. `RunApplicationMain(...)` prepares bootstrap diagnostics
4. `CreateApplicationRunner(...)` creates the host, initializes it, and returns runner state
5. repeated calls to `RunApplicationRunnerIteration(...)` drive the application loop
6. `DestroyApplicationRunner(...)` finalizes the host and destroys runner state

The executable entry path is the canonical integration surface for a normal desktop application executable.

## SDL Callback Entry Path

`Core/SDLEntryPoint.h` provides the SDL callback integration path.

Current flow:

1. `SDL_AppInit(...)` prepares bootstrap diagnostics
2. `SDL_AppInit(...)` creates an `ApplicationRunnerState` with external event pumping enabled
3. `SDL_AppEvent(...)` translates `SDL_Event` values into engine events and queues them through `QueueApplicationEvent(...)`
4. `SDL_AppIterate(...)` calls `RunApplicationRunnerIteration(...)`
5. `SDL_AppQuit(...)` destroys the runner state through `DestroyApplicationRunner(...)`

The important design point is that SDL callback integration changes event production, not loop authority. Iteration, event dispatch, timestep computation, and host lifecycle sequencing still flow through the same runner logic used by the executable path.

## Bootstrap Diagnostics

`PrepareApplicationBootstrapDiagnostics(...)` exists so failures that occur before `ApplicationHost` construction can still produce useful diagnostics.

That helper currently:

- installs crash diagnostics at the process level
- records a bootstrap application name
- captures the raw command line as a string vector

At this stage the engine does not yet have the real `ApplicationSpecification`, so the metadata is intentionally generic. `ApplicationHost` later rebinds crash diagnostics using the authoritative application name and crash-reporting policy.

## ApplicationRunnerState

`ApplicationRunnerState` is the concrete state object shared by both entry styles.

It currently contains:

- the live `ApplicationHost`
- the last frame timestamp used for timestep computation
- a mutex protecting pending externally queued events
- a pending event vector
- a flag indicating whether external event pumping is enabled

This state object is intentionally small. It exists to hold loop-owned state that must survive across callback boundaries or repeated loop iterations.

## Iteration Semantics

`RunApplicationRunnerIteration(...)` is the authoritative per-frame loop helper.

Current behavior is:

1. swap pending externally queued events out of runner-owned storage
2. dispatch queued events through `ApplicationHost::HandleEvent(...)`
3. if external pumping is disabled, poll platform runtime events through `ApplicationRuntime::PollEvent()`
4. stop early if the host is no longer running
5. compute frame timestep from the previous iteration timestamp
6. run one frame through `ApplicationHost::RunFrame(...)`

Both queued events and polled runtime events converge on the same host-routed event path.

## External Event Injection

`QueueApplicationEvent(...)` is the supported cross-boundary event injection surface for runner-owned state.

Current guarantees are intentionally narrow:

- queue insertion is mutex-protected
- null runner state or null events are ignored
- events are not dispatched immediately on the producer side
- actual dispatch still occurs during runner iteration on the authoritative loop thread

This keeps event delivery deterministic even when production originates outside the normal runtime poll loop.

## Exception Boundaries and Reporting Phases

The entry layer distinguishes bootstrap failures from runtime-loop failures.

### Executable path

The executable path currently reports handled exceptions with these phases:

- `RunApplicationMain` for bootstrap failures caught outside the loop
- `RunApplicationLoop` for runtime-loop failures caught during iteration

### SDL callback path

The SDL callback path currently reports handled exceptions with these phases:

- `SDL_AppInit` for callback bootstrap failures
- `SDL_AppIterate` for iteration-time failures
- `SDL_AppEvent` for event-translation or queueing failures during callback delivery

This phase distinction is intentional. It makes reports easier to interpret because a startup failure, a steady-state runtime failure, and an SDL callback failure do not all collapse into the same generic label.

## Teardown Behavior

`DestroyApplicationRunner(...)` is the normal teardown path for runner-owned state.

Current behavior is:

- finalize the host if runner state exists
- report and suppress teardown exceptions at the runner boundary
- release the host
- delete runner state
- null the caller's state pointer

There is also a convenience `RunApplication(ApplicationHost&)` helper for code that already owns a host directly. It follows the same high-level lifecycle pattern: initialize the host, iterate until shutdown, and ensure finalization during teardown.

## Design Rules

Future changes to entry and bootstrap code should preserve the following rules:

- keep one authoritative runner iteration path
- keep bootstrap diagnostics earlier than host creation
- keep executable and SDL callback integrations converged on shared runner helpers
- keep host finalization explicit at the runner boundary
- keep exception phase names specific enough to explain where a failure occurred

If a new bootstrap mode is added later, it should integrate into `ApplicationRunner` rather than introducing a second independent loop implementation.
