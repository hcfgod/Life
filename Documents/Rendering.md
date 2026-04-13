# Rendering

## Purpose

Life's current rendering layer is no longer just a thin wrapper around the backend device.

The active stack now includes a host-owned backend device, a general renderer service, a substantially fuller built-in `Renderer2D`, engine-owned scene-surface tooling integration, and a host-owned camera system. The goal is still the same: keep ownership and frame sequencing explicit while giving application and layer code a practical rendering path that does not take ownership of swapchain lifetime, backend selection, or platform-specific bring-up.

## Ownership Model

`ApplicationHost` owns rendering for the active application instance.

During host construction, the engine:

- creates the platform window
- attempts to create a `GraphicsDevice`
- registers `GraphicsDevice` only when device creation succeeds
- creates and registers `Renderer`, `Renderer2D`, and `SceneRenderer2D` only when rendering services can be constructed successfully
- always creates and registers `CameraManager` as a host-owned service
- always creates and registers `ImGuiSystem` as a host-owned tooling service, even when graphics are unavailable
- keeps the rest of the runtime running even if graphics initialization fails

This means rendering is an optional capability at host startup rather than a hard requirement for the entire application process.

That fallback is intentional. A runtime can still boot, process input, update layers, and execute non-rendering systems even when GPU initialization fails.

## Access Pattern

The preferred access path is owner-bound service lookup from application-facing code.

In practice:

- application and layer code should prefer `Application::TryGetService<T>()`, `GetService<T>()`, or `HasService<T>()`
- rendering consumers should normally ask for `CameraManager`, `SceneRenderer2D`, `Renderer2D`, `Renderer`, or `GraphicsDevice` through the bound application
- host-owned infrastructure may use `ApplicationHost::GetServices()` or host-owned direct state
- global service lookup should remain a last resort for ambient integration boundaries

The current runtime and editor code both follow that model. `Runtime/Source/GameLayer.cpp` resolves `SceneService`, `CameraManager`, and `SceneRenderer2D` from the bound application, keeps an active `Scene`, and renders that scene during `OnRender()`. The editor scene viewport similarly resolves `SceneRenderer2D` and renders the active scene to a `SceneSurface`.

## Current Render Stack

The active rendering stack is layered as follows.

### `GraphicsDevice`

`GraphicsDevice` is the backend abstraction and frame-resource owner.

Its public contract remains intentionally narrow:

- `BeginFrame()`
- `Present()`
- `GetCurrentBackBuffer()`
- `GetNvrhiDevice()`
- `GetCurrentCommandList()`
- `GetBackBufferWidth()`
- `GetBackBufferHeight()`
- `GetBackend()`
- `Resize(...)`

It owns swapchain-facing state, frame activation, and per-frame GPU objects.

### `Renderer`

`Renderer` is the higher-level rendering service that sits above `GraphicsDevice`.

It currently provides:

- shader-library ownership
- viewport and scissor state submission
- clear operations
- non-indexed and indexed draw submission
- graphics-pipeline creation

`RenderCommand` is a thin static forwarding layer over `Renderer`. It exists as a lightweight command surface for higher-level code without turning rendering into a global singleton.

### `Renderer2D`

`Renderer2D` is the current built-in higher-level renderer layered on top of `Renderer`.

It currently provides:

- `BeginScene(...)`
- color and textured quad rendering
- rotated quads
- dynamic instance-data streaming
- texture-aware batching with multiple texture batch ranges per flush
- batched draw submission through `RenderCommand::Draw(...)`
- per-camera viewport/scissor setup
- scene constant uploads for the active view-projection matrix

During `DrawQuad(...)` and `DrawRotatedQuad(...)`, the renderer keeps a shared static quad vertex buffer, builds per-instance transform/color/UV data, and appends those instances to the current batch.

During `Flush()` and `EndScene()`, it:

- uploads the queued instance data into a dynamic instance buffer
- submits one or more queued texture batch ranges from that shared upload
- submits the draw through `RenderCommand::Draw(...)`
- resets the batch for the next scene or flush boundary

The current implementation uploads a small scene constant buffer containing the view-projection matrix and lets the vertex shader apply per-quad translation, rotation, and size on the GPU. That keeps the higher-level API simple while moving the transform work out of the old CPU-baked path.

### `SceneRenderer2D`

`SceneRenderer2D` is the current engine-owned scene submission service layered above `Renderer2D`.

It currently provides:

- a lightweight `Scene2D` description with a required camera plus queued `QuadCommand` values
- direct scene rendering from `Scene` plus `Camera`
- direct scene rendering through `Render(...)`
- offscreen scene rendering through `RenderToSurface(...)`
- a stable consumer-facing seam for runtime and editor code that should not manually sequence `Renderer2D::BeginScene(...)` and `EndScene()`

In practice, `SceneRenderer2D` is the main engine seam between scene data and the lower-level quad renderer. When given a `Scene`, it enumerates enabled entities with `TransformComponent` and `SpriteComponent`, computes world transforms through `Scene::GetWorldTransformMatrix(...)`, builds explicit world-space quad axes from those transforms, and submits the final draw intent through `Renderer2D`.

### `SceneSurface`

`SceneSurface` is the current engine-owned abstraction for offscreen scene rendering into tooling UI.

Current responsibilities:

- own offscreen color-target creation and resize policy
- begin and end scene rendering against the active offscreen surface
- present the completed surface through `ImGuiSystem`
- keep raw render-target transitions and restoration inside `Renderer`

Tooling code should reason in terms of scene surfaces rather than `TextureResource` lifecycle details.

## Current 2D Rendering Path

`Renderer2D` is currently camera-driven.

When `BeginScene(const Camera&)` is called, it:

- converts the camera's normalized viewport into pixel-space viewport/scissor rectangles using the current framebuffer extent
- applies viewport and scissor state through `RenderCommand`
- clears the active back buffer when the camera uses `CameraClearMode::SolidColor`
- uploads the camera view-projection matrix into a scene constant buffer for subsequent draws

During `DrawQuad(...)` and `DrawRotatedQuad(...)`, the renderer builds:

- colored quads
- textured quads
- rotated quads

Current implementation characteristics:

- quad transforms are evaluated on the GPU from per-vertex local positions plus per-quad transform attributes
- the current camera view-projection matrix is uploaded through a scene constant buffer at scene start
- batched instance data is uploaded into one dynamic instance buffer per flush, selected from a rotating set of per-scene buffer versions to avoid reusing in-flight GPU data too aggressively
- scene constant uploads also rotate across a matching set of constant-buffer versions per scene
- pipeline and shader acquisition are prepared internally before scene submission
- draw submission now supports multiple texture batch ranges within one queued vertex upload instead of treating a single active texture as the architectural model
- color-only quads use an internal white texture, while missing-texture draws fall back to an internal magenta error texture

`Renderer2D` has moved well beyond a tiny first pass. The current implementation already covers the common 2D path exercised by the runtime and editor samples while preserving clean ownership and batching seams.

For most higher-level engine consumers, the intended path is now:

1. choose or build a `Camera`
2. keep or resolve an active `Scene` through `SceneService` when scene-driven rendering is desired
3. render through `SceneRenderer2D::Render(scene, camera)` for back-buffer rendering or `SceneRenderer2D::RenderToSurface(surface, scene, camera)` for an offscreen `SceneSurface`
4. use `SceneRenderer2D::Scene2D` only when a caller needs to assemble manual quad commands rather than render engine scene data directly

That keeps application and tooling code focused on scene intent rather than low-level render sequencing.

## Shader Assets and Build Flow

The current `Renderer2D` shaders live in:

- `Assets/Shaders/Renderer2D.vert`
- `Assets/Shaders/Renderer2D.frag`

On Windows builds, both `Runtime/premake5.lua` and `Editor/premake5.lua` add post-build commands that compile those GLSL sources into SPIR-V output under the target application directory:

- `<target>/Assets/Shaders/Renderer2D.vert.spv`
- `<target>/Assets/Shaders/Renderer2D.frag.spv`

At runtime, `Renderer2D` loads those compiled shader binaries relative to the executable directory from `Assets/Shaders`.

If the initial resource or shader acquisition fails, `Renderer2D` logs the failure, stays inert for that frame, and retries resource creation on later scene attempts once the required runtime assets become available again.

The current recovery policy is intentionally defensive:

- shader-library replacement is transactional, so a failed replacement attempt does not discard the last working shader object
- `Renderer2D` treats lost or invalid core GPU resources as recoverable and will rebuild them on the next `BeginScene(...)` path instead of treating the first failure as terminal
- higher-level scene consumers such as `Runtime/Source/GameLayer.cpp` tolerate temporarily missing scene assets and can recover naturally once the underlying asset keys resolve again through `AssetManager`

## ImGui and Tooling Integration

`ImGuiSystem` is host-owned and intentionally optional in the same way the rest of the rendering stack is optional.

Current behavior:

- the host constructs and registers `ImGuiSystem` even when no graphics device exists
- initialization succeeds only when Dear ImGui is present in the build, the window exposes an SDL handle, and the active graphics backend has renderer support
- SDL events are forwarded into `ImGuiSystem::OnSdlEvent(...)` before engine-event translation
- engine events later pass through `ImGuiSystem::CaptureEvent(...)`, which can stop propagation for keyboard and mouse input when the UI wants capture
- editor tooling can render offscreen through an engine-owned `SceneSurface` that owns its offscreen target lifetime while delegating render-target push/pop to `Renderer`

At the moment, the active renderer backend implementation is Vulkan. D3D12 remains a reserved seam rather than a complete tooling path here.

## Scene Surface Boundary

`SceneSurface` is the current engine-owned abstraction for offscreen scene rendering into tooling UI.

Current responsibilities:

- own offscreen color-target creation and resize policy
- begin and end scene rendering against the active offscreen surface
- present the completed surface through `ImGuiSystem`
- keep raw render-target transitions and restoration inside `Renderer`

Tooling code should reason in terms of scene surfaces rather than `TextureResource` lifecycle details.

## Failure Behavior

The host wraps `BeginFrame()` and `Present()` in exception handling and logs failures.

At startup, graphics-device creation failures are caught and downgraded to warnings so the application can continue without GPU rendering. Renderer-service creation failures are also caught and logged without tearing down the rest of the runtime.

For content-driven rendering failures after startup, the current policy is last-known-good plus retry:

- cached assets are reloaded explicitly through `AssetManager` when import and hot-reload paths update source content
- transactional reload paths keep existing asset or shader state alive when a replacement parse or load fails
- renderers and consumers should no-op or fall back for the current frame, then retry naturally on later frames when content becomes valid again

The intended policy is:

- backend bring-up failures are visible
- they do not automatically make the rest of the runtime unusable
- rendering consumers must gracefully tolerate the absence of rendering services or a valid active frame

## Backend Selection

`CreateGraphicsDevice(...)` is the backend factory.

When `GraphicsDeviceSpecification.Backend` is `GraphicsBackend::None`, the engine selects a preferred backend from the compiled feature set.

At the moment:

- Vulkan is preferred when `LIFE_GRAPHICS_VULKAN` is enabled
- D3D12 is reserved in the selection logic but is not the active runtime path here
- if no supported backend is available, device creation throws a `Life::Error` with `ErrorCode::GraphicsError`

That makes backend choice a build-and-platform capability question first, not something hidden behind scattered runtime conditionals.

## Current Vulkan Runtime Model

The active production path is `VulkanGraphicsDevice`.

Its construction sequence is straightforward and explicit:

1. validate that the window supports Vulkan surface creation
2. create the Vulkan instance
3. create the surface from the engine window
4. select a physical device
5. create the logical device and queues
6. create the NVRHI device wrapper
7. create the swapchain
8. wrap swapchain images for NVRHI
9. create the command list

This gives the engine a clear ownership chain from platform window to backend device to per-frame rendering resources.

### NVRHI integration

Life currently uses NVRHI as the command abstraction over Vulkan.

The graphics device exposes:

- the underlying NVRHI device through `GetNvrhiDevice()`
- the active per-frame command list through `GetCurrentCommandList()`
- the wrapped swapchain image through `GetCurrentBackBuffer()`

For the current engine stage, this is a pragmatic choice. It lets higher-level code issue GPU work without taking a direct dependency on raw Vulkan command-buffer management.

### Synchronization and frames in flight

The Vulkan backend currently maintains:

- two frames in flight
- per-frame image-available semaphores
- per-frame render-finished semaphores
- per-frame fences

`BeginFrame()` waits for the current frame fence, acquires the next swapchain image, queues the image-available semaphore wait, opens the command list, and marks the frame active.

`Present()` closes and executes the command list, signals the render-finished semaphore, flushes pending queue operations, signals the in-flight fence, and presents the current swapchain image.

The important developer-facing consequence is simple: the backend owns synchronization, while gameplay and layer code operate against an already-open command list during the active frame.

## Resize and Swapchain Behavior

Swapchain lifetime is backend-owned.

The Vulkan backend recreates the swapchain when:

- image acquisition returns `VK_ERROR_OUT_OF_DATE_KHR`
- presentation returns `VK_ERROR_OUT_OF_DATE_KHR`
- presentation returns `VK_SUBOPTIMAL_KHR`
- `Resize(...)` is called with a materially different size

During these transitions, the backend resets the active command-list state, rebuilds swapchain-backed textures, and recreates the command list.

This is why rendering consumers must not cache per-frame objects such as the current back buffer across frames.

## Relationship to Window Events

Window resize events still flow through the normal engine event system as `WindowResizeEvent`.

That event is useful for application- and layer-level logic such as:

- updating camera projection state
- resizing UI layout state
- invalidating cached render targets owned by higher-level systems

The current runtime sample uses resize events to update camera aspect ratios through `CameraManager::SetAspectRatioAll(...)`.

The swapchain itself remains the responsibility of the graphics backend.

In other words, resize events are for gameplay and engine systems; swapchain recreation is for the rendering backend.

## Recommended Usage

For current application-facing rendering code, the safest pattern is:

- query `CameraManager` and `SceneRenderer2D` from the bound application or layer owner
- treat `SceneRenderer2D`, `Renderer2D`, `Renderer`, and `GraphicsDevice` as optional services when startup rendering failed
- render from `Layer::OnRender()` or another host-driven render callback that executes inside the active frame
- prefer assembling scene data through `SceneRenderer2D::Scene2D` rather than manually sequencing `Renderer2D` in normal runtime or editor code
- do nothing when the relevant rendering services or per-frame objects are unavailable
- avoid caching backend-owned per-frame GPU objects across frames
- keep swapchain, command-list lifetime, and backend synchronization details out of gameplay code

This keeps layer code aligned with the engine's ownership model and avoids introducing ad hoc frame-management logic into application systems.

## Guidance for Future Expansion

As the rendering stack grows, preserve the following rules unless there is an explicit architectural reason to change them:

- keep `ApplicationHost` as the owner of rendering services
- keep frame begin/present sequencing host-owned
- keep backend-specific swapchain and synchronization details out of application code
- keep application-facing rendering access service-based rather than global
- let higher-level render systems build on `Renderer` and host-owned services rather than bypassing lifecycle ownership

The current system is intentionally ownership-first and still evolving. That is appropriate for this stage of the engine: it preserves predictable runtime behavior while leaving room to keep expanding higher-level rendering systems on top of a stable lifecycle and service foundation.
