#!/usr/bin/env sh
set -eu

CONFIGURATION=${1:-Debug}
shift || true
LOWER_CONFIGURATION=$(printf '%s' "$CONFIGURATION" | tr '[:upper:]' '[:lower:]')

resolve_default_arch_suffix() {
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

resolve_config() {
    DEFAULT_ARCH_SUFFIX=$(resolve_default_arch_suffix)

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
if [ $# -eq 0 ]; then
    set -- all
fi

make config="$MAKE_CONFIG" "$@"
