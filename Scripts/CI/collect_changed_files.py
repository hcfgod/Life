import argparse
import json
import re
import subprocess
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--event-name", default="push")
    parser.add_argument("--base-ref", default="")
    parser.add_argument("--base-sha", default="")
    parser.add_argument("--head-sha", default="")
    parser.add_argument("--pattern", required=True)
    parser.add_argument("--exclude-pattern", default="")
    parser.add_argument("--compile-commands", default="")
    parser.add_argument("--fallback-compile-db-pattern", default="")
    parser.add_argument("--output", required=True)
    return parser.parse_args()


def run_git(repo_root: Path, *arguments: str) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        ["git", *arguments],
        cwd=repo_root,
        text=True,
        capture_output=True,
        check=False,
    )


def git_commit_exists(repo_root: Path, sha: str) -> bool:
    if not sha:
        return False
    result = run_git(repo_root, "cat-file", "-e", f"{sha}^{{commit}}")
    return result.returncode == 0


def git_ref_exists(repo_root: Path, ref_name: str) -> bool:
    if not ref_name:
        return False
    result = run_git(repo_root, "rev-parse", "--verify", ref_name)
    return result.returncode == 0


def get_changed_files(arguments: argparse.Namespace, repo_root: Path) -> list[str]:
    event_name = arguments.event_name
    base_ref = arguments.base_ref
    base_sha = arguments.base_sha
    head_sha = arguments.head_sha

    candidates: list[str] = []

    if event_name == "pull_request" and base_ref and head_sha and git_commit_exists(repo_root, head_sha) and git_ref_exists(repo_root, f"origin/{base_ref}"):
        result = run_git(repo_root, "diff", "--name-only", "--diff-filter=ACMR", f"origin/{base_ref}...{head_sha}")
        if result.returncode == 0:
            candidates = result.stdout.splitlines()

    if not candidates and base_sha and head_sha and git_commit_exists(repo_root, base_sha) and git_commit_exists(repo_root, head_sha):
        result = run_git(repo_root, "diff", "--name-only", "--diff-filter=ACMR", base_sha, head_sha)
        if result.returncode == 0:
            candidates = result.stdout.splitlines()

    if not candidates and head_sha and git_commit_exists(repo_root, head_sha):
        parent_check = run_git(repo_root, "cat-file", "-e", f"{head_sha}^")
        if parent_check.returncode == 0:
            parent_sha = run_git(repo_root, "rev-parse", f"{head_sha}^").stdout.strip()
            if parent_sha:
                result = run_git(repo_root, "diff", "--name-only", "--diff-filter=ACMR", parent_sha, head_sha)
                if result.returncode == 0:
                    candidates = result.stdout.splitlines()

    if not candidates and head_sha and git_commit_exists(repo_root, head_sha):
        result = run_git(repo_root, "show", "--name-only", "--pretty=", "--diff-filter=ACMR", head_sha)
        if result.returncode == 0:
            candidates = result.stdout.splitlines()

    if not candidates:
        result = run_git(repo_root, "ls-files")
        if result.returncode == 0:
            candidates = result.stdout.splitlines()

    pattern = re.compile(arguments.pattern)
    filtered = sorted({path.replace("\\", "/") for path in candidates if pattern.search(path.replace("\\", "/"))})
    return filtered


def filter_by_compile_commands(paths: list[str], compile_commands_path: Path, repo_root: Path) -> list[str]:
    with compile_commands_path.open("r", encoding="utf-8") as file:
        compile_commands = json.load(file)

    compile_db_files: set[str] = set()
    for entry in compile_commands:
        raw_file = entry.get("file")
        raw_directory = entry.get("directory", ".")
        if not raw_file:
            continue

        directory = Path(raw_directory)
        if not directory.is_absolute():
            directory = (repo_root / directory).resolve()
        else:
            directory = directory.resolve()

        candidate = Path(raw_file)
        if not candidate.is_absolute():
            candidate = (directory / candidate).resolve()
        else:
            candidate = candidate.resolve()

        try:
            compile_db_files.add(candidate.relative_to(repo_root).as_posix())
        except ValueError:
            continue

    return [path for path in paths if path in compile_db_files]


def collect_compile_db_files(compile_commands_path: Path, repo_root: Path, pattern: str) -> list[str]:
    with compile_commands_path.open("r", encoding="utf-8") as file:
        compile_commands = json.load(file)

    compiled_pattern = re.compile(pattern)
    compile_db_files: set[str] = set()
    for entry in compile_commands:
        raw_file = entry.get("file")
        raw_directory = entry.get("directory", ".")
        if not raw_file:
            continue

        directory = Path(raw_directory)
        if not directory.is_absolute():
            directory = (repo_root / directory).resolve()
        else:
            directory = directory.resolve()

        candidate = Path(raw_file)
        if not candidate.is_absolute():
            candidate = (directory / candidate).resolve()
        else:
            candidate = candidate.resolve()

        try:
            relative_path = candidate.relative_to(repo_root).as_posix()
        except ValueError:
            continue

        if compiled_pattern.search(relative_path):
            compile_db_files.add(relative_path)

    return sorted(compile_db_files)


def main() -> int:
    arguments = parse_arguments()
    repo_root = Path(arguments.repo_root).resolve()
    output_path = Path(arguments.output).resolve()

    changed_files = get_changed_files(arguments, repo_root)

    if arguments.compile_commands:
        compile_commands_path = Path(arguments.compile_commands).resolve()
        if compile_commands_path.exists():
            changed_files = filter_by_compile_commands(changed_files, compile_commands_path, repo_root)
            if not changed_files and arguments.fallback_compile_db_pattern:
                changed_files = collect_compile_db_files(compile_commands_path, repo_root, arguments.fallback_compile_db_pattern)
        else:
            changed_files = []

    if arguments.exclude_pattern:
        exclude_pattern = re.compile(arguments.exclude_pattern)
        changed_files = [path for path in changed_files if not exclude_pattern.search(path)]

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(changed_files) + ("\n" if changed_files else ""), encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
