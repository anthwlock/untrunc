#!/bin/bash

# Build script for creating a static untrunc binary compatible with AWS Lambda
# This script builds the binary inside the AWS Lambda Python 3.13 runtime environment

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TAG="untrunc-lambda-builder"
OUTPUT_DIR="${SCRIPT_DIR}/lambda-build"

echo "Building static untrunc binary for AWS Lambda (x86_64 architecture)..."

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Build the Docker image for x86_64 (AWS Lambda architecture)
echo "Building Docker image for x86_64/amd64 architecture..."
docker build --platform linux/amd64 -f Dockerfile.lambda -t "$BUILD_TAG" .

# Create a temporary container to extract the binary
echo "Extracting binary from container..."
CONTAINER_ID=$(docker create "$BUILD_TAG")

# Copy the binary from the container
docker cp "$CONTAINER_ID:/opt/bin/untrunc" "$OUTPUT_DIR/untrunc"

# Clean up the temporary container
docker rm "$CONTAINER_ID"

# Verify the binary
echo "Verifying binary..."
if [ -f "$OUTPUT_DIR/untrunc" ]; then
    echo "✓ Binary created successfully at: $OUTPUT_DIR/untrunc"
    
    # Check if it's statically linked (should show "not a dynamic executable")
    echo "Checking if binary is statically linked..."
    if ldd "$OUTPUT_DIR/untrunc" 2>&1 | grep -q "not a dynamic executable\|statically linked"; then
        echo "✓ Binary is statically linked"
    else
        echo "⚠ Warning: Binary may not be fully static"
        ldd "$OUTPUT_DIR/untrunc" 2>&1 || true
    fi
    
    # Show file size
    SIZE=$(ls -lh "$OUTPUT_DIR/untrunc" | awk '{print $5}')
    echo "Binary size: $SIZE"
    
    echo ""
    echo "Lambda Layer Instructions:"
    echo "1. Create a Lambda layer directory structure:"
    echo "   mkdir -p lambda-layer/bin"
    echo "   cp $OUTPUT_DIR/untrunc lambda-layer/bin/"
    echo ""
    echo "2. Create the layer zip:"
    echo "   cd lambda-layer && zip -r ../untrunc-layer.zip ."
    echo ""
    echo "3. Upload untrunc-layer.zip as a Lambda layer"
    echo "4. In your Lambda function, the binary will be available at /opt/bin/untrunc"
    
else
    echo "✗ Failed to create binary"
    exit 1
fi

echo "Build completed successfully!"