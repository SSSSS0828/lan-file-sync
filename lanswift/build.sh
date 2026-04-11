#!/bin/bash
set -e

echo "=== LanSwift Transfer - Build Script ==="

# Check dependencies
check_dep() {
    if ! command -v "$1" &>/dev/null; then
        echo "ERROR: '$1' not found. Install with: $2"
        exit 1
    fi
}
check_dep cmake  "sudo apt install cmake"
check_dep qmake  "sudo apt install qt5-default qtbase5-dev"
check_dep g++    "sudo apt install build-essential"

# Check Qt5
QT_VER=$(qmake --version 2>/dev/null | grep -oP '\d+\.\d+\.\d+' | head -1)
echo "Qt version: $QT_VER"

# Check OpenSSL
if ! pkg-config --exists openssl 2>/dev/null; then
    echo "ERROR: OpenSSL dev headers not found. Install with: sudo apt install libssl-dev"
    exit 1
fi
echo "OpenSSL: $(pkg-config --modversion openssl)"

# Build
BUILD_TYPE=${1:-Release}
echo "Build type: $BUILD_TYPE"

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
make -j"$(nproc)"

echo ""
echo "=== Build successful ==="
echo "Binary: $(pwd)/bin/lanswift_transfer"
echo ""
echo "Usage examples:"
echo "  GUI mode:    ./bin/lanswift_transfer"
echo "  Server mode: ./bin/lanswift_transfer --mode server --port 9527 --save-dir ~/Downloads"
echo "  Client mode: ./bin/lanswift_transfer --mode client --host 192.168.1.x --port 9527 --file /path/to/file"
