#!/bin/bash
#
# Build Spindle Slurm rshlaunch test container for podman
#
# This builds the Slurm testing image with Spindle (rshlaunch launcher).
# Requires: spindle-slurm-base image (build with build-spindle-slurm-base.sh first)
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
BASE_IMAGE="spindle-slurm-base"
IMAGE_NAME="spindle-slurm-rshlaunch"
DOCKERFILE="$REPO_ROOT/containers/spindle-slurm-ubuntu/testing/Dockerfile.podman"

echo "=========================================="
echo "Building Spindle Slurm Rshlaunch Container"
echo "=========================================="
echo ""

# Check if base image exists (with or without localhost/ prefix)
if ! podman images --format "{{.Repository}}" | grep -qE "^(localhost/)?${BASE_IMAGE}$"; then
    echo "Error: Base image $BASE_IMAGE not found"
    echo ""
    echo "Available images:"
    podman images | grep spindle || echo "  (no spindle images found)"
    echo ""
    echo "Build it first with:"
    echo "  ./scripts/podman/build-spindle-slurm-base.sh"
    echo ""
    exit 1
fi

echo "Base image: $BASE_IMAGE"
echo "Test image: $IMAGE_NAME"
echo "Dockerfile: $DOCKERFILE"
echo ""

# Generate MariaDB configuration
echo "Generating MariaDB configuration..."
cd "$REPO_ROOT/containers/spindle-slurm-ubuntu/testing"
./generate_config.sh
echo "✓ Configuration generated"
cd "$REPO_ROOT"
echo ""

# Build the image
podman_build "$IMAGE_NAME" "$DOCKERFILE" "$REPO_ROOT" "--build-arg replicas=4"

echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
echo ""
echo "Image: $IMAGE_NAME"
echo ""
echo "Next steps:"
echo "  1. Run regular tests: ./scripts/podman/test-spindle-slurm-rshlaunch.sh"
echo ""
