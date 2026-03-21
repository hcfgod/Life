#!/usr/bin/env sh
set -eu

CONFIGURATION=${1:-Debug}

find_test_binary() {
    system_name=$1

    for test_binary in \
        "Build/${system_name}-x64/${CONFIGURATION}/Test/Test" \
        "Build/${system_name}-x86_64/${CONFIGURATION}/Test/Test"
    do
        if [ -x "$test_binary" ]; then
            printf '%s' "$test_binary"
            return 0
        fi
    done

    echo "Unable to find Test binary for ${system_name} ${CONFIGURATION}." >&2
    return 1
}

case "$(uname -s)" in
    Darwin)
        SYSTEM_NAME="macosx"
        TEST_BINARY=$(find_test_binary "$SYSTEM_NAME")
        TEST_DIRECTORY=$(dirname "$TEST_BINARY")
        DYLD_LIBRARY_PATH="$TEST_DIRECTORY${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" "$TEST_BINARY"
        ;;
    Linux)
        SYSTEM_NAME="linux"
        TEST_BINARY=$(find_test_binary "$SYSTEM_NAME")
        TEST_DIRECTORY=$(dirname "$TEST_BINARY")
        LD_LIBRARY_PATH="$TEST_DIRECTORY${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$TEST_BINARY"
        ;;
    *)
        echo "Unsupported platform for test execution."
        exit 1
        ;;
 esac
