# Scene System

## Purpose

Life now has an engine-owned scene layer built around `Scene`, `Entity`, built-in components, serialization, and a host-owned `SceneService`.

The goal is to give both runtime and editor code a stable scene-facing API that sits above raw EnTT usage while still preserving explicit ownership and direct data access where the engine needs it.

This document explains the current scene model, built-in components, serialization format, and active-scene workflow.

## Main API Surface

The current scene-system surface is centered on:

- `Scene`
- `Entity`
- `IdComponent`
- `TagComponent`
- `TransformComponent`
- `HierarchyComponent`
- `SpriteComponent`
- `SceneSerializer`
- `SceneService`

## `Scene`

`Scene` is the owning container for scene entities and the authoritative place for hierarchy and transform relationships.

It currently owns:

- the scene name
- the scene source path
- a lightweight load state
- the underlying `entt::registry`
- root-entity ordering

### Scene state

`Scene::State` currently has three values:

- `Unloaded`
- `Loading`
- `Ready`

The state is intentionally small. It is mainly used to distinguish blank, loading, and ready scenes in current runtime/editor flows.

## Entity creation and lookup

`Scene` currently provides:

- `CreateEntity(...)`
- `CreateChildEntity(...)`
- `DestroyEntity(...)`
- `Clear()`
- `FindEntityById(...)`
- `FindEntityByTag(...)`
- `GetEntities()`
- `GetRootEntities()`
- `WrapEntity(...)`

The wrapper-based API is intentional. Application and editor code can work with `Entity` handles instead of talking to the raw registry everywhere.

## `Entity`

`Entity` is the lightweight handle type that couples an EnTT entity ID with its owning `Scene`.

It currently provides:

- validity checks
- scene access
- ID and tag accessors
- enabled-state accessors
- templated component add/get/remove helpers
- parent/child hierarchy helpers

`Entity` is intentionally thin. It does not try to hide the data model behind heavy indirection.

## Built-in Components

The engine currently treats several components as canonical built-ins.

### `IdComponent`

Carries the stable string ID for an entity.

### `TagComponent`

Carries:

- the entity display tag
- the enabled flag used by editor and rendering paths

### `TransformComponent`

Carries local transform values:

- `LocalPosition`
- `LocalRotation`
- `LocalScale`

### `HierarchyComponent`

Carries:

- parent handle
- ordered child handles

### `SpriteComponent`

Carries the current built-in 2D renderable payload:

- `Size`
- `Color`
- `TextureAssetKey`
- `TextureAsset`

This is the component `SceneRenderer2D` currently consumes for scene-driven 2D rendering.

## Component Rules

A few built-in component rules are enforced today.

The canonical built-ins:

- `IdComponent`
- `TagComponent`
- `TransformComponent`
- `HierarchyComponent`

cannot be removed through `Entity::RemoveComponent<T>()`.

That preserves the minimum structural assumptions the rest of the scene system relies on.

## Hierarchy Model

`Scene` owns parent/child relationships directly.

Current hierarchy operations include:

- `SetParent(...)`
- `RemoveParent(...)`
- `GetParent(...)`
- `HasParent(...)`
- `GetChildren(...)`
- `GetSiblingIndex(...)`
- `SetSiblingIndex(...)`
- `IsDescendantOf(...)`

Important current behavior:

- root entities are tracked in explicit order
- reparenting updates both hierarchy components and root ordering
- cycle creation is rejected
- child traversal is scene-owned rather than reconstructed ad hoc

That gives the editor hierarchy panel and serializer a stable ordering model.

## Transform Model

`Scene` currently provides:

- `GetLocalTransformMatrix(...)`
- `GetWorldTransformMatrix(...)`

Local transforms come from `TransformComponent`. World transforms are composed by walking the hierarchy.

This matters because scene rendering uses the world transform, not just local position and a single Z rotation.

## Serialization

`SceneSerializer` is the authoritative on-disk scene translation layer.

It currently provides:

- `Load(...)`
- `Save(...)`

Important current constant:

- `SceneFileCurrentVersion = 1`

## Scene File Format

The current scene format is JSON-based.

Saved scene data includes:

- `version`
- `name`
- `entities`

Each serialized entity can include:

- `id`
- `tag`
- `enabled`
- `parentId`
- `transform.position`
- `transform.rotation`
- `transform.scale`
- `sprite.size`
- `sprite.color`
- `sprite.textureAssetKey`

The serializer writes entities by recursively visiting root entities and their children, which preserves hierarchy relationships in a scene-friendly order.

## Scene Load Behavior

`SceneSerializer::Load(...)` currently:

- requires a source path
- normalizes that path
- parses JSON and validates the version
- creates a new `Scene`
- sets the scene source path
- creates entities and restores built-in data
- resolves sprite texture assets immediately when an `AssetManager` is provided
- replays parent-child links after entity creation
- marks the scene `Ready`

This two-phase parent restoration is important because parent IDs may refer to entities that appear later in the file.

## Scene Save Behavior

`SceneSerializer::Save(...)` currently:

- requires a source path
- creates parent directories as needed
- serializes roots recursively
- writes to a temporary file first
- renames the temporary file into place
- falls back to remove-and-rename if replacement needs a second attempt

Like the project serializer, the scene serializer uses a defensive replacement flow rather than in-place overwrite.

## `SceneService`

`SceneService` is the host-owned runtime service that manages the active scene for the current application host.

It currently provides:

- `BindAssetManager(...)`
- `UnbindAssetManager()`
- `CreateScene(...)`
- `LoadScene(...)`
- `OpenScene(...)`
- `SetActiveScene(...)`
- `CloseScene()`
- `SaveActiveScene()`
- `SaveActiveSceneAs(...)`
- dirty-state and active-scene access helpers

## Host Integration

`ApplicationHost` constructs and registers `SceneService` during host setup.

Current behavior:

- `SceneService` is created after `AssetManager` and `ProjectService`
- it is bound to the host-owned `AssetManager`
- it is registered in the host `ServiceRegistry`
- if a startup project is opened and that project specifies `Startup.Scene`, the host opens that scene before application initialization completes

This makes scene state available to both runtime and editor code through the normal service model.

## Path Resolution Rules

`SceneService` resolves scene paths in a project-aware way.

For relative paths, it currently tries the following in order:

- resolve the path as an asset key through the asset-path helpers
- if a project root is active, interpret the path relative to that root
- otherwise fall back to an absolute lexical path from the current process context

That lets editor and runtime code open scenes through either project-relative asset-style paths or explicit filesystem paths.

## Create, Load, and Open Semantics

### `CreateScene(...)`

Creates a fresh in-memory scene, marks it `Ready`, makes it active, and clears dirty state.

### `LoadScene(...)`

Attempts to open an existing scene and fails if the path is empty, missing, or invalid.

### `OpenScene(...)`

Uses a more editor-friendly flow.

If the target scene cannot be loaded and blank fallback is allowed, it creates a ready blank scene associated with that path instead of failing hard.

This is why the editor can move cleanly between new and existing scene documents.

## Dirty-State Model

`SceneService` owns dirty tracking for the active scene.

Current helpers include:

- `IsActiveSceneDirty()`
- `MarkActiveSceneDirty()`
- `ClearDirty()`

Current behavior:

- creating, loading, opening, or setting a scene clears dirty state
- saving clears dirty state on success
- creating an unsaved scene in editor workflows may immediately mark it dirty to reflect unsaved work

The dirty flag is service-owned rather than embedded deeply inside every component mutation path.

## Asset Resolution

When `SceneService` has an `AssetManager` bound, it resolves sprite asset references for the active scene.

Current behavior:

- on scene bind or scene open, iterate entities
- if a `SpriteComponent` has a `TextureAssetKey` but no live `TextureAsset`, call `AssetManager::GetOrLoad<Assets::TextureAsset>(...)`

This keeps on-disk scene data stable as asset keys while still giving the live scene direct asset references when available.

## Rendering Integration

The current scene/render seam is straightforward.

`SceneRenderer2D` enumerates enabled entities that have both:

- `TransformComponent`
- `SpriteComponent`

It then:

- computes each entity's world transform through `Scene::GetWorldTransformMatrix(...)`
- builds explicit world-space quad axes from that matrix and sprite size
- submits the final draw intent through `Renderer2D`

This preserves hierarchy, scale, and full rotation effects in scene-driven sprite rendering.

## Editor Integration

The editor uses `SceneService` as the authoritative scene-document owner.

Current editor behavior includes:

- creating scenes from menu commands and the project assets panel
- opening scenes from dialogs or drag/drop
- saving active scenes and save-as flows through `SceneService`
- clearing selection when the active scene changes
- updating the active project's `Startup.Scene` after successful scene open/save flows

## Runtime Integration

The runtime also uses `SceneService` directly.

Current runtime behavior includes:

- ensuring an active runtime scene exists
- populating that scene with sample entities when needed
- rendering the active scene through `SceneRenderer2D`
- updating scene entity transforms over time

That keeps runtime sample logic on the same scene-facing API surface as the editor.

## Recommended Usage

For current engine and application code, the safest pattern is:

- work through `SceneService` for active-scene ownership
- treat `Entity` as the normal consumer-facing scene handle
- keep built-in structural components present on every entity
- serialize scene asset references as asset keys, not raw runtime pointers
- render scenes through `SceneRenderer2D` instead of manually mirroring scene traversal in application code

## Design Rules

Future scene-system changes should preserve the following rules unless there is a strong reason to change them explicitly:

- keep `SceneService` host-owned and authoritative for the active scene
- keep `Scene` and `Entity` as the primary consumer-facing scene API above raw EnTT usage
- keep hierarchy and world-transform computation scene-owned
- keep scene persistence versioned and defensive
- keep runtime and editor scene workflows on the same engine surface
