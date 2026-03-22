#!/usr/bin/env sh
set -eu

CONFIGURATION=${1:-Debug}
shift || true
LOWER_CONFIGURATION=$(printf '%s' "$CONFIGURATION" | tr '[:upper:]' '[:lower:]')
TARGET_ARCH=${LIFE_TARGET_ARCH:-}

for build_arg in "$@"; do
    case "$build_arg" in
        --arch=*)
            TARGET_ARCH=${build_arg#--arch=}
            ;;
    esac
done

normalize_architecture() {
    architecture=$(printf '%s' "$1" | tr '[:upper:]' '[:lower:]')
    case "$architecture" in
        amd64|x86_64)
            printf '%s' "x64"
            ;;
        arm64|aarch64)
            printf '%s' "arm64"
            ;;
        *)
            printf '%s' "$architecture"
            ;;
    esac
}

resolve_host_arch_suffix() {
    case "$(uname -m)" in
        arm64|aarch64)
            printf '%s' "arm64"
            ;;
        *)
            printf '%s' "x64"
            ;;
    esac
}

resolve_default_arch_suffix() {
    if [ -n "$TARGET_ARCH" ]; then
        normalize_architecture "$TARGET_ARCH"
        return 0
    fi

    case "$(uname -s)" in
        Darwin)
            resolve_host_arch_suffix
            ;;
        Linux)
            resolve_host_arch_suffix
            ;;
        *)
            printf '%s' "x64"
            ;;
    esac
}

resolve_linux_cross_prefix() {
    case "$1" in
        arm64)
            printf '%s' "aarch64-linux-gnu"
            ;;
        *)
            printf '%s' "x86_64-linux-gnu"
            ;;
    esac
}

resolve_make_architecture() {
    arch_suffix=$(resolve_default_arch_suffix)
    printf '%s' "$arch_suffix"
}

resolve_target_arch_from_config() {
    make_config=$1
    case "$make_config" in
        *_arm64)
            printf '%s' "arm64"
            ;;
        *_x64|*_x86_64)
            printf '%s' "x64"
            ;;
        *)
            resolve_make_architecture
            ;;
    esac
}

resolve_config() {
    DEFAULT_ARCH_SUFFIX=$(resolve_make_architecture)

    for candidate in "$LOWER_CONFIGURATION" "${LOWER_CONFIGURATION}_${DEFAULT_ARCH_SUFFIX}" "${LOWER_CONFIGURATION}_arm64" "${LOWER_CONFIGURATION}_x64" "${LOWER_CONFIGURATION}_x86_64"; do
        if [ -f Makefile ] && grep -qi "$candidate" Makefile; then
            printf '%s' "$candidate"
            return 0
        fi
    done

    printf '%s' "${LOWER_CONFIGURATION}_${DEFAULT_ARCH_SUFFIX}"
    return 0
}

MAKE_CONFIG=$(resolve_config)
TARGET_ARCH_SUFFIX=$(resolve_target_arch_from_config "$MAKE_CONFIG")
if [ $# -eq 0 ]; then
    set -- all
fi

MAKE_ARGUMENTS=""
for make_arg in "$@"; do
    case "$make_arg" in
        --arch=*)
            ;;
        *)
            if [ -z "$MAKE_ARGUMENTS" ]; then
                MAKE_ARGUMENTS=$(printf '%s' "$make_arg")
            else
                MAKE_ARGUMENTS=$(printf '%s\n%s' "$MAKE_ARGUMENTS" "$make_arg")
            fi
            ;;
    esac
done

if [ "$(uname -s)" = "Linux" ]; then
    HOST_ARCH_SUFFIX=$(resolve_host_arch_suffix)
    if [ "$HOST_ARCH_SUFFIX" != "$TARGET_ARCH_SUFFIX" ]; then
        LINUX_CROSS_PREFIX=${LIFE_LINUX_CROSS_PREFIX:-}
        if [ -z "$LINUX_CROSS_PREFIX" ]; then
            LINUX_CROSS_PREFIX=$(resolve_linux_cross_prefix "$TARGET_ARCH_SUFFIX")
        fi

        MAKE_CC=${CC:-${LINUX_CROSS_PREFIX}-gcc}
        MAKE_CXX=${CXX:-${LINUX_CROSS_PREFIX}-g++}
        MAKE_AR=${AR:-${LINUX_CROSS_PREFIX}-ar}
        MAKE_RANLIB=${RANLIB:-${LINUX_CROSS_PREFIX}-ranlib}

        for required_tool in "$MAKE_CC" "$MAKE_CXX" "$MAKE_AR" "$MAKE_RANLIB"; do
            if ! command -v "$required_tool" >/dev/null 2>&1; then
                echo "Linux cross-compilation requires '$required_tool'. Install the target toolchain or set CC/CXX/AR/RANLIB (or LIFE_LINUX_CROSS_PREFIX) explicitly." >&2
                exit 1
            fi
        done

        export CC="$MAKE_CC"
        export CXX="$MAKE_CXX"
        export AR="$MAKE_AR"
        export RANLIB="$MAKE_RANLIB"
    fi
fi

set --
if [ -n "$MAKE_ARGUMENTS" ]; then
    while IFS= read -r make_arg; do
        set -- "$@" "$make_arg"
    done <<EOF
$MAKE_ARGUMENTS
EOF
fi

make config="$MAKE_CONFIG" "$@"
