import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
VALIDATOR = REPO_ROOT / "Scripts" / "CI" / "validate_release_artifact.py"


def create_windows_artifact(path: Path, include_executable: bool = True) -> None:
    with tempfile.TemporaryDirectory(prefix="life-validator-src-") as temp_dir:
        root = Path(temp_dir) / "Runtime"
        shader_dir = root / "Assets" / "Shaders"
        shader_dir.mkdir(parents=True)
        if include_executable:
            (root / "Runtime.exe").write_text("synthetic executable", encoding="utf-8")
        (root / "SDL3.dll").write_text("synthetic sdl", encoding="utf-8")
        (shader_dir / "Renderer2D.vert.spv").write_bytes(b"spv")

        with zipfile.ZipFile(path, "w") as archive:
            for item in root.rglob("*"):
                archive.write(item, item.relative_to(root.parent))


def run_validator(artifact: Path) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [
            sys.executable,
            str(VALIDATOR),
            "--artifact",
            str(artifact),
            "--platform",
            "windows",
            "--app",
            "Runtime",
            "--write-sha256",
        ],
        cwd=REPO_ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )


def main() -> int:
    with tempfile.TemporaryDirectory(prefix="life-validator-test-") as temp_dir:
        temp_root = Path(temp_dir)

        valid_artifact = temp_root / "valid.zip"
        create_windows_artifact(valid_artifact)
        valid_result = run_validator(valid_artifact)
        sys.stdout.write(valid_result.stdout)
        if valid_result.returncode != 0:
            return valid_result.returncode
        if not (temp_root / "valid.zip.sha256").is_file():
            print("Expected checksum file was not written.")
            return 1

        invalid_artifact = temp_root / "invalid.zip"
        create_windows_artifact(invalid_artifact, include_executable=False)
        invalid_result = run_validator(invalid_artifact)
        if invalid_result.returncode == 0:
            sys.stdout.write(invalid_result.stdout)
            print("Validator unexpectedly accepted an artifact without Runtime.exe.")
            return 1
        if "Missing executable" not in invalid_result.stdout:
            sys.stdout.write(invalid_result.stdout)
            print("Validator failed for an unexpected reason.")
            return 1

    print("[CI] Release artifact validator self-test passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
