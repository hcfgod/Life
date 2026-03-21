import argparse
import concurrent.futures
import subprocess
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--compile-commands", required=True)
    parser.add_argument("--files", required=True)
    parser.add_argument("--clang-tidy-binary", default="clang-tidy")
    parser.add_argument("--header-filter", default="^(Engine|Runtime|Test)/")
    parser.add_argument("--jobs", type=int, default=0)
    parser.add_argument("--warnings-as-errors", default="*")
    return parser.parse_args()


def run_tidy(arguments: argparse.Namespace, repo_root: Path, source_file: str) -> tuple[str, int, str]:
    command = [
        arguments.clang_tidy_binary,
        "-p",
        str(Path(arguments.compile_commands).resolve().parent),
        f"--header-filter={arguments.header_filter}",
        f"--warnings-as-errors={arguments.warnings_as_errors}",
        source_file,
    ]

    result = subprocess.run(
        command,
        cwd=repo_root,
        text=True,
        capture_output=True,
        check=False,
    )

    output = "".join(part for part in [result.stdout, result.stderr] if part)
    return source_file, result.returncode, output


def main() -> int:
    arguments = parse_arguments()
    repo_root = Path(arguments.repo_root).resolve()
    files_path = Path(arguments.files).resolve()

    files = [line.strip() for line in files_path.read_text(encoding="utf-8").splitlines() if line.strip()]
    if not files:
        print("No files selected for clang-tidy.")
        return 0

    jobs = arguments.jobs if arguments.jobs and arguments.jobs > 0 else min(8, len(files))
    failures = []

    with concurrent.futures.ThreadPoolExecutor(max_workers=jobs) as executor:
        future_map = {
            executor.submit(run_tidy, arguments, repo_root, file_path): file_path
            for file_path in files
        }

        for future in concurrent.futures.as_completed(future_map):
            source_file, return_code, output = future.result()
            if output:
                print(output, end="" if output.endswith("\n") else "\n")
            if return_code != 0:
                failures.append(source_file)

    if failures:
        print("clang-tidy failed for:")
        for source_file in failures:
            print(source_file)
        return 1

    print(f"clang-tidy passed for {len(files)} file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
