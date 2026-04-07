# Rendering

## Purpose

Life's current rendering layer is still intentionally conservative, but it is no longer just a raw device wrapper.

The active stack now includes a host-owned backend device, a general renderer service, a first-pass 2D renderer, and a host-owned camera system. The goal is still the same: keep ownership and frame sequencing explicit while giving application and layer code a practical rendering path that does not take ownership of swapchain lifetime, backend selection, or platform-specific bring-up.

## Ownership Model

`ApplicationHost` owns rendering for the active application instance.

During host construction, the engine:

- creates the platform window
- attempts to create a `GraphicsDevice`
- registers `GraphicsDevice` only when device creation succeeds
- creates and registers `Renderer` and `Renderer2D` only when rendering services can be constructed successfully
- always creates and registers `CameraManager` as a host-owned service
- always creates and registers `ImGuiSystem` as a host-owned tooling service, even when graphics are unavailable
- keeps the rest of the runtime running even if graphics initialization fails

This means rendering is an optional capability at host startup rather than a hard requirement for the entire application process.

That fallback is intentional. A runtime can still boot, process input, update layers, and execute non-rendering systems even when GPU initialization fails.

## Access Pattern

The preferred access path is owner-bound service lookup from application-facing code.

In practice:

- application and layer code should prefer `Application::TryGetService<T>()`, `GetService<T>()`, or `HasService<T>()`
- rendering consumers should normally ask for `CameraManager`, `Renderer2D`, `Renderer`, or `GraphicsDevice` through the bound application
- host-owned infrastructure may use `ApplicationHost::GetServices()` or host-owned direct state
- global service lookup should remain a last resort for ambient integration boundaries

The current runtime sample in `Runtime/Source/GameLayer.cpp` follows that model: it resolves `CameraManager` and `Renderer2D` from the bound application, performs camera selection in update/event paths, and renders during `OnRender()`.

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

### `Renderer2D` is currently intended as the first built-in higher-level renderer layered on top of `Renderer`.

It currently provides:

- `BeginScene(...)`
- color-only quad rendering
- dynamic CPU-updated vertex streaming
- batched non-indexed triangle submission
- per-camera viewport/scissor setup

During `DrawQuad(...)` and `DrawRotatedQuad(...)`, the renderer builds transformed quad vertices on the CPU and appends them to the current batch.

During `Flush()` and `EndScene()`, it:

- uploads the batched vertex data into a dynamic vertex buffer
- submits one or more queued texture batch ranges from that shared upload
- submits the draw through `RenderCommand::Draw(...)`
- resets the batch for the next scene or flush boundary

The current implementation deliberately keeps transform handling simple by multiplying each quad vertex by the scene view-projection matrix on the CPU before upload. That avoids introducing constant-buffer or push-constant machinery into the first-pass renderer.

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
- caches the camera view-projection matrix for subsequent draws

During `DrawQuad(...)` and `DrawRotatedQuad(...)`, the renderer builds:

- colored quads
- textured quads
- rotated quads

Current implementation characteristics:

- quad vertices are transformed on the CPU using the current view-projection matrix before upload
- batched geometry is uploaded into one dynamic vertex buffer per flush
- pipeline and shader acquisition are prepared internally before scene submission
- draw submission now supports multiple texture batch ranges within one queued vertex upload instead of treating a single active texture as the architectural model

This is still an early renderer, but the resource-ownership and batching seams are now cleaner than the original one-texture-per-flush path.

## Shader Assets and Build Flow

The current `Renderer2D` shaders live in:

- `Assets/Shaders/Renderer2D.vert`
- `Assets/Shaders/Renderer2D.frag`

On Windows builds, both `Runtime/premake5.lua` and `Editor/premake5.lua` add post-build commands that compile those GLSL sources into SPIR-V output under the target application directory:

- `<target>/Assets/Shaders/Renderer2D.vert.spv`
- `<target>/Assets/Shaders/Renderer2D.frag.spv`

At runtime, `Renderer2D` loads those compiled shader binaries relative to the executable directory from `Assets/Shaders`.

If the vertex buffer or shader loads fail, `Renderer2D` logs initialization failure and remains unable to render until the process is restarted with a valid runtime asset layout.

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

- query `CameraManager` and `Renderer2D` from the bound application or layer owner
- treat `Renderer2D`, `Renderer`, and `GraphicsDevice` as optional services when startup rendering failed
- render from `Layer::OnRender()` or another host-driven render callback that executes inside the active frame
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

The current system is intentionally conservative. That is appropriate for this stage of the engine. It establishes predictable runtime behavior first, so that more advanced rendering systems can be added on top of a stable ownership and lifecycle foundation.
