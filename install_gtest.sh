#!/bin/bash

# GTest installation script for DMS Service
# This script installs Google Test framework properly

set -e

echo "===  Installation Script ==="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "This script needs to be run as root (sudo)"
    exit 1
fi

echo "Installing dependencies..."

# Update package list
apt-get update

# Install build tools and GTest development package
apt-get install -y \
    build-essential \
    cmake \
    libgtest-dev \
    libgmock-dev \
    pkg-config \
    libsdbus-c++-dev

echo "Building and installing GTest..."

# Build and install GTest
cd /usr/src/gtest
cmake .
make -j$(nproc)
make install

# Update library cache
ldconfig

echo "Verifying GTest installation..."

# Check if GTest is properly installed
if pkg-config --exists gtest; then
    echo "✅ GTest found via pkg-config"
    pkg-config --cflags --libs gtest
else
    echo "⚠️  GTest not found via pkg-config, but may be installed manually"
fi

# Check if GMock is properly installed
if pkg-config --exists gmock; then
    echo "✅ GMock found via pkg-config"
    pkg-config --cflags --libs gmock
else
    echo "⚠️  GMock not found via pkg-config, but may be installed manually"
fi

# Check if libraries exist
if [ -f "/usr/local/lib/libgtest.a" ] || [ -f "/usr/lib/x86_64-linux-gnu/libgtest.a" ]; then
    echo "✅ GTest libraries found"
else
    echo "❌ GTest libraries not found"
    exit 1
fi

# Check if GMock libraries exist
if [ -f "/usr/local/lib/libgmock.a" ] || [ -f "/usr/lib/x86_64-linux-gnu/libgmock.a" ]; then
    echo "✅ GMock libraries found"
else
    echo "⚠️  GMock libraries not found (optional)"
fi

# Check if headers exist
if [ -f "/usr/local/include/gtest/gtest.h" ] || [ -f "/usr/include/gtest/gtest.h" ]; then
    echo "✅ GTest headers found"
else
    echo "❌ GTest headers not found"
    exit 1
fi

# Check if GMock headers exist
if [ -f "/usr/local/include/gmock/gmock.h" ] || [ -f "/usr/include/gmock/gmock.h" ]; then
    echo "✅ GMock headers found"
else
    echo "⚠️  GMock headers not found (optional)"
fi

echo ""
echo "=== GTest Installation Complete ==="
echo "You can now build the DMS Service tests:"
echo "  cd /path/to/DMS_Service"
echo "  mkdir -p build && cd build"
echo "  cmake .. && make -j\$(nproc)"
echo ""
echo "Or run the test script:"
echo "  sudo ./run_tests.sh"
