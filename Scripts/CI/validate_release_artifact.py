import argparse
import hashlib
import os
import subprocess
import sys
import tarfile
import tempfile
import zipfile
from pathlib import Path


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Validate a packaged Life application artifact.")
    parser.add_argument("--artifact", required=True, help="Path to .zip or .tar.gz artifact.")
    parser.add_argument("--platform", required=True, choices=["windows", "linux", "macos"])
    parser.add_argument("--app", default="Runtime", help="Application executable stem to validate.")
    parser.add_argument("--run-diagnostics", action="store_true", help="Run the packaged executable with --diagnostics.")
    parser.add_argument("--write-sha256", action="store_true", help="Write ARTIFACT.sha256 beside the artifact.")
    return parser.parse_args()


def extract_artifact(artifact: Path, destination: Path) -> None:
    extraction_root = destination.resolve()
    if artifact.suffix == ".zip":
        with zipfile.ZipFile(artifact) as archive:
            for member in archive.infolist():
                target = (destination / member.filename).resolve()
                if not target.is_relative_to(extraction_root):
                    raise ValueError(f"Archive member escapes extraction root: {member.filename}")
            archive.extractall(destination)
        return

    if artifact.name.endswith(".tar.gz"):
        with tarfile.open(artifact, "r:gz") as archive:
            for member in archive.getmembers():
                target = (destination / member.name).resolve()
                if not target.is_relative_to(extraction_root):
                    raise ValueError(f"Archive member escapes extraction root: {member.name}")
            archive.extractall(destination)
        return

    raise ValueError(f"Unsupported artifact format: {artifact}")


def resolve_package_root(extracted_root: Path, app: str) -> Path:
    direct = extracted_root / app
    if direct.is_dir():
        return direct

    matches = [path for path in extracted_root.iterdir() if path.is_dir()]
    if len(matches) == 1:
        return matches[0]

    raise FileNotFoundError(f"Unable to resolve package root under {extracted_root}.")


def executable_name(platform: str, app: str) -> str:
    return f"{app}.exe" if platform == "windows" else app


def validate_required_files(package_root: Path, platform: str, app: str) -> Path:
    executable = package_root / executable_name(platform, app)
    if not executable.is_file():
        raise FileNotFoundError(f"Missing executable: {executable}")

    if platform == "windows":
        required_dependency = package_root / "SDL3.dll"
        if not required_dependency.is_file():
            raise FileNotFoundError(f"Missing SDL runtime: {required_dependency}")
    elif platform == "linux":
        if not list(package_root.glob("libSDL3.so*")):
            raise FileNotFoundError(f"Missing SDL runtime in {package_root}.")
    elif platform == "macos":
        if not list(package_root.glob("libSDL3*.dylib")):
            raise FileNotFoundError(f"Missing SDL runtime in {package_root}.")

    shader_dir = package_root / "Assets" / "Shaders"
    if shader_dir.is_dir() and list(shader_dir.glob("*.spv")):
        print(f"[CI] Found compiled shader assets in {shader_dir}.")
    else:
        print(f"[CI] WARNING: compiled shader assets were not found in {shader_dir}.")

    return executable


def run_diagnostics(executable: Path, platform: str, package_root: Path) -> None:
    env = os.environ.copy()
    if platform == "windows":
        env["PATH"] = str(package_root) + os.pathsep + env.get("PATH", "")
    elif platform == "linux":
        env["LD_LIBRARY_PATH"] = str(package_root) + os.pathsep + env.get("LD_LIBRARY_PATH", "")
    elif platform == "macos":
        env["DYLD_LIBRARY_PATH"] = str(package_root) + os.pathsep + env.get("DYLD_LIBRARY_PATH", "")

    result = subprocess.run(
        [str(executable), "--diagnostics"],
        cwd=package_root,
        env=env,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=30,
        check=False,
    )
    sys.stdout.write(result.stdout)
    if result.returncode != 0:
        raise RuntimeError(f"{executable} --diagnostics exited with {result.returncode}.")
    for token in ("Version:", "Commit:", "Configuration:", "Platform:", "Architecture:"):
        if token not in result.stdout:
            raise RuntimeError(f"Diagnostics output missing required token: {token}")


def write_sha256(artifact: Path) -> Path:
    digest = hashlib.sha256()
    with artifact.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)

    checksum_path = artifact.with_name(artifact.name + ".sha256")
    checksum_path.write_text(f"{digest.hexdigest()}  {artifact.name}\n", encoding="utf-8")
    return checksum_path


def main() -> int:
    args = parse_arguments()
    artifact = Path(args.artifact).resolve()
    if not artifact.is_file():
        raise FileNotFoundError(f"Artifact not found: {artifact}")

    with tempfile.TemporaryDirectory(prefix="life-artifact-") as temp_dir:
        extracted_root = Path(temp_dir)
        extract_artifact(artifact, extracted_root)
        package_root = resolve_package_root(extracted_root, args.app)
        executable = validate_required_files(package_root, args.platform, args.app)
        if args.run_diagnostics:
            run_diagnostics(executable, args.platform, package_root)

    if args.write_sha256:
        checksum_path = write_sha256(artifact)
        print(f"[CI] Wrote checksum: {checksum_path}")

    print(f"[CI] Validated {artifact}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
