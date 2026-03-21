#!/usr/bin/env sh
set -eu

CONFIGURATION=${1:-Debug}
shift || true
LOWER_CONFIGURATION=$(printf '%s' "$CONFIGURATION" | tr '[:upper:]' '[:lower:]')

resolve_config() {
    for candidate in "$LOWER_CONFIGURATION" "${LOWER_CONFIGURATION}_x64" "${LOWER_CONFIGURATION}_x86_64"; do
        if [ -f Makefile ] && grep -qi "$candidate" Makefile; then
            printf '%s' "$candidate"
            return 0
        fi
    done

    printf '%s' "${LOWER_CONFIGURATION}_x86_64"
    return 0
}

MAKE_CONFIG=$(resolve_config)
if [ $# -eq 0 ]; then
    set -- all
fi

make config="$MAKE_CONFIG" "$@"
