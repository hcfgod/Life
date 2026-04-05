# Layers and Overlays

## Purpose

Life uses a host-owned layer model for application-facing update, render, and event participation.

The intent is to give runtime and tooling code a structured place to plug into the frame and event pipeline without turning `Application` into a monolith and without making every system a global singleton.

This document describes the current `Layer` and `LayerStack` model, the difference between regular layers and overlays, and the lifecycle and ordering guarantees the engine currently provides.

## Main API Surface

The current layer surface is centered on:

- `Layer`
- `LayerStack`
- `Application::PushLayer(...)`
- `Application::PushOverlay(...)`
- `Application::PopLayer(...)`
- `Application::PopOverlay(...)`
- `Application::GetLayerStack()`

## Ownership Model

`LayerStack` is host-owned.

During `ApplicationHost` construction, the host:

- constructs the authoritative `LayerStack`
- binds it to the active `Application`
- registers it in the host-owned `ServiceRegistry`
- routes update, render, and event work through it during the host-owned frame and event sequence

That means layers are part of the active application runtime, not ambient process state.

## `Layer`

`Layer` is the application-facing base class for update, render, and event participation.

The current callback surface is intentionally small:

- `OnAttach()`
- `OnDetach()`
- `OnUpdate(float timestep)`
- `OnRender()`
- `OnEvent(Event&)`

Each layer also exposes:

- a debug name
- enabled/disabled state
- attached state
- overlay state
- access to the bound `Application`
- access to the bound `Window`

A layer is considered valid for normal use only while attached. Accessing `GetApplication()` on a detached layer throws, which helps catch lifecycle misuse early.

## `LayerStack`

`LayerStack` is the ordered container that owns layer insertion, removal, and traversal order.

The current stack tracks:

- a bound `Application`
- a vector of `LayerRef`
- an insertion index that separates regular layers from overlays

This insertion index is the key detail behind the current model:

- regular layers are inserted before the overlay partition
- overlays are appended at the end of the stack
- the stack therefore preserves one ordered sequence while still maintaining a semantic split between gameplay/runtime layers and overlay-style UI or tooling layers

## Regular Layers vs Overlays

The distinction is about ordering and intent rather than about separate container types.

### Regular layers

Regular layers are intended for core runtime behavior such as:

- gameplay logic
- scene-facing simulation or rendering work
- application-specific runtime systems

They are inserted before overlays and therefore render earlier. In reverse-ordered event traversal, they are reached only after overlays have had first inspection.

### Overlays

Overlays are intended for cross-cutting UI and tooling surfaces such as:

- diagnostics overlays
- editor shell UI
- debug or developer-facing panels

They are appended after regular layers and therefore:

- render after normal layers in forward traversal
- receive events first in reverse traversal

That makes overlays a good fit for tooling that should visually sit on top of the main scene and intercept input before lower-level runtime layers react.

## Attachment and Detachment Semantics

Layer attachment is explicit and exception-aware.

When a layer is inserted:

1. the stack verifies it is bound to an application
2. null insertion is rejected
3. duplicate insertion of the same layer instance is rejected
4. the layer is bound to the application and marked as regular-layer or overlay
5. `OnAttach()` is invoked
6. the attach is rolled back if `OnAttach()` throws

When a layer is removed:

1. `OnDetach()` is invoked
2. the layer is unbound
3. the layer is erased from the stack
4. the stack logs whether a layer or overlay was removed

`LayerStack::Clear()` detaches in reverse order so teardown mirrors the natural top-down ownership view of the active stack.

## Update Order

`LayerStack::OnUpdate(...)` walks the stack in forward order.

Current behavior:

- disabled layers are skipped
- null entries are skipped
- regular layers update before overlays

This is the natural runtime ordering for gameplay-first work followed by overlay or tooling work that may depend on the results of the main update.

## Render Order

`LayerStack::OnRender()` also walks the stack in forward order.

Current behavior:

- disabled layers are skipped
- null entries are skipped
- regular layers render before overlays

That means overlays naturally draw after the main scene-facing layers, which is the right default for diagnostics panels and editor-style UI surfaces.

## Event Order

Layer event delivery is intentionally different from update and render ordering.

`LayerStack::OnEvent(...)` walks the stack in reverse order.

Current behavior:

- overlays receive events before regular layers
- disabled layers are skipped
- routing stops once `event.IsPropagationStopped()` becomes true

This is a common engine-style rule: visual topmost or tooling-oriented layers get first chance to consume input, while lower layers only react when earlier layers allow propagation to continue.

## Relationship to `ApplicationEventRouter`

`LayerStack` is not the full event pipeline by itself.

The current host-routed event order is:

1. `Application::OnEvent(...)`
2. `ImGuiSystem::CaptureEvent(...)` when present and available
3. `LayerStack::OnEvent(...)`
4. `EventBus` subscriber dispatch
5. built-in engine handlers

That placement is deliberate.

It means:

- application-level logic gets first inspection rights
- tooling capture can stop input before it reaches gameplay layers
- overlays and layers get a structured middle layer in the route
- lower-level fallback behavior such as shutdown remains available when earlier stages do not stop propagation

## Relationship to the Host Frame

Layers do not own the frame.

The host remains the lifecycle and frame authority.

In the current frame sequence:

1. input actions are updated
2. a graphics frame begins when graphics are available
3. an ImGui frame begins when available
4. `Application::OnUpdate(...)` runs through the host
5. `LayerStack::OnUpdate(...)` runs
6. `LayerStack::OnRender()` runs when a graphics frame is active
7. ImGui rendering runs when available
8. the graphics frame is presented

This keeps frame ownership explicit. Layers participate inside the host-managed frame window rather than creating or presenting frames themselves.

## Current Usage in the Repository

Two concrete examples show the current intended usage:

- `Runtime/Source/GameLayer.cpp` is a regular runtime layer that consumes input, manages cameras through host-owned services, and renders through `Renderer2D`
- `Runtime/Source/RuntimeDiagnosticsOverlay.cpp` and `Editor/Source/EditorShellOverlay.cpp` are overlay-style layers that behave like top-level diagnostics or editor UI surfaces

The editor overlay is especially illustrative because it uses the overlay model to host docking UI, stats panels, and a scene viewport while still participating in the same host-owned frame lifecycle as the runtime.

## Recommended Usage

For current application-facing code, the safest pattern is:

- use regular layers for main runtime logic and scene-facing behavior
- use overlays for diagnostics, tooling, and UI that should sit on top of normal layers
- keep long-lived systems host-owned and resolve them through `Application::GetService<T>()` instead of hiding ownership inside layers
- treat `OnRender()` as a host-framed callback rather than a place to begin or present frames manually
- stop propagation only when the layer truly intends to block lower layers or built-in behavior

## Design Rules

Future changes should preserve the following rules unless there is a strong reason to change them explicitly:

- keep `LayerStack` host-owned
- keep update and render traversal forward
- keep event traversal reverse-ordered
- keep overlays as an ordering concept rather than a second disconnected system
- keep attach/detach sequencing exception-aware
- keep layer code consumer-facing rather than lifecycle-owning

If a future scene or editor system becomes more complex, it should still plug into the existing host/layer model rather than bypassing it with a second ad hoc runtime loop.
