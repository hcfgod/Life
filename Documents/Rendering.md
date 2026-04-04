# Rendering

## Purpose

Life's current rendering layer is intentionally small.

It provides a host-owned graphics device, a minimal frame boundary API, and just enough abstraction for application and layer code to render without taking ownership of swapchain lifetime, backend selection, or platform-specific device bring-up.

That design is deliberate. At the moment, the rendering system is not trying to be a full render graph, material system, or scene renderer. It is establishing the engine-side ownership model and the runtime contracts that future rendering systems will build on.

## Ownership Model

`ApplicationHost` owns rendering for the active application instance.

During host construction, the engine:

- creates the platform window
- attempts to create a `GraphicsDevice`
- registers `GraphicsDevice` in the host-owned `ServiceRegistry` only when device creation succeeds
- keeps the rest of the application running even if graphics initialization fails

This means rendering is an optional capability at host startup rather than a hard requirement for the entire application process.

That fallback is intentional. A runtime can still boot, process input, and execute non-rendering systems even when GPU initialization fails.

## Access Pattern

The preferred access path is owner-bound service lookup from application-facing code.

In practice:

- application and layer code should prefer `Application::TryGetService<GraphicsDevice>()` or `Application::GetService<GraphicsDevice>()`
- host-owned infrastructure may use `ApplicationHost::GetGraphicsDevice()` or `ApplicationHost::GetServices()`
- global service lookup should remain a last resort for ambient integration boundaries

A layer-facing example already exists in `Runtime/Source/GameLayer.cpp`, where the layer asks the bound application for `GraphicsDevice`, then checks whether the current back buffer and command list are available before issuing commands.

That null-checking is not defensive ceremony. It reflects the actual contract of the rendering API.

## GraphicsDevice Contract

`GraphicsDevice` is the engine's current backend abstraction.

Its public contract is intentionally narrow:

- `BeginFrame()`
- `Present()`
- `GetCurrentBackBuffer()`
- `GetNvrhiDevice()`
- `GetCurrentCommandList()`
- `GetBackBufferWidth()`
- `GetBackBufferHeight()`
- `GetBackend()`
- `Resize(...)`

A few details matter when consuming this API.

### Frame ownership

The host owns the frame envelope.

`ApplicationHost::RunFrame(...)` currently performs rendering-related work in this order:

1. update input actions
2. call `GraphicsDevice::BeginFrame()` when a device exists
3. run `Application::OnUpdate(...)`
4. run `LayerStack::OnUpdate(...)` if the application is still running
5. call `GraphicsDevice::Present()` when a frame was successfully started

Application and layer code render inside that host-owned frame window. They do not start or end frames themselves.

### Validity of per-frame objects

`GetCurrentBackBuffer()` and `GetCurrentCommandList()` are only valid while a frame is active.

The current Vulkan backend explicitly returns `nullptr` when no frame is active. That can happen when:

- the host has no graphics device
- `BeginFrame()` failed
- swapchain acquisition forced a resize/recreate path
- presentation has already completed

Code that records rendering commands must therefore treat these pointers as conditional.

### Failure behavior

The host wraps `BeginFrame()` and `Present()` in exception handling and logs failures.

At startup, graphics-device creation failures are also caught and downgraded to a warning, allowing the application to continue without GPU rendering.

The intended policy is:

- backend bring-up failures are visible
- they do not automatically make the rest of the runtime unusable
- rendering consumers must gracefully tolerate the absence of a valid frame

## Backend Selection

`CreateGraphicsDevice(...)` is the backend factory.

When `GraphicsDeviceSpecification.Backend` is `GraphicsBackend::None`, the engine selects a preferred backend from the compiled feature set.

At the moment:

- Vulkan is preferred when `LIFE_GRAPHICS_VULKAN` is enabled
- D3D12 is reserved in the selection logic but not implemented here as the active runtime path
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

The swapchain itself remains the responsibility of the graphics backend.

In other words, resize events are for gameplay and engine systems; swapchain recreation is for the rendering backend.

## Recommended Usage

For current application-facing rendering code, the safest pattern is:

- query `GraphicsDevice` from the bound application or layer owner
- ask for the current back buffer and command list during `OnUpdate(...)`
- do nothing if either object is unavailable
- avoid caching per-frame GPU objects that are owned by the backend
- treat backend creation and frame availability as conditional runtime state

This keeps layer code aligned with the engine's ownership model and avoids introducing ad hoc frame-management logic into application systems.

## Guidance for Future Expansion

As the rendering stack grows, preserve the following rules unless there is an explicit architectural reason to change them:

- keep `ApplicationHost` as the owner of the graphics device
- keep frame begin/present sequencing host-owned
- keep backend-specific swapchain and synchronization details out of application code
- keep application-facing rendering access service-based rather than global
- keep the `GraphicsDevice` contract small and stable until a broader rendering architecture is ready

The current system is intentionally conservative. That is appropriate for this stage of the engine. It establishes predictable runtime behavior first, so that more advanced rendering systems can be added on top of a stable ownership and lifecycle foundation.
