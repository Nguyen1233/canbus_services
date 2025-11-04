#!/bin/bash

# Build script for DMS Service with tests
# This script builds the project and runs tests

set -e

echo "=== DMS Service Build Script ==="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: CMakeLists.txt not found. Please run this script from the project root."
    exit 1
fi

# Check if GTest is available
echo "Checking for GTest..."
if ! pkg-config --exists gtest 2>/dev/null && [ ! -f "/usr/local/lib/libgtest.a" ] && [ ! -f "/usr/lib/x86_64-linux-gnu/libgtest.a" ]; then
    echo "❌ GTest not found!"
    echo ""
    echo "Please install GTest first:"
    echo "  sudo ./install_gtest.sh"
    echo ""
    echo "Or manually:"
    echo "  sudo apt-get install libgtest-dev libgmock-dev"
    echo "  cd /usr/src/gtest"
    echo "  sudo cmake ."
    echo "  sudo make"
    echo "  sudo make install"
    echo ""
    exit 1
else
    echo "✅ GTest found"
fi

# Check if GMock is available (optional)
echo "Checking for GMock..."
if pkg-config --exists gmock 2>/dev/null || [ -f "/usr/local/lib/libgmock.a" ] || [ -f "/usr/lib/x86_64-linux-gnu/libgmock.a" ]; then
    echo "✅ GMock found"
else
    echo "⚠️  GMock not found (optional - tests will use GTest only)"
fi

# Create build directory
echo "Creating build directory..."
mkdir -p build
cd build

# Configure with CMake
echo "Configuring with CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build the project
echo "Building project..."
make -j$(nproc)

echo "Build completed successfully!"

# Locate test executables
TEST_BIN_DIR="."
if [ -x "./test_can_connector" ] || [ -x "./test_integration" ]; then
    TEST_BIN_DIR="."
elif [ -x "./tests/test_can_connector" ] || [ -x "./tests/test_integration" ]; then
    TEST_BIN_DIR="./tests"
fi

if [ -x "$TEST_BIN_DIR/test_can_connector" ] || [ -x "$TEST_BIN_DIR/test_integration" ] || [ -x "$TEST_BIN_DIR/test_can_listener" ] || [ -x "$TEST_BIN_DIR/test_app_server_bridge" ]; then
    echo "✅ Test executables found in: $TEST_BIN_DIR"
    ls -la $TEST_BIN_DIR/test_* || true
    echo ""
    echo "To run tests manually:"
    echo "  cd build"
    echo "  $TEST_BIN_DIR/test_can_connector"
    echo "  $TEST_BIN_DIR/test_can_listener"
    echo "  $TEST_BIN_DIR/test_app_server_bridge"
    echo "  $TEST_BIN_DIR/test_integration"
    echo ""
    echo "Or run all with ctest:"
    echo "  cd build && ctest --output-on-failure"
else
    echo "❌ Test executables not found. Check CMake configuration."
    echo ""
    echo "Common issues:"
    echo "- GTest not properly installed"
    echo "- sdbus-c++ not found"
    echo "- Missing build dependencies"
    echo ""
    echo "Try: sudo ./install_gtest.sh"
fi

echo "=== Build Script Completed ==="
