#!/usr/bin/env sh
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT="$SCRIPT_DIR"
PREMAKE_VERSION="5.0.0-beta2"
CMAKE_VERSION="4.3.0"
cd "$REPO_ROOT"

PREMAKE_ACTION=${1:-}
if [ $# -gt 0 ]; then
    shift
fi
TARGET_ARCH=""
for premake_arg in "$@"; do
    case "$premake_arg" in
        --arch=*)
            TARGET_ARCH=${premake_arg#--arch=}
            ;;
    esac
done

normalize_target_architecture() {
    target_architecture=$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')
    case "$target_architecture" in
        amd64|x86_64)
            printf '%s' "x64"
            ;;
        arm64|aarch64)
            printf '%s' "arm64"
            ;;
        *)
            printf '%s' "$target_architecture"
            ;;
    esac
}

resolve_host_architecture() {
    normalize_target_architecture "$(uname -m)"
}

resolve_target_architecture() {
    if [ -n "$TARGET_ARCH" ]; then
        normalize_target_architecture "$TARGET_ARCH"
        return 0
    fi

    host_architecture=$(resolve_host_architecture)
    if [ "$host_architecture" = "arm64" ]; then
        printf '%s' "arm64"
        return 0
    fi

    printf '%s' "x64"
}

HOST_ARCH=$(resolve_host_architecture)
TARGET_ARCH=$(resolve_target_architecture)
if [ "$TARGET_ARCH" != "x64" ] && [ "$TARGET_ARCH" != "arm64" ]; then
    echo "[Setup] Unsupported target architecture: $TARGET_ARCH"
    exit 1
fi

if [ "$(uname -s)" = "Linux" ] && [ "$HOST_ARCH" != "$TARGET_ARCH" ]; then
    echo "[Setup] Linux cross-architecture generation is not configured in Setup.sh. Install/configure an explicit cross toolchain before targeting $TARGET_ARCH from $HOST_ARCH."
    exit 1
fi

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
            if [ "$HOST_ARCH" = "arm64" ]; then
                cmake_archive_name="cmake-${CMAKE_VERSION}-linux-aarch64.tar.gz"
                cmake_root_dir="cmake-${CMAKE_VERSION}-linux-aarch64"
            else
                cmake_archive_name="cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"
                cmake_root_dir="cmake-${CMAKE_VERSION}-linux-x86_64"
            fi
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

have_sdl_install_artifacts() {
    sdl_install_dir="$1"
    sdl_platform_name="$2"

    case "$sdl_platform_name" in
        Linux|linux)
            [ -d "$sdl_install_dir/lib" ] && find "$sdl_install_dir/lib" -maxdepth 1 \( -name 'libSDL3.so' -o -name 'libSDL3.so.*' \) | grep -q .
            ;;
        Darwin|macos)
            [ -f "$sdl_install_dir/lib/libSDL3.0.dylib" ]
            ;;
        *)
            return 1
            ;;
    esac
}

build_sdl() {
    case "$(uname -s)" in
        Darwin)
            sdl_platform="macos"
            if [ "$TARGET_ARCH" = "arm64" ]; then
                SDL_CMAKE_ARCHITECTURE="arm64"
            else
                SDL_CMAKE_ARCHITECTURE="x86_64"
            fi
            if [ "$PREMAKE_ACTION" = "xcode4" ]; then
                SDL_CMAKE_GENERATOR="Xcode"
            else
                SDL_CMAKE_GENERATOR="Unix Makefiles"
            fi
            ;;
        Linux)
            sdl_platform="linux"
            SDL_CMAKE_ARCHITECTURE=""
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
    sdl_build_dir="$REPO_ROOT/Vendor/SDL3/Build/$sdl_platform/$TARGET_ARCH/$sdl_config"
    sdl_install_dir="$REPO_ROOT/Vendor/SDL3/Install/$sdl_platform/$TARGET_ARCH/$sdl_config"

    if [ -f "$sdl_build_dir/CMakeCache.txt" ] && have_sdl_install_artifacts "$sdl_install_dir" "$sdl_platform"; then
        echo "[Setup] SDL3 ($sdl_config) is already available. Skipping build."
        return 0
    fi

    echo "[Setup] Building SDL3 ($sdl_config)..."
    if [ -n "$SDL_CMAKE_ARCHITECTURE" ]; then
        "$CMAKE_CMD" -S "$REPO_ROOT/Vendor/SDL3" -B "$sdl_build_dir" -G "$SDL_CMAKE_GENERATOR" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=ON -DCMAKE_BUILD_TYPE="$sdl_config" -DCMAKE_OSX_ARCHITECTURES="$SDL_CMAKE_ARCHITECTURE" -DCMAKE_INSTALL_PREFIX="$sdl_install_dir"
    else
        "$CMAKE_CMD" -S "$REPO_ROOT/Vendor/SDL3" -B "$sdl_build_dir" -G "$SDL_CMAKE_GENERATOR" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=ON -DCMAKE_BUILD_TYPE="$sdl_config" -DCMAKE_INSTALL_PREFIX="$sdl_install_dir"
    fi
    "$CMAKE_CMD" --build "$sdl_build_dir" --config "$sdl_config" --target install
}

resolve_premake() {
    if command -v premake5 >/dev/null 2>&1; then
        PREMAKE_CMD="premake5"
        return 0
    fi

    if [ "$(uname -s)" = "Linux" ] && [ "$HOST_ARCH" = "arm64" ]; then
        echo "[Setup] Premake was not found. Automatic Premake bootstrap is not configured for Linux arm64 yet; install premake5 manually and rerun Setup.sh."
        exit 1
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
"$PREMAKE_CMD" "$PREMAKE_ACTION" "$@"

echo "[Setup] Dependencies, SDL3, and project files are ready."
