#!/bin/bash

# Build little-buddy locally using Docker
# Replicates the GitHub Actions workflow

set -e

echo "Building little-buddy using Docker..."

# Run the build in a Ubuntu container with ARM GCC toolchain
docker run --rm -v "$(pwd):/workspace" -w /workspace ubuntu:latest bash -c '
  set -e
  
  echo "Installing dependencies..."
  apt update
  apt install -y --no-install-recommends \
    gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    srecord \
    cmake \
    make \
    git \
    ca-certificates \
    python3 \
    python3-pip
  
  echo "Initializing submodules..."
  git config --global --add safe.directory /workspace
  git submodule update --init --recursive
  
  echo "Building firmware..."
  cd firmware
  mkdir -p build-feather
  cd build-feather
  
  echo "Running CMake with PICO_BOARD=feather_host..."
  PICO_BOARD=feather_host cmake ..
  
  echo "Building little_buddy target..."
  make -j4 little_buddy
  
  echo "Build complete!"
  ls -la little_buddy.*
'

# Copy the built firmware to artifacts directory
echo "Creating artifacts directory..."
mkdir -p artifacts
if [ -f firmware/build-feather/little_buddy.uf2 ]; then
    cp firmware/build-feather/little_buddy.uf2 artifacts/little_buddy.uf2
    echo "✅ Build successful! Firmware available at: artifacts/little_buddy.uf2"
    ls -la artifacts/little_buddy.uf2
else
    echo "❌ Build failed - no .uf2 file found"
    exit 1
fi