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

### `Renderer2D`

`Renderer2D` is the first built-in scene-facing renderer.

Its current implementation is intentionally narrow:

- color-only quad rendering
- dynamic CPU-updated vertex streaming
- batched non-indexed triangle submission
- per-camera viewport/scissor setup
- per-camera clear handling

The current 2D path does not introduce a full material system, texture binding model, or generalized scene graph. It is a first-pass renderer built on the stable ownership model above.

### `Camera` and `CameraManager`

`Camera` represents the view/projection state used by higher-level rendering code.

The current camera surface includes:

- perspective and orthographic projection
- position/orientation state
- `LookAt(...)` helpers
- clear mode and clear color
- normalized viewport rectangles
- lazy view/projection rebuilds

`CameraManager` is a host-owned service that stores named cameras, supports explicit primary-camera selection, provides priority-sorted retrieval, and can update aspect ratio across all managed cameras.

## Frame Ownership

The host owns the frame envelope.

`ApplicationHost::RunFrame(...)` currently performs render-related work in this order:

1. update input actions
2. call `GraphicsDevice::BeginFrame()` when a device exists
3. invoke `Application::OnUpdate(...)` through `OnHostRunFrame(...)`
4. run `LayerStack::OnUpdate(...)` if the application is still running
5. run `LayerStack::OnRender()` when a frame was successfully started and the application is still running
6. call `GraphicsDevice::Present()` when a frame was successfully started
7. end the input frame before returning

Application and layer code render inside that host-owned frame window. They do not start or end frames themselves.

## Validity of Per-Frame Objects

`GetCurrentBackBuffer()` and `GetCurrentCommandList()` are only valid while a frame is active.

The current Vulkan backend explicitly returns `nullptr` when no frame is active. That can happen when:

- the host has no graphics device
- `BeginFrame()` failed
- swapchain acquisition forced a resize/recreate path
- presentation has already completed

Code that records rendering commands must therefore treat these pointers as conditional and must not cache them across frames.

## Current 2D Rendering Path

`Renderer2D` is currently camera-driven.

When `BeginScene(const Camera&)` is called, it:

- converts the camera's normalized viewport into pixel-space viewport/scissor rectangles using the current framebuffer extent
- applies viewport and scissor state through `RenderCommand`
- clears the active back buffer when the camera uses `CameraClearMode::SolidColor`
- caches the camera view-projection matrix for subsequent draws

During `DrawQuad(...)` and `DrawRotatedQuad(...)`, the renderer builds transformed quad vertices on the CPU and appends them to the current batch.

During `Flush()` and `EndScene()`, it:

- uploads the batched vertex data into a dynamic vertex buffer
- lazily creates the required graphics pipeline if needed
- submits the draw through `RenderCommand::Draw(...)`
- resets the batch for the next scene or flush boundary

The current implementation deliberately keeps transform handling simple by multiplying each quad vertex by the scene view-projection matrix on the CPU before upload. That avoids introducing constant-buffer or push-constant machinery into the first-pass renderer.

## Shader Assets and Build Flow

The current `Renderer2D` shaders live in:

- `Assets/Shaders/Renderer2D.vert`
- `Assets/Shaders/Renderer2D.frag`

On Windows builds, `Runtime/premake5.lua` adds post-build commands that compile those GLSL sources into SPIR-V output under the target runtime directory:

- `<target>/Assets/Shaders/Renderer2D.vert.spv`
- `<target>/Assets/Shaders/Renderer2D.frag.spv`

At runtime, `Renderer2D` loads those compiled shader binaries relative to the executable directory from `Assets/Shaders`.

If the vertex buffer or shader loads fail, `Renderer2D` logs initialization failure and remains unable to render until the process is restarted with a valid runtime asset layout.

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
