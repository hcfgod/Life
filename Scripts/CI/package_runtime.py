import argparse
import shutil
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-dir", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--format", default="zip", choices=["zip", "gztar"])
    return parser.parse_args()


def resolve_input_dir(input_dir_argument: str) -> Path:
    if any(token in input_dir_argument for token in ("*", "?", "[")):
        matches = sorted(Path().glob(input_dir_argument))
        directory_matches = [match.resolve() for match in matches if match.is_dir()]
        if len(directory_matches) != 1:
            raise ValueError(
                f"Expected exactly one directory match for pattern '{input_dir_argument}', found {len(directory_matches)}."
            )
        return directory_matches[0]

    return Path(input_dir_argument).resolve()


def main() -> int:
    arguments = parse_arguments()
    input_dir = resolve_input_dir(arguments.input_dir)
    output_path = Path(arguments.output).resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)

    archive_base = output_path
    suffix = ''.join(output_path.suffixes)
    if suffix:
        archive_base = output_path.with_suffix('')
        while archive_base.suffix:
            archive_base = archive_base.with_suffix('')

    archive_path = shutil.make_archive(
        str(archive_base),
        arguments.format,
        root_dir=str(input_dir.parent),
        base_dir=input_dir.name,
    )

    generated_path = Path(archive_path)
    if generated_path.resolve() != output_path:
        if output_path.exists():
            output_path.unlink()
        shutil.move(str(generated_path), str(output_path))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
