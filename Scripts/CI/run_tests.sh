#!/usr/bin/env sh
set -eu

CONFIGURATION=${1:-Debug}
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../.." && pwd)

resolve_target_arch_suffix() {
    case "$(uname -s)" in
        Darwin)
            case "$(uname -m)" in
                arm64|aarch64)
                    printf '%s' "arm64"
                    ;;
                *)
                    printf '%s' "x64"
                    ;;
            esac
            ;;
        Linux)
            case "$(uname -m)" in
                arm64|aarch64)
                    printf '%s' "arm64"
                    ;;
                *)
                    printf '%s' "x64"
                    ;;
            esac
            ;;
        *)
            printf '%s' "x64"
            ;;
    esac
}

TARGET_ARCH_SUFFIX=$(resolve_target_arch_suffix)

find_test_binary() {
    system_name=$1

    for test_binary in \
        "$REPO_ROOT/Build/${system_name}-${TARGET_ARCH_SUFFIX}/${CONFIGURATION}/Test/Test" \
        "$REPO_ROOT/Build/${system_name}-x64/${CONFIGURATION}/Test/Test" \
        "$REPO_ROOT/Build/${system_name}-arm64/${CONFIGURATION}/Test/Test" \
        "$REPO_ROOT/Build/${system_name}-x86_64/${CONFIGURATION}/Test/Test"
    do
        if [ -x "$test_binary" ]; then
            printf '%s' "$test_binary"
            return 0
        fi
    done

    echo "Unable to find Test binary for ${system_name} ${CONFIGURATION}." >&2
    return 1
}

find_sdl_lib_dir() {
    system_name=$1

    case "$system_name" in
        Darwin|macosx)
            sdl_platform="macos"
            ;;
        Linux|linux)
            sdl_platform="linux"
            ;;
        *)
            echo "Unsupported platform for SDL library resolution: ${system_name}." >&2
            return 1
            ;;
    esac

    for sdl_lib_dir in \
        "$REPO_ROOT/Vendor/SDL3/Install/${sdl_platform}/${TARGET_ARCH_SUFFIX}/${CONFIGURATION}/lib" \
        "$REPO_ROOT/Vendor/SDL3/Install/${sdl_platform}/${TARGET_ARCH_SUFFIX}/Release/lib" \
        "$REPO_ROOT/Vendor/SDL3/Install/${sdl_platform}/x64/${CONFIGURATION}/lib" \
        "$REPO_ROOT/Vendor/SDL3/Install/${sdl_platform}/x64/Release/lib" \
        "$REPO_ROOT/Vendor/SDL3/Install/${sdl_platform}/arm64/${CONFIGURATION}/lib" \
        "$REPO_ROOT/Vendor/SDL3/Install/${sdl_platform}/arm64/Release/lib"
    do
        if [ -d "$sdl_lib_dir" ]; then
            printf '%s' "$sdl_lib_dir"
            return 0
        fi
    done

    echo "Unable to find SDL3 library directory for ${system_name} ${CONFIGURATION}." >&2
    return 1
}

case "$(uname -s)" in
    Darwin)
        SYSTEM_NAME="macosx"
        TEST_BINARY=$(find_test_binary "$SYSTEM_NAME")
        TEST_DIRECTORY=$(dirname "$TEST_BINARY")
        SDL_LIB_DIR=$(find_sdl_lib_dir "$SYSTEM_NAME")
        DYLD_LIBRARY_PATH="$TEST_DIRECTORY:$SDL_LIB_DIR${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" "$TEST_BINARY"
        ;;
    Linux)
        SYSTEM_NAME="linux"
        TEST_BINARY=$(find_test_binary "$SYSTEM_NAME")
        TEST_DIRECTORY=$(dirname "$TEST_BINARY")
        SDL_LIB_DIR=$(find_sdl_lib_dir "$SYSTEM_NAME")
        LD_LIBRARY_PATH="$TEST_DIRECTORY:$SDL_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$TEST_BINARY"
        ;;
    *)
        echo "Unsupported platform for test execution."
        exit 1
        ;;
 esac
