# Editor and Tooling

## Purpose

Life now includes a dedicated `Editor` application target and a host-owned tooling path built around `ImGuiSystem`.

This is no longer just a runtime sandbox with debug UI bolted on. The repository now has a separate editor executable, dedicated editor overlay code, docking-based UI layout, and an offscreen scene-surface path that renders engine content into an editor panel.

This document explains how the current editor/tooling path fits into the broader engine architecture.

## Current High-Level Shape

The current tooling stack has five main pieces:

- the `Editor` application target
- the host-owned `ImGuiSystem`
- the project-first editor flow built around `ProjectService`
- the layer/overlay model used to host editor UI
- the offscreen scene-surface path used by the editor shell

The design goal is to keep tooling inside the same authoritative host, service, frame, and event model used by the runtime rather than introducing a second ad hoc application architecture just for the editor.

## Editor Target

`Editor` is now a dedicated application target alongside `Engine`, `Runtime`, and `Test`.

The editor entry path is straightforward:

- `Editor/Source/EditorApp.cpp` defines an `EditorApplication` derived from `Application`
- the editor creates an `InputActionAsset` for editor-facing actions such as `Quit`
- the editor pushes `EditorShellOverlay` as its main overlay surface
- the same entrypoint and host bootstrap model used by the runtime remains authoritative

That means the editor is a real consumer of the engine API, not a special case that bypasses the public architecture.

## Project Hub and Workspace Flow

The editor no longer assumes it should drop directly into a workspace every time.

`EditorShellOverlay` now has two explicit modes:

- `ProjectHub`
- `Workspace`

On attach, the overlay resolves editor services once, initializes the project hub UI state, attaches the scene viewport panel, and then chooses its starting mode based on whether `ProjectService` already has an active project.

### Project hub mode

`Editor/Source/ProjectHub/EditorProjectHub.cpp` is the current project-entry surface.

Current behavior includes:

- creating new projects through `ProjectService::CreateProject(...)`
- opening existing projects through `ProjectService::OpenProject(...)`
- tracking a recent-project list in a user-data JSON file
- accepting either a direct `.lifeproject` path or a project folder path that can be resolved to one
- deleting a recent project entry or deleting the underlying project files from disk

The project hub is intentionally application-facing rather than bootstrap-only. It uses the same host-owned project service as the rest of the engine.

### Workspace mode

Once a project is opened, the overlay switches into `Workspace` mode.

At that point the editor shell becomes the main UI surface and the overlay ensures scene state is coherent with the active project:

- if the active project descriptor specifies `Startup.Scene`, the editor opens it through `SceneService`
- otherwise the editor creates a fresh scene document for the workspace
- scene create, open, save, save-as, and close operations are handled through `SceneService`
- when scene operations succeed, the editor updates the active project's `Startup.Scene` field and saves the project descriptor

## `ImGuiSystem`

`ImGuiSystem` is a host-owned service created by `ApplicationHost` during setup.

Important current properties:

- the service is always constructed and registered
- initialization only succeeds when Dear ImGui is present in the build, the window exposes an SDL handle, and the active graphics backend has support
- the current working renderer backend is Vulkan
- D3D12 remains an intentional future seam rather than a complete tooling backend today

This lets the rest of the engine treat tooling availability as optional without making the service itself disappear from the host service model.

## SDL and Engine Event Integration

Tooling participates in two related input paths.

### Raw SDL path

Before SDL events are translated into engine events, the SDL runtime path forwards them to:

- `ImGuiSystem::OnSdlEvent(...)`
- `InputSystem::OnSdlEvent(...)`

That gives Dear ImGui access to the native event stream it expects for text input, mouse input, focus, and related UI behavior.

### Engine event path

After translation into engine events, the event route currently flows through:

1. `Application::OnEvent(...)`
2. `ImGuiSystem::CaptureEvent(...)`
3. `LayerStack::OnEvent(...)`
4. `EventBus`
5. built-in handlers

`ImGuiSystem::CaptureEvent(...)` can call `event.Accept()` for keyboard and mouse events when Dear ImGui wants capture.

This matters because it prevents gameplay or lower-level runtime layers from reacting to input that is currently intended for editor UI interaction.

## ImGui Initialization and Frame Model

The host owns ImGui frame sequencing.

Current behavior:

1. `ApplicationHost::Initialize()` calls `ImGuiSystem::Initialize()` before application initialization completes
2. `ApplicationHost::RunFrame(...)` begins an ImGui frame after a graphics frame begins
3. application update and layer update/render logic run inside the same host-owned frame envelope
4. `ImGuiSystem::Render()` submits Dear ImGui draw data before presentation
5. `ApplicationHost::Finalize()` shuts the ImGui system down during host teardown

That means editor and tooling code can assume ImGui work happens inside the same authoritative frame as the rest of rendering, rather than on a separate UI loop.

## Vulkan Tooling Backend

The active editor/tooling renderer path is Vulkan-backed.

The current Vulkan ImGui backend performs the following work:

- creates a dedicated descriptor pool for Dear ImGui
- initializes Dear ImGui against the active Vulkan device and SDL3 platform backend
- renders draw data directly into the active back buffer through dynamic rendering
- manages texture handles for engine `TextureResource` objects shown inside ImGui

This is the key seam that makes editor scene-surface rendering possible.

## Editor Shell Overlay

`Editor/Source/EditorShellOverlay.cpp` is the main current editor UI surface.

It uses the normal `Layer`/`LayerStack` model rather than a separate editor-only framework.

Current behavior includes:

- creating a root dockspace window
- presenting a project hub first when no project is active
- switching between project-hub and workspace modes inside one overlay
- building a default docking layout for `Project Assets`, `Hierarchy`, `Inspector`, `Console`, `Stats`, and `Scene`
- rendering a workspace chrome strip that shows the active project and scene state
- exposing project, scene, window, and layout commands through the main menu bar
- reporting renderer, surface, and scene-render stats in the stats panel
- inspecting camera state through `CameraManager`
- managing a dedicated editor camera
- rendering an offscreen scene-surface and displaying it as an ImGui image

This is important architecturally. The editor is using the same host-owned services as runtime code: `Application`, `InputSystem`, `AssetManager`, `ProjectService`, `SceneService`, `CameraManager`, `Renderer`, `SceneRenderer2D`, `GraphicsDevice`, `ImGuiSystem`, and `LayerStack`.

The overlay acquires those dependencies once through `EditorServices::Acquire(...)`, stores optional references for the lifetime of the overlay attachment, and clears them on detach. That keeps normal editor code owner-bound without repeatedly resolving services on hot paths.

## Workspace Panels

The current editor workspace is composed from focused panel types rather than one monolithic overlay file.

Important current panels include:

- `ProjectAssetsPanel` for project-folder browsing, search, creation, rename, move, delete, drag/drop, and external file import into the active project's `Assets` directory
- `HierarchyPanel` for scene entity hierarchy inspection and selection
- `InspectorPanel` for entity/component inspection and editing
- `ConsolePanel` for log viewing
- `StatsPanel` for renderer, camera, surface, and scene statistics
- `SceneViewportPanel` for offscreen scene presentation and scene interaction
- `FpsOverlayPanel` for optional lightweight runtime frame-rate display

`EditorShell` owns the main dockspace, menu bar, layout persistence flow, and workspace chrome, while `EditorShellOverlay` coordinates the panels and routes scene/project actions.

## Scene Surface Path

The current editor scene-surface is a real engine rendering path, not a placeholder image.

At a high level, `EditorShellOverlay` delegates scene-panel rendering to `SceneViewportPanel`, which currently:

1. creates a `SceneSurface` once renderer, scene-renderer, and ImGui services are available
2. creates or resizes that engine-owned `SceneSurface` to the current viewport region
3. ensures the dedicated editor camera exists and updates its aspect ratio to match the actual surface size
4. uses the editor fly camera only while the editor is in `Edit` mode
5. switches to the scene's resolved primary camera while the editor is in `Play` or `Simulation`
6. enables Unity-style right-mouse fly-camera navigation inside the viewport using SDL relative mouse mode when available in `Edit`
7. renders either the editable scene document or the current runtime-scene clone through `SceneRenderer2D::RenderToSurface(...)`
8. refuses runtime preview rendering when no usable scene camera exists instead of silently falling back to the editor camera
6. gathers scene-quad and renderer statistics for the stats panel
7. presents the completed surface through `ImGuiSystem`

This gives the editor a proper offscreen-rendered scene-surface panel while still staying inside the same frame and renderer ownership model as the runtime.

The scene viewport is also integrated with project-asset drag and drop. Scene assets dragged from `ProjectAssetsPanel` onto the viewport are opened through `SceneService`.

## Camera Usage in the Editor

The editor currently uses a dedicated camera managed through the normal `CameraManager` service.

Current behavior:

- the editor camera tool ensures the editor camera exists on attach and before rendering
- the camera is perspective in the current implementation
- the camera is used only for `Edit` mode viewport navigation and rendering
- the camera's aspect ratio is updated to match the current scene-surface size while editing
- right mouse inside the `Scene` viewport enables fly-camera look with SDL relative mouse mode in `Edit`
- movement uses `W`, `A`, `S`, `D`, vertical motion uses `Q` and `E`, and `Shift` applies a speed boost
- the camera tool maintains yaw and pitch state and updates the persistent camera orientation and position over time
- the overlay destroys the camera on detach if it created it

Scene-owned cameras are now authoritative for runtime preview. Entering `Play` or `Simulation` requires the active scene to already have a usable camera; the editor does not auto-insert one at preview start.

## Editor Transport State

The editor now has a first-pass transport model with:

- `Edit`
- `Play`
- `Simulation`
- runtime-scene clone ownership

Current behavior:

- entering `Play` or `Simulation` clones the editable scene into a transient runtime preview scene
- `Stop` discards that preview scene and restores normal edit-mode rendering
- the current preview path is render-authoritative but not gameplay-authoritative yet
- pause and single-frame step remain disabled until a real runtime scene tick hook exists for the editor

This keeps the editor honest about what is implemented today while preserving the runtime-scene/document split needed for fuller play-mode work later.

## Tooling Availability and Fallback Behavior

The tooling path is intentionally resilient.

Current expectations:

- if graphics-device creation fails, the application can still continue without GPU rendering
- if Dear ImGui is unavailable in the build, `ImGuiSystem` remains unavailable rather than crashing the host
- if the active backend lacks a renderer backend, the editor UI path is unavailable rather than pretending to work
- if offscreen texture creation or texture-handle acquisition fails, the editor logs the failure and avoids invalid rendering for that frame
- if no active project exists, the editor remains in project-hub mode rather than trying to operate on a missing workspace
- if no active scene exists when entering the workspace, the editor opens the configured startup scene or creates a fresh scene document instead of failing hard
- if `SceneSurface::BeginScene2D(...)` cannot start a valid 2D scene, it unwinds the offscreen render state instead of leaving the surface mid-frame
- if `Renderer2D` loses internal GPU resources after an earlier successful initialization, later scene attempts will retry resource creation instead of requiring a process restart

This matches the broader engine policy of visible failures without unnecessary total-runtime collapse.

## Build and Asset Flow

The editor shares the same shader assets used by the runtime rendering path.

Current behavior on Windows builds:

- `Editor/premake5.lua` compiles `Assets/Shaders/Renderer2D.vert` and `Assets/Shaders/Renderer2D.frag`
- the resulting SPIR-V assets are copied under the editor target's `Assets/Shaders` directory
- the editor can therefore use the same `Renderer2D` shader path as the runtime executable

That keeps runtime and editor rendering behavior aligned instead of fragmenting shader ownership between targets.

## Relationship to the Runtime App

The runtime and editor are now distinct consumers of the same engine core.

In practice:

- `Runtime` is the game/runtime integration sample and sandbox app
- `Editor` is the tooling-facing application shell
- both still rely on the same host-owned lifecycle, services, event routing, layer model, and rendering infrastructure

This is a healthy architecture boundary. It keeps the engine reusable while allowing runtime and tooling concerns to evolve separately.

## Recommended Usage

For current tooling and editor work, the safest pattern is:

- treat `ImGuiSystem` as a host-owned service, not a process-global singleton
- build editor UI as layers or overlays inside the existing layer model
- resolve engine services through `Application::GetService<T>()` or `TryGetService<T>()`
- render editor scene-surfaces through explicit render targets owned by rendering services rather than by ad hoc native API calls
- tolerate the absence of graphics or ImGui support gracefully in higher-level tooling code

## Design Rules

Future editor/tooling expansion should preserve the following rules:

- keep the editor as a normal application target that consumes the engine API
- keep `ImGuiSystem` host-owned and optional
- keep one authoritative frame and event pipeline for runtime and tooling work
- keep scene-surface rendering expressed through engine render services rather than bypassing them
- keep tooling layered on top of the existing service and layer model

If the editor grows substantially from here, the next systems should still plug into the current host-owned architecture rather than introducing a second independent tooling runtime.
