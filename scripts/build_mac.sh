#!/bin/bash
# =============================================================================
# LombardEar Mac Build Script
# =============================================================================
# Usage: ./scripts/build_mac.sh [debug|release|test]
# =============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"

BUILD_TYPE="${1:-release}"

echo "=============================================="
echo "LombardEar Mac Build"
echo "Build type: $BUILD_TYPE"
echo "=============================================="

# Check dependencies
echo "[1/4] Checking dependencies..."

if ! command -v brew &> /dev/null; then
    echo "ERROR: Homebrew not found. Install from https://brew.sh"
    exit 1
fi

if ! brew list portaudio &> /dev/null; then
    echo "PortAudio not found. Installing..."
    brew install portaudio
fi

# Create build directory
echo "[2/4] Configuring CMake..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

CMAKE_BUILD_TYPE="Release"
if [ "$BUILD_TYPE" = "debug" ]; then
    CMAKE_BUILD_TYPE="Debug"
fi

cmake .. \
    -DCMAKE_BUILD_TYPE="$CMAKE_BUILD_TYPE" \
    -DLE_BUILD_APP=ON \
    -DLE_BUILD_TESTS=ON \
    -DLE_WITH_WEBSOCKETS=ON

# Build
echo "[3/4] Building..."
cmake --build . --parallel $(sysctl -n hw.ncpu)

# Run tests if requested
if [ "$BUILD_TYPE" = "test" ]; then
    echo "[4/4] Running tests..."
    ctest --output-on-failure
else
    echo "[4/4] Skipping tests (run with 'test' argument to enable)"
fi

echo ""
echo "=============================================="
echo "Build Complete!"
echo "=============================================="
echo "Executable: $BUILD_DIR/lombardear"
echo ""
echo "Quick start:"
echo "  cd $BUILD_DIR"
echo "  ./lombardear"
echo ""
echo "WebSocket UI: http://localhost:8000"
echo "=============================================="
