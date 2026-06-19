import json
from pathlib import Path


DEPENDENCIES = [
    ("SDL3", "Vendor/SDL3", ["LICENSE.txt", "LICENSE.md", "LICENSE"]),
    ("ImGui", "Vendor/imgui", ["LICENSE.txt", "LICENSE"]),
    ("ImGuizmo", "Vendor/ImGuizmo", ["LICENSE", "LICENSE.txt"]),
    ("spdlog", "Vendor/spdlog", ["LICENSE"]),
    ("doctest", "Vendor/doctest", ["LICENSE.txt"]),
    ("nlohmann_json", "Vendor/json", ["LICENSE.MIT", "LICENSE"]),
    ("glm", "Vendor/glm", ["copying.txt", "license.md", "LICENSE"]),
    ("NVRHI", "Vendor/nvrhi", ["LICENSE", "LICENSE.txt"]),
    ("vk-bootstrap", "Vendor/vk-bootstrap", ["LICENSE.txt", "LICENSE"]),
    ("stb_image", "Vendor/stb_image", ["LICENSE", "LICENSE.txt"]),
]


def read_git_revision(path: Path) -> str:
    git_file = path / ".git"
    if git_file.is_file():
        content = git_file.read_text(encoding="utf-8", errors="replace").strip()
        if content.startswith("gitdir:"):
            git_dir = (path / content.split(":", 1)[1].strip()).resolve()
            head = git_dir / "HEAD"
            if head.is_file():
                head_value = head.read_text(encoding="utf-8", errors="replace").strip()
                if head_value.startswith("ref:"):
                    ref = git_dir / head_value.split(":", 1)[1].strip()
                    if ref.is_file():
                        return ref.read_text(encoding="utf-8", errors="replace").strip()
                return head_value
    return "unknown"


def find_license(path: Path, candidates: list[str]) -> str:
    for candidate in candidates:
        license_path = path / candidate
        if license_path.is_file():
            return str(license_path.as_posix())
    return ""


def main() -> int:
    repo_root = Path.cwd()
    inventory = []
    for name, relative_path, license_candidates in DEPENDENCIES:
        dependency_path = repo_root / relative_path
        inventory.append(
            {
                "name": name,
                "path": relative_path,
                "present": dependency_path.exists(),
                "revision": read_git_revision(dependency_path) if dependency_path.exists() else "missing",
                "license": find_license(dependency_path, license_candidates) if dependency_path.exists() else "",
            }
        )

    output_dir = repo_root / "Build" / "Reports"
    output_dir.mkdir(parents=True, exist_ok=True)
    output_path = output_dir / "dependency-inventory.json"
    output_path.write_text(json.dumps({"dependencies": inventory}, indent=2) + "\n", encoding="utf-8")
    print(f"[CI] Wrote dependency inventory: {output_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
