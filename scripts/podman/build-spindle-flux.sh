#!/bin/bash
#
# Build Spindle Flux container for podman
#
# This builds the Spindle Flux multi-node container.
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-flux-ubuntu"
DOCKERFILE="$REPO_ROOT/containers/spindle-flux-ubuntu/Dockerfile.podman"

echo "=========================================="
echo "Building Spindle Flux Container"
echo "=========================================="
echo ""
echo "This builds the Spindle Flux multi-node test container."
echo "Image: $IMAGE_NAME"
echo "Dockerfile: $DOCKERFILE"
echo ""

# Build the image
# Pass replicas=4 to match docker-compose configuration
podman_build "$IMAGE_NAME" "$DOCKERFILE" "$REPO_ROOT" "--build-arg replicas=4"

echo ""
echo "=========================================="
echo "Build complete!"
echo "=========================================="
echo ""
echo "Image: $IMAGE_NAME"
echo ""
echo "Next steps:"
echo "  1. Run regular tests: ./scripts/podman/test-spindle-flux.sh"
echo "  2. Run crash tests: ./scripts/podman/test-spindle-flux-crash.sh"
echo ""
