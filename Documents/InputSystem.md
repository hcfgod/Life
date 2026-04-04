# Input System

## Purpose

Life's input system is designed around a host-owned runtime service rather than a process-global singleton.

That choice is intentional. Input state is part of the active application runtime, not ambient process state. The engine therefore creates and owns `InputSystem` inside `ApplicationHost`, registers it in the host-owned `ServiceRegistry`, and exposes it through the same service-access patterns used by the rest of the application architecture.

The result is a system that supports both low-level device queries and higher-level action-based input, while remaining consistent with the engine's ownership and lifecycle rules.

## Ownership and Access

`ApplicationHost` owns the authoritative `InputSystem` instance for the active application.

During host setup, the engine:

- constructs `InputSystem`
- registers it in the host-owned `ServiceRegistry`
- synchronizes already-connected gamepads
- makes it available to application and layer code through owner-bound service access

For application-facing code, the preferred access pattern is:

- `Application::GetService<InputSystem>()`
- `Application::TryGetService<InputSystem>()`

That keeps input usage tied to the active application instance rather than to a hidden global dependency.

A concrete usage example exists in `Runtime/Source/GameLayer.cpp`, where the runtime layer queries the bound application's `InputSystem` service and reads named gameplay actions such as `Quit` and `Move`.

## Two Layers of Input

The current system exposes two complementary ways to consume input.

### Raw device state

`InputSystem` tracks direct device state for:

- keyboard keys
- mouse buttons
- mouse position and delta
- mouse wheel delta
- up to four gamepads
- per-gamepad buttons and axes

This layer is useful for systems that need exact device-level data or that are building higher-order behavior on top of input.

### Action-based input

On top of raw state, the engine supports named input actions through:

- `InputAction`
- `InputActionMap`
- `InputActionAsset`

This layer gives application code a more stable gameplay-facing vocabulary.

Instead of asking whether a particular key or button is pressed, code can ask whether `Gameplay/Move`, `Gameplay/Look`, or `Gameplay/Quit` is active.

That separation is important for maintainability. Raw device bindings can change without forcing gameplay code to be rewritten around specific hardware inputs.

## Event Ingestion Path

The input system is fed directly from SDL events.

In the current SDL window path, the platform event layer forwards each `SDL_Event` to `InputSystem::OnSdlEvent(...)` before the event is translated into the engine's higher-level event type.

That means the engine effectively maintains two related views of input:

- a stateful device/action model inside `InputSystem`
- a translated event stream inside the normal event-routing pipeline

These serve different purposes and are expected to coexist.

The input system handles the following SDL event families directly:

- key down and key up
- mouse motion
- mouse button down and up
- mouse wheel
- gamepad added and removed
- gamepad axis motion
- gamepad button down and up

If a rebinding session is active and consumes an event, that event is handled by the rebinding flow before normal input-state updates occur.

## Frame Lifecycle

Input is frame-based.

The current host frame sequence is:

1. SDL events update raw input state as they arrive
2. `ApplicationHost::RunFrame(...)` calls `InputSystem::UpdateActions()` at the start of the frame
3. application update and layer update consume action and raw input state
4. `InputSystem::EndFrame()` clears per-frame transitional state at frame end

This frame boundary matters because several parts of the API are intentionally transient.

Examples include:

- `WasKeyPressedThisFrame(...)`
- `WasKeyReleasedThisFrame(...)`
- `WasMouseButtonPressedThisFrame(...)`
- `WasGamepadButtonPressedThisFrame(...)`
- action phase queries such as `WasActionStartedThisFrame(...)`

These values are only meaningful within the current frame.

## Raw State Model

The raw-state API distinguishes between persistent state and transitional state.

### Persistent state

Persistent state answers questions such as:

- is this key currently down
- is this mouse button currently down
- is this gamepad connected
- what is the current gamepad axis value
- what is the current mouse position

### Transitional state

Transitional state answers questions such as:

- was this key pressed this frame
- was this button released this frame
- what mouse delta accumulated this frame
- what wheel delta accumulated this frame

The engine clears transitional state in `EndFrame()` so that frame-sensitive gameplay logic can reliably detect edges rather than continuously-held inputs.

## Action Model

The action system is built from three layers.

### `InputAction`

An `InputAction` has:

- a name
- a value type
- a set of bindings
- an enabled state
- a current phase
- a current value

Supported value types are:

- `Button`
- `Axis1D`
- `Axis2D`

Each action evaluates its bindings against the current `InputSystem` state and produces both a value and a phase.

### `InputActionMap`

An `InputActionMap` groups related actions under a named domain such as `Gameplay`.

Maps can be enabled or disabled as a unit. When a map is disabled, its actions stop updating in normal gameplay flow.

This is the right level for coarse-grained mode control such as gameplay versus UI input, or in-game controls versus menu controls.

### `InputActionAsset`

An `InputActionAsset` owns the full collection of maps used by the current input configuration.

The asset is the unit that gets assigned to `InputSystem` and the unit that is serialized to or loaded from disk.

## Action Phases

Action phases provide a stable way to reason about edges and sustained activity.

The current phases are:

- `Disabled`
- `Waiting`
- `Started`
- `Performed`
- `Canceled`

The transition model is straightforward:

- `Started` means the action was not actuated last frame and is actuated now
- `Performed` means the action was actuated last frame and remains actuated now
- `Canceled` means the action was actuated last frame and is no longer actuated now
- `Waiting` means the action is inactive
- `Disabled` means the action itself is disabled

This gives gameplay code a clear distinction between a press edge, a held state, and a release edge.

## Supported Binding Types

The current binding model covers common keyboard, mouse, and gamepad cases.

Supported bindings include:

- `KeyboardButtonBinding`
- `MouseButtonBinding`
- `KeyboardAxis1DBinding`
- `KeyboardAxis2DBinding`
- `MouseDeltaBinding`
- `GamepadButtonBinding`
- `GamepadAxis1DBinding`
- `GamepadAxis2DBinding`

A few practical details matter:

- axis bindings support scale values
- gamepad axis bindings apply deadzones
- 2D gamepad bindings can optionally invert Y
- bindings for a single action are combined into one evaluated action value

The current implementation is designed to cover the foundational gameplay cases first rather than expose an overly abstract input grammar too early.

## Project Asset and Override Stack

`InputSystem` supports two levels of action-asset selection.

### Project action asset

The project action asset is the default asset assigned to the system through:

- `SetProjectActionAsset(...)`
- `LoadProjectActionAssetFromFile(...)`

This is the normal baseline configuration for an application.

### Override stack

The input system also supports an action-asset override stack through:

- `PushActionAssetOverride(...)`
- `PopActionAssetOverride()`
- `PopActionAssetOverride(expectedTop)`

When overrides are present, the topmost override becomes the active action asset.

This is useful for temporary mode-specific remapping, modal UI flows, or test scenarios that need to replace the active input surface without mutating the project asset permanently.

The stack model is preferable to a single mutable override because it keeps temporary ownership explicit and naturally nested.

## Rebinding

Runtime rebinding is handled by `InputRebinding`.

A rebinding session is described by a request containing:

- the target asset or asset path
- the target map name
- the target action name
- the binding index to replace
- an optional device filter
- whether the result should be saved to disk

While a session is active, `InputSystem::OnSdlEvent(...)` gives the rebinding object first chance to consume incoming SDL events.

The current rebinding flow supports capture from:

- keyboard keys
- mouse buttons
- gamepad buttons
- gamepad sticks
- gamepad triggers

If rebinding succeeds, the target binding is updated and may be written back to disk.

If the write to the original asset path fails, the current implementation can fall back to a user-data override location under `InputActionsOverrides` when a user-data path is available.

That fallback is a practical engine-style behavior. It preserves the distinction between shipped defaults and user-specific overrides without making rebinding fail purely because the source asset location is read-only.

## Serialization Format

`InputActionAssetSerializer` is responsible for loading and saving action assets.

The serialized format is JSON and is centered around a top-level `maps` array.

At a high level, the file stores:

- map names and enabled state
- action names and value types
- binding records with binding-specific fields

The serializer accepts both human-readable names and numeric fallback identifiers for several binding types. That is useful for durability across environments while still keeping files reasonably understandable during debugging.

Error reporting uses the engine's normal `Result<T>` and `Life::Error` model, so malformed files, missing files, and unsupported configuration data can be surfaced with structured diagnostics.

## Mouse Warp Suppression

The input system includes explicit handling for synthetic mouse motion generated by mouse warping.

`NotifyMouseWarped()` increments an internal suppression counter so that the next synthetic motion events can update absolute mouse position without being misinterpreted as real gameplay look deltas.

This matters for relative-look and cursor-management workflows, where the engine may need to reposition the cursor while avoiding artificial camera input.

## Gamepad Model

The current system supports up to four gamepads.

At startup, `SyncConnectedGamepads()` queries SDL for already-attached devices so the runtime does not depend exclusively on hot-plug events that occur after initialization.

Per-gamepad state includes:

- device identity
- connection status
- button state
- button transition flags
- axis values

Gameplay-facing code can either read raw gamepad state directly or consume gamepad-backed actions through the action system.

## Relationship to the Event System

It is important not to conflate the input system with the event system.

The input system is stateful and frame-oriented.

The event system is message-oriented and routed through `ApplicationEventRouter`, which currently dispatches events in this order:

1. `Application::OnEvent(...)`
2. `LayerStack`
3. `EventBus`
4. built-in engine handlers

Use the input system when you need stable polling semantics, named gameplay actions, or device state.

Use the event system when you need immediate message-style reactions to discrete events or when you are writing systems that are naturally event-driven.

Many real systems will use both.

## Recommended Usage

For gameplay and layer code, the current best practice is:

- define named actions in an `InputActionAsset`
- assign the asset through the host-owned `InputSystem`
- read actions from `OnUpdate(...)` rather than scattering raw device checks everywhere
- use raw device queries only where device-specific behavior genuinely matters
- treat per-frame transition queries as frame-local state
- keep temporary remapping isolated through the override stack or rebinding APIs rather than by mutating unrelated gameplay code

This keeps gameplay systems readable and keeps the binding layer flexible.

## Guidance for Future Expansion

As the input stack evolves, the current architectural rules are worth preserving:

- keep `InputSystem` host-owned
- keep application-facing access owner-bound through `Application`
- keep raw state and action state in the same authoritative runtime service
- keep SDL ingestion centralized rather than letting subsystems read native events directly
- keep action assets as the primary configuration surface for gameplay input

That approach matches how a professional engine typically scales input: first establish clean ownership, deterministic frame semantics, and a stable action vocabulary, then build editor tooling, presets, device profiles, and higher-level input workflows on top of that foundation.
