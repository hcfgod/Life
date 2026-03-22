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

LINUX_CROSS_COMPILE=0
LINUX_CROSS_PREFIX=${LIFE_LINUX_CROSS_PREFIX:-}
LINUX_SYSROOT=${LIFE_LINUX_SYSROOT:-}
LINUX_CMAKE_TOOLCHAIN_FILE=${LIFE_LINUX_CMAKE_TOOLCHAIN_FILE:-}
LINUX_CMAKE_SYSTEM_PROCESSOR=""
LINUX_CC=""
LINUX_CXX=""
LINUX_AR=""
LINUX_RANLIB=""

resolve_linux_target_processor() {
    case "$TARGET_ARCH" in
        arm64)
            printf '%s' "aarch64"
            ;;
        *)
            printf '%s' "x86_64"
            ;;
    esac
}

resolve_linux_cross_prefix() {
    case "$TARGET_ARCH" in
        arm64)
            printf '%s' "aarch64-linux-gnu"
            ;;
        *)
            printf '%s' "x86_64-linux-gnu"
            ;;
    esac
}

configure_linux_cross_toolchain() {
    if [ "$(uname -s)" != "Linux" ] || [ "$HOST_ARCH" = "$TARGET_ARCH" ]; then
        return 0
    fi

    if [ -z "$LINUX_CROSS_PREFIX" ]; then
        LINUX_CROSS_PREFIX=$(resolve_linux_cross_prefix)
    fi

    if [ -n "${CC:-}" ]; then
        LINUX_CC=$CC
    else
        LINUX_CC="${LINUX_CROSS_PREFIX}-gcc"
    fi

    if [ -n "${CXX:-}" ]; then
        LINUX_CXX=$CXX
    else
        LINUX_CXX="${LINUX_CROSS_PREFIX}-g++"
    fi

    if [ -n "${AR:-}" ]; then
        LINUX_AR=$AR
    else
        LINUX_AR="${LINUX_CROSS_PREFIX}-ar"
    fi

    if [ -n "${RANLIB:-}" ]; then
        LINUX_RANLIB=$RANLIB
    else
        LINUX_RANLIB="${LINUX_CROSS_PREFIX}-ranlib"
    fi

    for required_tool in "$LINUX_CC" "$LINUX_CXX" "$LINUX_AR" "$LINUX_RANLIB"; do
        if ! command -v "$required_tool" >/dev/null 2>&1; then
            echo "[Setup] Linux cross-compilation requires '$required_tool'. Install the target toolchain or set CC/CXX/AR/RANLIB (or LIFE_LINUX_CROSS_PREFIX) explicitly."
            exit 1
        fi
    done

    if [ -n "$LINUX_CMAKE_TOOLCHAIN_FILE" ] && [ ! -f "$LINUX_CMAKE_TOOLCHAIN_FILE" ]; then
        echo "[Setup] Linux CMake toolchain file was not found: $LINUX_CMAKE_TOOLCHAIN_FILE"
        exit 1
    fi

    if [ -n "$LINUX_SYSROOT" ] && [ ! -e "$LINUX_SYSROOT" ]; then
        echo "[Setup] Linux sysroot path was not found: $LINUX_SYSROOT"
        exit 1
    fi

    LINUX_CROSS_COMPILE=1
    LINUX_CMAKE_SYSTEM_PROCESSOR=$(resolve_linux_target_processor)
}

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
    set -- "$CMAKE_CMD" -S "$REPO_ROOT/Vendor/SDL3" -B "$sdl_build_dir" -G "$SDL_CMAKE_GENERATOR" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST_LIBRARY=OFF -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF -DSDL_INSTALL=ON -DCMAKE_BUILD_TYPE="$sdl_config" -DCMAKE_INSTALL_PREFIX="$sdl_install_dir"

    if [ -n "$SDL_CMAKE_ARCHITECTURE" ]; then
        set -- "$@" -DCMAKE_OSX_ARCHITECTURES="$SDL_CMAKE_ARCHITECTURE"
    fi

    if [ "$sdl_platform" = "linux" ] && [ "$LINUX_CROSS_COMPILE" -eq 1 ]; then
        if [ -n "$LINUX_CMAKE_TOOLCHAIN_FILE" ]; then
            set -- "$@" -DCMAKE_TOOLCHAIN_FILE="$LINUX_CMAKE_TOOLCHAIN_FILE"
        else
            set -- "$@" -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR="$LINUX_CMAKE_SYSTEM_PROCESSOR" -DCMAKE_C_COMPILER="$LINUX_CC" -DCMAKE_CXX_COMPILER="$LINUX_CXX" -DCMAKE_AR="$LINUX_AR" -DCMAKE_RANLIB="$LINUX_RANLIB"
        fi

        if [ -n "$LINUX_SYSROOT" ]; then
            set -- "$@" -DCMAKE_SYSROOT="$LINUX_SYSROOT"
        fi
    fi

    "$@"
    "$CMAKE_CMD" --build "$sdl_build_dir" --config "$sdl_config" --target install
}

bootstrap_premake_from_source() {
    premake_dir="$1"
    premake_bin="$2"
    premake_archive="$premake_dir/premake-${PREMAKE_VERSION}-source.tar.gz"
    premake_source_root="$premake_dir/source"
    premake_source_url="https://github.com/premake/premake-core/archive/refs/tags/v${PREMAKE_VERSION}.tar.gz"
    premake_bootstrap_cc=${LIFE_PREMAKE_BOOTSTRAP_CC:-}

    if ! command -v make >/dev/null 2>&1; then
        echo "[Setup] make is required to bootstrap Premake from source on Linux $HOST_ARCH."
        exit 1
    fi

    if [ -z "$premake_bootstrap_cc" ]; then
        if command -v cc >/dev/null 2>&1; then
            premake_bootstrap_cc="cc"
        elif command -v gcc >/dev/null 2>&1; then
            premake_bootstrap_cc="gcc"
        elif command -v clang >/dev/null 2>&1; then
            premake_bootstrap_cc="clang"
        else
            echo "[Setup] Unable to find a host C compiler for Premake bootstrap. Set LIFE_PREMAKE_BOOTSTRAP_CC to a native compiler and rerun Setup.sh."
            exit 1
        fi
    fi

    if ! command -v "$premake_bootstrap_cc" >/dev/null 2>&1; then
        echo "[Setup] Host compiler '$premake_bootstrap_cc' for Premake bootstrap was not found."
        exit 1
    fi

    echo "[Setup] Premake was not found. Bootstrapping Premake $PREMAKE_VERSION from source for Linux $HOST_ARCH..."
    mkdir -p "$premake_dir"
    rm -rf "$premake_source_root"

    if command -v curl >/dev/null 2>&1; then
        curl -L --fail -o "$premake_archive" "$premake_source_url"
    elif command -v wget >/dev/null 2>&1; then
        wget -O "$premake_archive" "$premake_source_url"
    else
        echo "[Setup] Neither curl nor wget is available to download Premake source."
        exit 1
    fi

    mkdir -p "$premake_source_root"
    tar -xzf "$premake_archive" -C "$premake_source_root"
    rm -f "$premake_archive"

    premake_source_dir=$(find "$premake_source_root" -mindepth 1 -maxdepth 1 -type d | head -n 1)
    if [ -z "$premake_source_dir" ]; then
        echo "[Setup] Failed to extract Premake source archive."
        exit 1
    fi

    (
        cd "$premake_source_dir"
        make -f Bootstrap.mak linux CC="$premake_bootstrap_cc"
    )

    built_premake_bin=""
    for candidate in \
        "$premake_source_dir/bin/release/premake5" \
        "$premake_source_dir/bin/debug/premake5" \
        "$premake_source_dir/build/bootstrap/bin/release/premake5" \
        "$premake_source_dir/build/bootstrap/bin/debug/premake5"
    do
        if [ -f "$candidate" ]; then
            built_premake_bin="$candidate"
            break
        fi
    done

    if [ -z "$built_premake_bin" ]; then
        built_premake_bin=$(find "$premake_source_dir" -type f -name premake5 | head -n 1)
    fi

    if [ -z "$built_premake_bin" ] || [ ! -f "$built_premake_bin" ]; then
        echo "[Setup] Premake source bootstrap completed but no premake5 binary was found."
        exit 1
    fi

    cp "$built_premake_bin" "$premake_bin"
    chmod +x "$premake_bin"
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

    if [ "$premake_platform" = "linux" ] && [ "$HOST_ARCH" = "arm64" ]; then
        premake_dir="$REPO_ROOT/Scripts/Premake/$premake_platform/$HOST_ARCH"
        premake_bin="$premake_dir/premake5"

        if [ -f "$premake_bin" ]; then
            chmod +x "$premake_bin"
            PREMAKE_CMD="$premake_bin"
            return 0
        fi

        bootstrap_premake_from_source "$premake_dir" "$premake_bin"
        PREMAKE_CMD="$premake_bin"
        return 0
    fi

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

resolve_premake
resolve_cmake
configure_linux_cross_toolchain
build_sdl

echo "[Setup] Generating project files with Premake ($PREMAKE_ACTION)..."
"$PREMAKE_CMD" "$PREMAKE_ACTION" "$@"

echo "[Setup] Dependencies, SDL3, and project files are ready."
