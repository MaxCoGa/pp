#!/bin/bash
# Build script for static pp binary using Docker and musl-libc

set -e

echo "Building static pp binary with musl-libc..."

# Build the Docker image
docker build --target builder -t pp-builder .

# Extract the static binary from the container
docker create --name pp-extract pp-builder
docker cp pp-extract:/build/pp ./pp-static
docker rm pp-extract

echo "Static binary created: pp-static"
echo "Verifying binary..."
file pp-static
ls -lh pp-static

echo ""
echo "Testing if binary is truly static..."
if ldd pp-static 2>&1 | grep -q "not a dynamic executable"; then
    echo "Success! Binary is statically linked."
else
    echo "Warning: Binary may not be fully static."
    ldd pp-static
fi

echo ""
echo "Done! Your static binary is ready: pp-static"
