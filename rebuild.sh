#!/usr/bin/env bash
set -e

BUILD_DIR="build"

DO_CLEAN=false
DO_INSTALL=false
DO_PURGE=false
CMAKE_EXTRA_ARGS=()

usage() {
    echo "Usage: $(basename "$0") [options] [cmake-args...]"
    echo ""
    echo "Options:"
    echo "  clean    Remove build directory before building"
    echo "  install  Run 'sudo make install' after building"
    echo "  purge    Uninstall + clean before rebuilding"
    echo "  help     Show this message"
    echo ""
    echo "Extra arguments are passed to cmake, e.g.:"
    echo "  $(basename "$0") clean install -DCMAKE_BUILD_TYPE=Release"
}

for arg in "$@"; do
    case "$arg" in
        help|-h|--help) usage; exit 0 ;;
        clean)   DO_CLEAN=true ;;
        install) DO_INSTALL=true ;;
        purge)   DO_PURGE=true; DO_CLEAN=true ;;
        *)       CMAKE_EXTRA_ARGS+=("$arg") ;;
    esac
done

if $DO_PURGE && [ -f "$BUILD_DIR/install_manifest.txt" ]; then
    echo "Uninstalling..."
    sudo make -C "$BUILD_DIR" uninstall
fi

if $DO_CLEAN; then
    echo "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

cmake .. \
    -DNTRIP_TLS_SUPPORT=ON \
    -DBUILD_EXAMPLES=ON \
    -DBUILD_TOOLS=ON \
    -DBUILD_TESTS=ON \
    -DBUILD_PYTHON=ON \
    "${CMAKE_EXTRA_ARGS[@]}"

make -j"$(nproc)"

if $DO_INSTALL; then
    sudo make install
fi