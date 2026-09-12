#!/bin/bash
#
# Test 01-basic: Validate podman environment
#
# This test verifies:
# - Container can build with apt-get (tests setgroups fix)
# - Container can run
# - Network connectivity works
# - SSL certificates are properly configured
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-hello-01-basic"
DOCKERFILE="$REPO_ROOT/containers/spindle-hello-podman/01-basic/Dockerfile"

echo "=========================================="
echo "Spindle Hello World - 01-basic"
echo "=========================================="
echo ""
echo "This test validates the podman environment on LC systems."
echo ""

# Build the image
podman_build "$IMAGE_NAME" "$DOCKERFILE" "$REPO_ROOT"

echo ""

# Run the container
podman_run "$IMAGE_NAME"

echo ""
echo "=========================================="
echo "Test complete!"
echo ""
echo "What was tested:"
echo "  ✓ apt-get works (setgroups fix applied)"
echo "  ✓ Container runs successfully"
echo "  ✓ Network connectivity"
echo "  ✓ SSL certificates configured"
echo ""
echo "Next step: run-hello-02-user.sh"
echo "=========================================="

# Optional: Clean up the image
# Uncomment if you want to remove the image after testing
# podman_cleanup "$IMAGE_NAME"
