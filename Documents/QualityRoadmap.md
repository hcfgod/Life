# Quality Roadmap

## Purpose

Life separates fast pull-request confidence from heavier release and runtime validation. Pull requests should stay focused on build, test, static analysis, and script self-tests. Expensive live graphics, benchmark, sanitizer, and dependency inventory checks can run on schedule or by manual dispatch.

## Build Metadata and CLI Diagnostics

Premake defines build metadata for every target:

- `LIFE_BUILD_VERSION`
- `LIFE_BUILD_COMMIT`
- `LIFE_BUILD_DATE`
- `LIFE_BUILD_ARCHITECTURE`
- `LIFE_BUILD_CONFIGURATION`

CI can override these through environment variables. Local builds fall back to `git describe --tags --always --dirty`, the short git SHA, and the current UTC build time.

`Runtime` and `Editor` support these process-level options before any window or host is created:

```text
--help
--version
--diagnostics
```

The first non-option argument remains the project descriptor path.

## Release Artifact Validation

Tag releases package the `Runtime` target for each release platform, then run `Scripts/CI/validate_release_artifact.py` on the same native runner before uploading artifacts. The validator extracts the archive, checks the executable and SDL runtime layout, runs `Runtime --diagnostics`, and writes a `.sha256` file beside the archive.

The validator is intentionally app-name configurable so the same path can validate future `Editor` release artifacts.

## Deep Quality Lanes

`.github/workflows/nightly-deep-quality.yml` contains scheduled/manual checks for:

- sanitizer builds
- Windows live backend smoke
- report-only benchmarks
- dependency/license inventory

Benchmarks write JSON artifacts and do not fail on performance thresholds yet. Thresholds should only be added after enough stable history exists.

The live backend smoke test can emit a deterministic PPM artifact under `VisualSmoke/`. `Scripts/CI/validate_visual_smoke.py` validates dimensions, nonblank output, and minimum color variety. This is a smoke threshold path, not pixel-perfect screenshot comparison.

## Dependency Inventory

`Scripts/CI/generate_dependency_inventory.py` writes `Build/Reports/dependency-inventory.json` with dependency presence, git revision when available, and license-file path. Keep this report updated when adding or replacing third-party code.

## Crash Reports

Crash diagnostics still write the human-readable `.crash.txt` report. They also write a `.crash.json` sidecar beside it with schema `life.crash-report.v1`, build metadata, command line, platform data, phase, reason, report path, and stack trace.

Use the JSON sidecar for automated collection and the text report for direct debugging.

## Contributor Patterns

When adding a new engine surface:

- Add public interfaces under `Engine/Include` and implementation under `Engine/Source`.
- Prefer host-owned services over globals.
- Add doctest coverage for ownership, lifecycle, failure, and serialization behavior.
- Add CI script validation when a workflow depends on a nontrivial script.
- Keep heavy graphics/performance validation behind nightly/manual gates unless it is cheap and deterministic.
