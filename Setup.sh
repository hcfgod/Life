#!/usr/bin/env sh
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT="$SCRIPT_DIR"
PREMAKE_VERSION="5.0.0-beta2"
CMAKE_VERSION="4.3.0"
cd "$REPO_ROOT"

PREMAKE_ACTION=${1:-}
if [ -z "$PREMAKE_ACTION" ]; then
    case "$(uname -s)" in
        Darwin)
            PREMAKE_ACTION="xcode4"
            ;;
        *)
            PREMAKE_ACTION="gmake2"
            ;;
    esac
fi

needs_bootstrap=0
if [ ! -f .gitmodules ]; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && ! grep -F "path = Vendor/SDL3" .gitmodules >/dev/null 2>&1; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && ! grep -F "path = Vendor/spdlog" .gitmodules >/dev/null 2>&1; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && ! grep -F "path = Vendor/json" .gitmodules >/dev/null 2>&1; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && ! grep -F "path = Vendor/doctest" .gitmodules >/dev/null 2>&1; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && [ ! -d "Vendor/SDL3" -o -z "$(ls -A "Vendor/SDL3" 2>/dev/null)" ]; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && [ ! -d "Vendor/spdlog" -o -z "$(ls -A "Vendor/spdlog" 2>/dev/null)" ]; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && [ ! -d "Vendor/json" -o -z "$(ls -A "Vendor/json" 2>/dev/null)" ]; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 0 ] && [ ! -d "Vendor/doctest" -o -z "$(ls -A "Vendor/doctest" 2>/dev/null)" ]; then
    needs_bootstrap=1
fi

if [ "$needs_bootstrap" -eq 1 ]; then
    echo "[Setup] Bootstrap state not found. Running bootstrap..."
    sh "$REPO_ROOT/Scripts/BootstrapRepo.sh"
fi

echo "[Setup] Initializing git submodules..."
git submodule init

echo "[Setup] Updating submodules recursively..."
git submodule update --init --recursive

resolve_cmake() {
    if command -v cmake >/dev/null 2>&1; then
        CMAKE_CMD="cmake"
        return 0
    fi

    case "$(uname -s)" in
        Darwin)
            cmake_platform="macos"
            cmake_archive_name="cmake-${CMAKE_VERSION}-macos-universal.tar.gz"
            cmake_root_dir="cmake-${CMAKE_VERSION}-macos-universal/CMake.app/Contents"
            ;;
        Linux)
            cmake_platform="linux"
            cmake_archive_name="cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
            cmake_root_dir="cmake-${CMAKE_VERSION}-linux-x86_64"
            ;;
        *)
            echo "[Setup] Unsupported platform for CMake download."
            exit 1
            ;;
    esac

    cmake_dir="$REPO_ROOT/Scripts/CMake/$cmake_platform"
    cmake_archive="$cmake_dir/$cmake_archive_name"
    cmake_url="https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/${cmake_archive_name}"
    cmake_bin="$cmake_dir/$cmake_root_dir/bin/cmake"

    if [ -f "$cmake_bin" ]; then
        chmod +x "$cmake_bin"
        CMAKE_CMD="$cmake_bin"
        return 0
    fi

    echo "[Setup] CMake was not found. Downloading CMake $CMAKE_VERSION for $cmake_platform..."
    mkdir -p "$cmake_dir"

    if command -v curl >/dev/null 2>&1; then
        curl -L --fail -o "$cmake_archive" "$cmake_url"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$cmake_archive" "$cmake_url"
    else
        echo "[Setup] Neither curl nor wget is available to download CMake."
        exit 1
    fi

    tar -xzf "$cmake_archive" -C "$cmake_dir"
    rm -f "$cmake_archive"
    chmod +x "$cmake_bin"
    CMAKE_CMD="$cmake_bin"
    return 0

    echo "[Setup] CMake download failed."
    exit 1
}

build_sdl() {
    case "$(uname -s)" in
        Darwin)
            sdl_platform="macos"
            if [ "$PREMAKE_ACTION" = "xcode4" ]; then
                SDL_CMAKE_GENERATOR="Xcode"
            else
                SDL_CMAKE_GENERATOR="Unix Makefiles"
            fi
            ;;
        Linux)
            sdl_platform="linux"
            SDL_CMAKE_GENERATOR="Unix Makefiles"
            ;;
        *)
            echo "[Setup] Unsupported platform for SDL3 build."
            exit 1
            ;;
    esac

    build_sdl_config Debug
    build_sdl_config Release
}

build_sdl_config() {
    sdl_config="$1"
    sdl_build_dir="$REPO_ROOT/Vendor/SDL3/Build/$sdl_platform/x64/$sdl_config"
    sdl_install_dir="$REPO_ROOT/Vendor/SDL3/Install/$sdl_platform/x64/$sdl_config"

    echo "[Setup] Building SDL3 ($sdl_config)..."
    "$CMAKE_CMD" -S "$REPO_ROOT/Vendor/SDL3" -B "$sdl_build_dir" -G "$SDL_CMAKE_GENERATOR" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=ON -DCMAKE_BUILD_TYPE="$sdl_config" -DCMAKE_INSTALL_PREFIX="$sdl_install_dir"
    "$CMAKE_CMD" --build "$sdl_build_dir" --config "$sdl_config" --target install
}

resolve_premake() {
    if command -v premake5 >/dev/null 2>&1; then
        PREMAKE_CMD="premake5"
        return 0
    fi

    case "$(uname -s)" in
        Darwin)
            premake_platform="macosx"
            premake_archive_name="premake-${PREMAKE_VERSION}-macosx.tar.gz"
            ;;
        Linux)
            premake_platform="linux"
            premake_archive_name="premake-${PREMAKE_VERSION}-linux.tar.gz"
            ;;
        *)
            echo "[Setup] Unsupported platform for Premake download."
            exit 1
            ;;
    esac

    premake_dir="$REPO_ROOT/Scripts/Premake/$premake_platform"
    premake_bin="$premake_dir/premake5"

    if [ -f "$premake_bin" ]; then
        chmod +x "$premake_bin"
        PREMAKE_CMD="$premake_bin"
        return 0
    fi

    premake_archive="$premake_dir/$premake_archive_name"
    premake_url="https://github.com/premake/premake-core/releases/download/v${PREMAKE_VERSION}/${premake_archive_name}"

    echo "[Setup] Premake was not found. Downloading Premake $PREMAKE_VERSION for $premake_platform..."
    mkdir -p "$premake_dir"

    if command -v curl >/dev/null 2>&1; then
        curl -L --fail -o "$premake_archive" "$premake_url"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$premake_archive" "$premake_url"
    else
        echo "[Setup] Neither curl nor wget is available to download Premake."
        exit 1
    fi

    tar -xzf "$premake_archive" -C "$premake_dir"
    rm -f "$premake_archive"
    chmod +x "$premake_bin"

    PREMAKE_CMD="$premake_bin"
}

resolve_cmake
build_sdl
resolve_premake

echo "[Setup] Generating project files with Premake ($PREMAKE_ACTION)..."
"$PREMAKE_CMD" "$PREMAKE_ACTION"

echo "[Setup] Dependencies, SDL3, and project files are ready."
