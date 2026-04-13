# Project System

## Purpose

Life now has an engine-owned project system rather than treating project folders as editor-only state.

The project system gives the engine a canonical way to describe an active workspace, resolve project-relative asset roots, store startup content such as the default scene, and let both runtime and editor code work through the same service boundary.

This document explains the current descriptor model, serializer rules, and `ProjectService` behavior.

## Main API Surface

The current project-system surface is centered on:

- `Assets::ProjectDescriptor`
- `Assets::ProjectPaths`
- `Assets::Project`
- `Assets::ProjectCreateOptions`
- `Assets::ProjectSerializer`
- `Assets::ProjectService`

## Descriptor Model

Projects are described by a `.lifeproject` JSON descriptor.

The current descriptor contains:

- `Version`
- `Name`
- `EngineVersion`
- `Paths.Assets`
- `Paths.Settings`
- `Startup.Scene`

Important current constants:

- `ProjectDescriptorCurrentVersion = 1`
- `ProjectDescriptorFileExtension = ".lifeproject"`
- `ProjectDefaultEngineVersion = "0.1.0"`

The descriptor stores logical project settings. Resolved absolute paths live in the separate `ProjectPaths` structure.

## Resolved Paths

At runtime, a loaded project also carries resolved path information through `ProjectPaths`:

- `RootDirectory`
- `DescriptorPath`
- `AssetsDirectory`
- `SettingsDirectory`

This split is intentional.

The descriptor remains portable and relative where appropriate, while the engine can still work with normalized absolute filesystem paths once a project is loaded or created.

## Serializer Responsibilities

`ProjectSerializer` is the authoritative translation layer between on-disk JSON and in-memory project state.

It currently provides:

- `Load(...)`
- `Save(...)`
- `CreateInMemory(...)`
- `CreateOnDisk(...)`

### Load behavior

`ProjectSerializer::Load(...)`:

- requires a descriptor path
- normalizes that path to an absolute lexical form
- parses the descriptor JSON
- derives `RootDirectory`, `AssetsDirectory`, and `SettingsDirectory`
- validates both descriptor contents and resolved path rules

### Save behavior

`ProjectSerializer::Save(...)` uses a defensive replace flow:

- validate the project first
- ensure the project directory structure exists
- write JSON to a temporary file beside the real descriptor
- rename the temporary file into place
- fall back to remove-and-rename if the first replacement attempt fails

That keeps descriptor writes more durable than direct in-place overwrite.

### Create behavior

`CreateProjectFromOptions(...)` currently:

- requires a non-empty root directory
- infers the project name from the root directory when no explicit name is provided
- defaults `EngineVersion`, `Assets`, and `Settings` when omitted
- sanitizes or resolves the descriptor file name
- stores `StartupScene` into `Descriptor.Startup.Scene`
- derives absolute project paths from the chosen root

`CreateOnDisk(...)` then creates the directories and persists the descriptor to disk.

## Validation Rules

The current serializer enforces a small but important set of invariants.

### Descriptor rules

The descriptor must satisfy the following:

- `Version` must match `ProjectDescriptorCurrentVersion`
- `Name` must not be empty after trimming
- `Paths.Assets` and `Paths.Settings` must not be empty
- asset and settings paths must be relative, not absolute
- `Startup.Scene`, when present, must also be relative

### Project path rules

The resolved project must satisfy the following:

- `RootDirectory` and `DescriptorPath` must both exist in the in-memory model
- the descriptor must live directly under the project root
- `AssetsDirectory` and `SettingsDirectory` must both be derivable

These rules keep the project shape explicit and avoid ambiguous layouts.

## `ProjectService`

`ProjectService` is the host-owned runtime service that manages the active project for the current application host.

It currently provides:

- `BindAssetSystems(...)`
- `UnbindAssetSystems()`
- `CreateProject(...)`
- `OpenProject(...)`
- `SaveProject()`
- `SaveProjectAs(...)`
- `CloseProject()`
- active-project query and access helpers

## Host Integration

`ApplicationHost` constructs and registers `ProjectService` during host setup.

Current setup behavior:

- `AssetDatabase` and `AssetManager` are created first
- `ProjectService` is bound to those asset systems
- `ProjectService` is registered in the host `ServiceRegistry`
- if `ApplicationSpecification.ProjectDescriptorPath` is provided, the host opens that project before application initialization completes

This makes the project system available to both runtime and editor applications through the same service model.

## Active-Project Rebinding

The most important current responsibility of `ProjectService` is rebinding the active project root.

When a project becomes active, `ProjectService`:

- updates the process-wide active project root through the asset-path helpers
- resets `AssetDatabase`
- clears `AssetManager` caches

When a project is closed, it:

- clears the active project root
- resets `AssetDatabase`
- clears `AssetManager` caches

This behavior is deliberate. Asset lookup and caching must follow the current project boundary rather than leaking state from a previous workspace.

## Save and Close Semantics

### Save

`SaveProject()` requires an active project and persists the current in-memory descriptor.

`SaveProjectAs(...)`:

- requires an active project
- requires a non-empty descriptor path
- updates the resolved root, descriptor, assets, and settings paths
- saves the updated project
- makes that updated project active

### Close

`CloseProject()` is idempotent-friendly.

If no project is active, it still attempts to rebind the project root to the empty state so asset lookups stay coherent.

If a project is active, it clears the binding first and only then drops the in-memory active project.

## Editor Integration

The editor uses `ProjectService` directly for the project-first workflow.

Current editor behavior includes:

- the project hub creates projects through `CreateProject(...)`
- the project hub opens projects through `OpenProject(...)`
- the project hub tracks recent projects independently in a user-data JSON file
- the workspace updates `Descriptor.Startup.Scene` when scene operations succeed
- closing a project returns the editor to project-hub mode

This keeps the editor aligned with the same engine-owned project model used by the runtime.

## Runtime Integration

The runtime can also enter through a project-aware path.

If `ApplicationSpecification.ProjectDescriptorPath` is set:

- the host opens that project during construction
- the project root becomes authoritative for asset-path resolution
- `SceneService` may open `Descriptor.Startup.Scene` automatically when one is configured

That makes the project system runtime-aware rather than editor-only.

## Recommended Usage

For current engine and application code, the safest pattern is:

- treat `ProjectService` as the authoritative active-project boundary
- store project-relative configuration in the descriptor, not absolute machine-specific paths
- let serializer validation reject malformed descriptors instead of silently accepting them
- expect asset caches to reset when the active project changes
- update `Startup.Scene` only with project-relative scene paths

## Design Rules

Future project-system changes should preserve the following rules unless there is a strong reason to change them explicitly:

- keep the project system engine-owned rather than editor-private
- keep descriptor content relative and portable where appropriate
- keep active-project rebinding authoritative for asset-root resolution
- keep project persistence defensive and transactional where practical
- keep runtime and editor code on the same `ProjectService` surface
