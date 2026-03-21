import argparse
import json
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("compile_commands_path")
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--project-root", action="append", dest="project_roots", default=[])
    return parser.parse_args()


def resolve_source_path(raw_path: str, directory: Path, repo_root: Path, project_roots: list[str]) -> Path:
    raw = Path(raw_path)
    candidates: list[Path] = []

    if raw.is_absolute():
        candidates.append(raw)
    else:
        candidates.append((directory / raw).resolve())
        candidates.append((repo_root / raw).resolve())

        normalized_raw = Path(str(raw).lstrip("./\\"))
        if normalized_raw.parts and normalized_raw.parts[0] == "Source":
            for project_root in project_roots:
                candidates.append((repo_root / project_root / normalized_raw).resolve())

    for candidate in candidates:
        if candidate.exists():
            return candidate.resolve()

    if candidates:
        return candidates[0]

    return raw.resolve() if raw.is_absolute() else (directory / raw).resolve()


def rewrite_arguments(arguments: list[object], original_file: str, resolved_file: str) -> list[object]:
    rewritten: list[object] = []
    original_path = Path(original_file)
    original_name = original_path.name

    for argument in arguments:
        if not isinstance(argument, str):
            rewritten.append(argument)
            continue

        normalized_argument = argument.replace("\\", "/")
        normalized_original = original_file.replace("\\", "/")

        if argument == original_file or normalized_argument == normalized_original:
            rewritten.append(resolved_file)
            continue

        if argument.endswith(original_name) and normalized_original.endswith(normalized_argument):
            rewritten.append(resolved_file)
            continue

        rewritten.append(argument)

    return rewritten


def main() -> int:
    arguments = parse_arguments()
    repo_root = Path(arguments.repo_root).resolve()
    compile_commands_path = Path(arguments.compile_commands_path).resolve()
    project_roots = arguments.project_roots or ["Engine", "Runtime", "Test"]

    with compile_commands_path.open("r", encoding="utf-8") as file:
        compile_commands = json.load(file)

    for entry in compile_commands:
        raw_directory = str(entry.get("directory", "."))
        directory = Path(raw_directory)
        if not directory.is_absolute():
            directory = (repo_root / directory).resolve()
        else:
            directory = directory.resolve()

        entry["directory"] = str(directory)

        raw_file = str(entry.get("file", ""))
        if not raw_file:
            continue

        resolved_file = resolve_source_path(raw_file, directory, repo_root, project_roots)
        entry["file"] = str(resolved_file)

        if isinstance(entry.get("arguments"), list):
            entry["arguments"] = rewrite_arguments(entry["arguments"], raw_file, str(resolved_file))

    with compile_commands_path.open("w", encoding="utf-8") as file:
        json.dump(compile_commands, file, indent=2)
        file.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
