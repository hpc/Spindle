#!/bin/bash
#
# Test 02-user-switch: Validate user switching and permissions
#
# This test verifies:
# - Creating a non-root user in the container
# - Switching to that user (USER directive)
# - File permission handling
# - Sudo access when needed
#
# This pattern is used in all Spindle containers where builds and tests
# run as a non-root user for security and to match typical HPC environments.
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-hello-02-user"
DOCKERFILE="$REPO_ROOT/containers/spindle-hello-podman/02-user-switch/Dockerfile"

echo "=========================================="
echo "Spindle Hello World - 02-user-switch"
echo "=========================================="
echo ""
echo "This test validates non-root user patterns used in Spindle containers."
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
echo "  ✓ Non-root user creation"
echo "  ✓ User switching with USER directive"
echo "  ✓ Home directory setup"
echo "  ✓ File permission handling"
echo "  ✓ Sudo access"
echo ""
echo "This pattern is used in all Spindle containers to:"
echo "  - Match HPC security practices"
echo "  - Test file permissions realistically"
echo "  - Avoid running builds as root"
echo ""
echo "Next step: run-hello-03-filesystem.sh"
echo "=========================================="

# Optional: Clean up the image
# Uncomment if you want to remove the image after testing
# podman_cleanup "$IMAGE_NAME"
