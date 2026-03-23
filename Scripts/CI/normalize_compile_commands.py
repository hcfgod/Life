import argparse
import json
import shlex
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


def is_pch_path(value: str) -> bool:
    normalized_value = value.replace("\\", "/")
    return normalized_value.endswith("PCH.h") or normalized_value.endswith(".gch") or normalized_value.endswith(".pch")


def sanitize_arguments(arguments: list[object]) -> list[object]:
    sanitized: list[object] = []
    index = 0

    while index < len(arguments):
        argument = arguments[index]

        if not isinstance(argument, str):
            sanitized.append(argument)
            index += 1
            continue

        next_argument = arguments[index + 1] if index + 1 < len(arguments) else None

        if argument in {"-Winvalid-pch", "-fpch-preprocess"}:
            index += 1
            continue

        if argument in {"-include-pch", "/Yu", "/Fp"}:
            index += 2 if next_argument is not None else 1
            continue

        if argument in {"-include", "/FI"}:
            if isinstance(next_argument, str) and is_pch_path(next_argument):
                index += 2
                continue

            sanitized.append(argument)
            index += 1
            continue

        if argument.startswith("-include-pch") and argument != "-include-pch":
            index += 1
            continue

        if argument.startswith("-include") and argument != "-include":
            include_target = argument[len("-include"):]
            if include_target.startswith("="):
                include_target = include_target[1:]
            if is_pch_path(include_target):
                index += 1
                continue

        if argument.startswith("/FI") and argument != "/FI":
            include_target = argument[len("/FI"):]
            if is_pch_path(include_target):
                index += 1
                continue

        if argument.startswith("/Yu") and argument != "/Yu":
            index += 1
            continue

        if argument.startswith("/Fp") and argument != "/Fp":
            index += 1
            continue

        if is_pch_path(argument):
            index += 1
            continue

        sanitized.append(argument)
        index += 1

    return sanitized


def rewrite_command(command: str, original_file: str, resolved_file: str) -> str:
    command_arguments = shlex.split(command)
    rewritten_arguments = rewrite_arguments(command_arguments, original_file, resolved_file)
    sanitized_arguments = sanitize_arguments(rewritten_arguments)
    return shlex.join(str(argument) for argument in sanitized_arguments)


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
            rewritten_arguments = rewrite_arguments(entry["arguments"], raw_file, str(resolved_file))
            entry["arguments"] = sanitize_arguments(rewritten_arguments)

        if isinstance(entry.get("command"), str):
            entry["command"] = rewrite_command(entry["command"], raw_file, str(resolved_file))

    with compile_commands_path.open("w", encoding="utf-8") as file:
        json.dump(compile_commands, file, indent=2)
        file.write("\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
