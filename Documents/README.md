# Life Engine Documents

This folder contains implementation-facing documentation for the engine's runtime architecture, operational behavior, and integration surfaces.

The intent is to explain how the current systems fit together, what owns what at runtime, and where application or engine code is expected to plug in without forcing readers to reconstruct design intent from headers alone.

## Recommended Reading Order

- `ApplicationArchitecture.md` for the canonical startup path, ownership model, and service boundaries.
- `ProjectSystem.md` for the engine-owned project descriptor model, active-project service behavior, and asset-root rebinding.
- `SceneSystem.md` for `Scene`, `Entity`, built-in components, scene serialization, and `SceneService` workflow.
- `EntryPointsAndBootstrap.md` for executable entry, SDL callback bootstrap, runner state, and exception boundaries.
- `LayersAndOverlays.md` for the host-owned layer model, overlay ordering, attach/detach rules, and layer participation in frame and event flow.
- `Rendering.md` for rendering ownership, service boundaries, cameras, `Renderer`, `Renderer2D`, `SceneRenderer2D`, scene surfaces, shader asset flow, and current Vulkan/NVRHI runtime behavior.
- `EditorAndTooling.md` for the dedicated editor app, `ImGuiSystem`, docking UI, cached editor services, and offscreen scene-surface behavior.
- `InputSystem.md` for raw input state, action assets, rebinding, and frame-based input semantics.
- `EventThreadingInvariants.md` for the event pipeline and thread-safety assumptions that shape runtime behavior.
- `Logging.md` for the authoritative logging configuration model.
- `CrashDiagnostics.md` for crash-reporting lifecycle, output, and operational guidance.
- `ErrorHandling.md` for the structured error model, result types, and error utilities.
- `PlatformRuntime.md` for runtime platform metadata, SDL runtime ownership, and low-level platform utilities.
- `PlatformSupport.md` for host and target platform expectations.

## Documents

- `ApplicationArchitecture.md` - startup flow, ownership boundaries, service registry behavior, and the authoritative application loop.
- `ProjectSystem.md` - project descriptor shape, serializer rules, `ProjectService` lifecycle, active-project rebinding, and editor integration.
- `SceneSystem.md` - `Scene`/`Entity` structure, built-in components, scene serialization, scene dirty/save behavior, and `SceneService` workflow.
- `EntryPointsAndBootstrap.md` - executable entry, SDL callback bootstrap, runner iteration, event injection, and teardown responsibilities.
- `LayersAndOverlays.md` - the host-owned layer model, overlay ordering, update/render/event traversal, and attach/detach semantics.
- `Rendering.md` - rendering ownership, frame sequencing, service responsibilities, camera integration, `Renderer2D`, `SceneRenderer2D`, scene surfaces, shader assets, and current Vulkan/NVRHI behavior.
- `EditorAndTooling.md` - the dedicated editor app, host-owned `ImGuiSystem`, tooling event/frame integration, cached editor services, and the current scene-surface path.
- `InputSystem.md` - host-owned input architecture, raw-state polling, action assets, rebinding, and input frame semantics.
- `EventThreadingInvariants.md` - event ordering, runtime ownership, and thread-safety boundaries for the current engine architecture.
- `Logging.md` - engine logging configuration, sink behavior, reconfiguration, and integration guidance.
- `CrashDiagnostics.md` - crash-reporting configuration, install timing, report contents, and platform-specific behavior.
- `ErrorHandling.md` - the engine error model, `Result<T>` conventions, assertions, verification, and system error mapping.
- `PlatformRuntime.md` - runtime platform detection, `ApplicationRuntime`, SDL runtime lifetime, and `PlatformUtils` behavior.
- `PlatformSupport.md` - supported host and target platforms, Windows/Linux/macOS architecture notes, and CI/build expectations.
