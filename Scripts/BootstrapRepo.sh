#!/usr/bin/env sh
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)
cd "$REPO_ROOT"

echo "[Bootstrap] Registering repository dependencies..."
if ! git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "[Bootstrap] Initializing git repository..."
    git init
fi

ensure_submodule() {
    submodule_path="$1"
    submodule_url="$2"

    if [ -f .gitmodules ] && git config --file .gitmodules --get-regexp '^submodule\..*\.path$' 2>/dev/null | grep -F " $submodule_path" >/dev/null 2>&1; then
        if git submodule status -- "$submodule_path" >/dev/null 2>&1; then
            echo "[Bootstrap] $submodule_path already registered."
            return 0
        fi
    fi

    if [ -d "$submodule_path" ]; then
        if [ -z "$(ls -A "$submodule_path" 2>/dev/null)" ]; then
            rmdir "$submodule_path"
        else
            echo "[Bootstrap] $submodule_path already exists and will be reused."
            return 0
        fi
    fi

    echo "[Bootstrap] Adding $submodule_path..."
    git submodule add --force "$submodule_url" "$submodule_path"
}

ensure_submodule "Vendor/SDL3" "https://github.com/libsdl-org/SDL.git"
ensure_submodule "Vendor/spdlog" "https://github.com/gabime/spdlog.git"
ensure_submodule "Vendor/json" "https://github.com/nlohmann/json.git"
ensure_submodule "Vendor/doctest" "https://github.com/doctest/doctest.git"
ensure_submodule "Vendor/nvrhi" "https://github.com/NVIDIAGameWorks/nvrhi.git"
ensure_submodule "Vendor/vk-bootstrap" "https://github.com/charles-lunarg/vk-bootstrap.git"

echo "[Bootstrap] Syncing submodules..."
git submodule update --init --recursive

echo "[Bootstrap] Repository dependencies are ready."
