#!/usr/bin/env sh
set -e

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT="$SCRIPT_DIR"
PREMAKE_VERSION="5.0.0-beta2"
CMAKE_VERSION="4.3.0"
VULKAN_SDK_VERSION="1.4.304.1"
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

evaluate_declared_submodules() {
    found_declared_submodule=0
    while read -r submodule_key submodule_path; do
        [ -n "$submodule_key" ] || continue
        found_declared_submodule=1

        if ! git submodule status -- "$submodule_path" >/dev/null 2>&1; then
            needs_bootstrap=1
            continue
        fi

        if [ ! -d "$submodule_path" ] || [ -z "$(ls -A "$submodule_path" 2>/dev/null)" ]; then
            needs_bootstrap=1
        fi
    done <<EOF
$(git config --file .gitmodules --get-regexp '^submodule\..*\.path$' 2>/dev/null || true)
EOF

    if [ "$found_declared_submodule" -eq 0 ]; then
        needs_bootstrap=1
    fi
}

if [ "$needs_bootstrap" -eq 0 ]; then
    evaluate_declared_submodules
fi

if [ "$needs_bootstrap" -eq 1 ]; then
    echo "[Setup] Bootstrap state not found. Running bootstrap..."
    sh "$REPO_ROOT/Scripts/BootstrapRepo.sh"
fi

echo "[Setup] Initializing git submodules..."
git submodule init

echo "[Setup] Updating submodules recursively..."
git submodule update --init --recursive

ensure_vk_bootstrap_premake() {
    if [ ! -d "$REPO_ROOT/Vendor/vk-bootstrap" ]; then
        echo "[Setup] Vendor/vk-bootstrap was not found after submodule sync."
        exit 1
    fi

    cat > "$REPO_ROOT/Vendor/vk-bootstrap/premake5.lua" <<'EOF'
project "VkBootstrap"
    location "."
    kind "StaticLib"

    SetupProject()

    files
    {
        "src/VkBootstrap.h",
        "src/VkBootstrap.cpp",
        "src/VkBootstrapDispatch.h",
        "src/VkBootstrapFeatureChain.h",
        "src/VkBootstrapFeatureChain.inl"
    }

    includedirs
    {
        "src"
    }

    externalincludedirs
    {
        IncludeDir["VulkanHeaders"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
EOF
}

ensure_vk_bootstrap_premake

ensure_imgui_premake() {
    if [ ! -d "$REPO_ROOT/Vendor/imgui" ]; then
        echo "[Setup] Vendor/imgui was not found after submodule sync."
        exit 1
    fi

    cat > "$REPO_ROOT/Vendor/imgui/premake5.lua" <<'EOF'
project "ImGui"
    location "."
    kind "StaticLib"

    SetupProject()

    files
    {
        "imgui.h",
        "imgui_internal.h",
        "imconfig.h",
        "imstb_rectpack.h",
        "imstb_textedit.h",
        "imstb_truetype.h",
        "imgui.cpp",
        "imgui_draw.cpp",
        "imgui_tables.cpp",
        "imgui_widgets.cpp",
        "backends/imgui_impl_sdl3.h",
        "backends/imgui_impl_sdl3.cpp",
        "backends/imgui_impl_vulkan.h",
        "backends/imgui_impl_vulkan.cpp"
    }

    includedirs
    {
        ".",
        "backends"
    }

    externalincludedirs
    {
        IncludeDir["SDL3"],
        IncludeDir["VulkanHeaders"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
EOF
}

ensure_imgui_premake

ensure_stb_image_premake() {
    if [ ! -d "$REPO_ROOT/Vendor/stb_image" ]; then
        echo "[Setup] Vendor/stb_image was not found."
        exit 1
    fi

    cat > "$REPO_ROOT/Vendor/stb_image/premake5.lua" <<'EOF'
project "StbImage"
    location "."
    kind "StaticLib"

    SetupProject()

    files
    {
        "stb_image.h",
        "stb_image_source.h",
        "stb_image_impl.cpp"
    }

    includedirs
    {
        "."
    }

    externalincludedirs
    {
        IncludeDir["SDL3"]
    }

    ConfigureSanitizers()
    ConfigureCommonProject()
EOF
}

ensure_stb_image_premake

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

resolve_vulkan_sdk() {
    LIFE_VULKAN_SDK=""

    case "$(uname -s)" in
        Darwin)
            vulkan_sdk_platform="mac"
            vulkan_sdk_local_dir="$REPO_ROOT/Vendor/VulkanSDK/$VULKAN_SDK_VERSION/macOS"
            vulkan_sdk_lib_check="lib/libvulkan.1.dylib"
            ;;
        Linux)
            vulkan_sdk_platform="linux"
            vulkan_sdk_local_dir="$REPO_ROOT/Vendor/VulkanSDK/$VULKAN_SDK_VERSION/x86_64"
            vulkan_sdk_lib_check="lib/libvulkan.so.1"
            ;;
        *)
            echo "[Setup] Unsupported platform for Vulkan SDK."
            exit 1
            ;;
    esac

    # Check VULKAN_SDK environment variable first
    if [ -n "${VULKAN_SDK:-}" ] && [ -d "$VULKAN_SDK" ]; then
        LIFE_VULKAN_SDK="$VULKAN_SDK"
        echo "[Setup] Vulkan SDK found via VULKAN_SDK at $VULKAN_SDK"
        return 0
    fi

    # Check system-installed Vulkan on Linux (from apt libvulkan-dev)
    if [ "$(uname -s)" = "Linux" ]; then
        if [ -f "/usr/lib/x86_64-linux-gnu/libvulkan.so" ] || [ -f "/usr/lib/aarch64-linux-gnu/libvulkan.so" ]; then
            LIFE_VULKAN_SDK="system"
            echo "[Setup] Vulkan SDK found via system packages."
            return 0
        fi
    fi

    # Check local vendor copy
    if [ -d "$vulkan_sdk_local_dir" ]; then
        LIFE_VULKAN_SDK="$vulkan_sdk_local_dir"
        echo "[Setup] Vulkan SDK found at $vulkan_sdk_local_dir"
        return 0
    fi

    # Download the SDK
    vulkan_sdk_archive_dir="$REPO_ROOT/Vendor/VulkanSDK"
    mkdir -p "$vulkan_sdk_archive_dir"

    if [ "$vulkan_sdk_platform" = "linux" ]; then
        vulkan_sdk_archive="$vulkan_sdk_archive_dir/vulkan_sdk_${VULKAN_SDK_VERSION}.tar.xz"
        vulkan_sdk_url="https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/linux/vulkan_sdk.tar.xz"

        echo "[Setup] Vulkan SDK not found. Downloading Vulkan SDK $VULKAN_SDK_VERSION for Linux..."
        if command -v curl >/dev/null 2>&1; then
            curl -L --fail -o "$vulkan_sdk_archive" "$vulkan_sdk_url"
        elif command -v wget >/dev/null 2>&1; then
            wget -O "$vulkan_sdk_archive" "$vulkan_sdk_url"
        else
            echo "[Setup] Neither curl nor wget is available to download Vulkan SDK."
            exit 1
        fi

        echo "[Setup] Extracting Vulkan SDK..."
        tar -xJf "$vulkan_sdk_archive" -C "$vulkan_sdk_archive_dir"
        rm -f "$vulkan_sdk_archive"
        LIFE_VULKAN_SDK="$vulkan_sdk_local_dir"

    elif [ "$vulkan_sdk_platform" = "mac" ]; then
        vulkan_sdk_archive="$vulkan_sdk_archive_dir/vulkan_sdk_${VULKAN_SDK_VERSION}.zip"
        vulkan_sdk_url="https://sdk.lunarg.com/sdk/download/${VULKAN_SDK_VERSION}/mac/vulkan_sdk.zip"
        vulkan_sdk_extract_dir="$vulkan_sdk_archive_dir/vulkansdk-macOS-${VULKAN_SDK_VERSION}-extract"
        vulkan_sdk_install_root="$REPO_ROOT/Vendor/VulkanSDK/$VULKAN_SDK_VERSION"

        echo "[Setup] Vulkan SDK not found. Downloading Vulkan SDK $VULKAN_SDK_VERSION for macOS..."
        if command -v curl >/dev/null 2>&1; then
            curl -L --fail -o "$vulkan_sdk_archive" "$vulkan_sdk_url"
        elif command -v wget >/dev/null 2>&1; then
            wget -O "$vulkan_sdk_archive" "$vulkan_sdk_url"
        else
            echo "[Setup] Neither curl nor wget is available to download Vulkan SDK."
            exit 1
        fi

        echo "[Setup] Extracting Vulkan SDK..."
        rm -rf "$vulkan_sdk_extract_dir"
        mkdir -p "$vulkan_sdk_extract_dir"
        unzip -q -o "$vulkan_sdk_archive" -d "$vulkan_sdk_extract_dir"
        rm -f "$vulkan_sdk_archive"

        vulkan_sdk_installer_app=$(find "$vulkan_sdk_extract_dir" -type d -name "InstallVulkan.app" | head -n 1)
        if [ -z "$vulkan_sdk_installer_app" ]; then
            vulkan_sdk_installer_app=$(find "$vulkan_sdk_extract_dir" -type d -name "*.app" | head -n 1)
        fi
        if [ -z "$vulkan_sdk_installer_app" ]; then
            echo "[Setup] Unable to locate the extracted macOS Vulkan SDK installer app."
            exit 1
        fi

        vulkan_sdk_installer_bin="$vulkan_sdk_installer_app/Contents/MacOS/InstallVulkan"
        if [ ! -f "$vulkan_sdk_installer_bin" ]; then
            vulkan_sdk_installer_bin=$(find "$vulkan_sdk_installer_app/Contents/MacOS" -maxdepth 1 -type f -perm -111 | head -n 1)
        fi
        if [ ! -f "$vulkan_sdk_installer_bin" ]; then
            echo "[Setup] Unable to locate the macOS Vulkan SDK installer binary."
            exit 1
        fi

        mkdir -p "$vulkan_sdk_install_root"
        "$vulkan_sdk_installer_bin" --root "$vulkan_sdk_install_root" --accept-licenses --default-answer --confirm-command install copy_only=1

        rm -rf "$vulkan_sdk_extract_dir"
        LIFE_VULKAN_SDK="$vulkan_sdk_local_dir"
    fi

    if [ -n "$LIFE_VULKAN_SDK" ] && [ -d "$LIFE_VULKAN_SDK" ]; then
        echo "[Setup] Vulkan SDK $VULKAN_SDK_VERSION installed locally at $LIFE_VULKAN_SDK"
        return 0
    fi

    echo "[Setup] Vulkan SDK installation failed. Please install manually from https://vulkan.lunarg.com/sdk/home"
    exit 1
}

build_nvrhi() {
    case "$(uname -s)" in
        Darwin)
            nvrhi_platform="macos"
            NVRHI_CMAKE_DX12_FLAG="-DNVRHI_WITH_DX12=OFF"
            if [ "$PREMAKE_ACTION" = "xcode4" ]; then
                NVRHI_CMAKE_GENERATOR="Xcode"
            else
                NVRHI_CMAKE_GENERATOR="Unix Makefiles"
            fi
            ;;
        Linux)
            nvrhi_platform="linux"
            NVRHI_CMAKE_DX12_FLAG="-DNVRHI_WITH_DX12=OFF"
            NVRHI_CMAKE_GENERATOR="Unix Makefiles"
            ;;
        *)
            echo "[Setup] Unsupported platform for NVRHI build."
            exit 1
            ;;
    esac

    build_nvrhi_config Debug
    build_nvrhi_config Release
}

have_nvrhi_install_artifacts() {
    nvrhi_install_dir="$1"

    [ -f "$nvrhi_install_dir/include/nvrhi/nvrhi.h" ] || return 1
    [ -f "$nvrhi_install_dir/lib/libnvrhi.a" ] || return 1
    [ -f "$nvrhi_install_dir/lib/libnvrhi_vk.a" ] || return 1
    return 0
}

build_nvrhi_config() {
    nvrhi_config="$1"
    nvrhi_build_dir="$REPO_ROOT/Vendor/nvrhi/Build/$nvrhi_platform/$TARGET_ARCH/$nvrhi_config"
    nvrhi_install_dir="$REPO_ROOT/Vendor/nvrhi/Install/$nvrhi_platform/$TARGET_ARCH/$nvrhi_config"

    if have_nvrhi_install_artifacts "$nvrhi_install_dir"; then
        echo "[Setup] NVRHI ($nvrhi_config) is already available. Skipping build."
        return 0
    fi

    echo "[Setup] Building NVRHI ($nvrhi_config)..."
    set -- "$CMAKE_CMD" -S "$REPO_ROOT/Vendor/nvrhi" -B "$nvrhi_build_dir" -G "$NVRHI_CMAKE_GENERATOR" \
        -DNVRHI_WITH_VULKAN=ON \
        $NVRHI_CMAKE_DX12_FLAG \
        -DNVRHI_WITH_DX11=OFF \
        -DNVRHI_WITH_NVAPI=OFF \
        -DNVRHI_WITH_RTXMU=OFF \
        -DNVRHI_WITH_AFTERMATH=OFF \
        -DNVRHI_BUILD_SHARED=OFF \
        -DNVRHI_INSTALL=ON \
        -DCMAKE_BUILD_TYPE="$nvrhi_config" \
        -DCMAKE_INSTALL_PREFIX="$nvrhi_install_dir"

    if [ "$(uname -s)" = "Darwin" ] && [ -n "$TARGET_ARCH" ]; then
        if [ "$TARGET_ARCH" = "arm64" ]; then
            set -- "$@" -DCMAKE_OSX_ARCHITECTURES="arm64"
        else
            set -- "$@" -DCMAKE_OSX_ARCHITECTURES="x86_64"
        fi
    fi

    "$@"
    "$CMAKE_CMD" --build "$nvrhi_build_dir" --config "$nvrhi_config" --target install
}

resolve_premake() {
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

    if command -v premake5 >/dev/null 2>&1; then
        PREMAKE_CMD="premake5"
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
resolve_vulkan_sdk
build_nvrhi

echo "[Setup] Using Premake command: $PREMAKE_CMD"
echo "[Setup] Generating project files with Premake ($PREMAKE_ACTION)..."
"$PREMAKE_CMD" "$PREMAKE_ACTION" "$@"

echo "[Setup] Dependencies, SDL3, NVRHI, and project files are ready."
