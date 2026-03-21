#!/usr/bin/env sh
set -eu

CONFIGURATION=${1:-Debug}
case "$(uname -s)" in
    Darwin)
        SYSTEM_NAME="macosx"
        TEST_BINARY="Build/${SYSTEM_NAME}-x86_64/${CONFIGURATION}/Test/Test"
        TEST_DIRECTORY=$(dirname "$TEST_BINARY")
        DYLD_LIBRARY_PATH="$TEST_DIRECTORY${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}" "$TEST_BINARY"
        ;;
    Linux)
        SYSTEM_NAME="linux"
        TEST_BINARY="Build/${SYSTEM_NAME}-x86_64/${CONFIGURATION}/Test/Test"
        TEST_DIRECTORY=$(dirname "$TEST_BINARY")
        LD_LIBRARY_PATH="$TEST_DIRECTORY${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$TEST_BINARY"
        ;;
    *)
        echo "Unsupported platform for test execution."
        exit 1
        ;;
 esac
