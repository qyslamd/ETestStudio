#!/bin/bash
set -e

PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_ROOT"

BUILD_TYPE=""
TARGET=""
CONFIGURE_ONLY=0

show_help() {
    cat <<'EOF'
Usage: build_ninja.sh -t <type> [-m <target>] [-c]
Build ETestStudio on Linux with Ninja.

Options:
  -t, --type <type>       Build type: debug / relwithdebinfo / release
  -m, --target <target>   Build target (e.g. ETestStudio), omit for all
  -c, --configure         Only run CMake configure, skip build
  -h, --help              Show this help

Examples:
  build_ninja.sh -t debug
  build_ninja.sh -t debug -m ETestStudio
  build_ninja.sh -t relwithdebinfo -c
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)
            show_help
            exit 0
            ;;
        -t|--type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --type=*)
            BUILD_TYPE="${1#--type=}"
            shift
            ;;
        -m|--target)
            TARGET="$2"
            shift 2
            ;;
        --target=*)
            TARGET="${1#--target=}"
            shift
            ;;
        -c|--configure)
            CONFIGURE_ONLY=1
            shift
            ;;
        *)
            echo "Unknown argument: $1"
            show_help
            exit 1
            ;;
    esac
done

if [[ -z "$BUILD_TYPE" ]]; then
    BUILD_TYPE="debug"
fi

case "$BUILD_TYPE" in
    debug)
        PRESET="ninja-debug-linux"
        BUILD_DIR="build/ninja-debug-linux"
        ;;
    relwithdebinfo)
        PRESET="ninja-relwithdebinfo-linux"
        BUILD_DIR="build/ninja-relwithdebinfo-linux"
        ;;
    release)
        PRESET="ninja-release-linux"
        BUILD_DIR="build/ninja-release-linux"
        ;;
    *)
        echo "Unknown build type: $BUILD_TYPE"
        echo "Usage: build_ninja.sh -t <debug|relwithdebinfo|release> [-m <target>] [-c]"
        exit 1
        ;;
esac

echo "Build type: $BUILD_TYPE ($PRESET)"
[[ -n "$TARGET" ]] && echo "Build target: $TARGET"

cmake -S . --preset "$PRESET"

echo "Configure OK"

if [[ $CONFIGURE_ONLY -eq 1 ]]; then
    echo "Configure only mode, skipping build."
    exit 0
fi

if [[ -n "$TARGET" ]]; then
    cmake --build "$BUILD_DIR" --target "$TARGET"
else
    cmake --build "$BUILD_DIR"
fi

echo "Build OK!"
