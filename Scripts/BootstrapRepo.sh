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

register_declared_submodules() {
    if [ ! -f .gitmodules ]; then
        echo "[Bootstrap] .gitmodules was not found. No dependencies to register."
        return 0
    fi

    submodules_found=0
    while read -r submodule_key submodule_path; do
        [ -n "$submodule_key" ] || continue
        submodules_found=1
        submodule_url_key=${submodule_key%.path}.url
        submodule_url=$(git config --file .gitmodules --get "$submodule_url_key" 2>/dev/null || true)

        if [ -z "$submodule_url" ]; then
            echo "[Bootstrap] Skipping $submodule_path because no submodule URL is declared."
            continue
        fi

        ensure_submodule "$submodule_path" "$submodule_url"
    done <<EOF
$(git config --file .gitmodules --get-regexp '^submodule\..*\.path$' 2>/dev/null || true)
EOF

    if [ "$submodules_found" -eq 0 ]; then
        echo "[Bootstrap] No submodules were declared in .gitmodules."
    fi
}

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

register_declared_submodules

echo "[Bootstrap] Syncing submodules..."
git submodule update --init --recursive

echo "[Bootstrap] Repository dependencies are ready."
