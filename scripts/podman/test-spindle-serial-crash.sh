#!/bin/bash
#
# Run Spindle serial crash tests in podman
#
# This runs only the crash tests (slower).
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-serial-ubuntu"
CONTAINER_NAME="spindlenode"

echo "=========================================="
echo "Spindle Serial Crash Tests"
echo "=========================================="
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up container..."
    podman rm -f "$CONTAINER_NAME" 2>/dev/null || true
    echo "✓ Cleanup complete"
}

# Set trap to cleanup on exit
trap cleanup EXIT

# Initial cleanup
cleanup

echo "Starting Spindle serial container..."
echo ""

# Start the container
podman run \
    --name "$CONTAINER_NAME" \
    --hostname "$CONTAINER_NAME" \
    --cap-add SYS_NICE \
    -d \
    -t \
    "$IMAGE_NAME"

echo "✓ Container started"
sleep 3

echo ""
echo "=========================================="
echo "Running crash tests..."
echo "=========================================="
echo ""
echo "This will take several minutes."
echo ""

# Run crash tests
if podman exec "$CONTAINER_NAME" bash -c 'cd Spindle-build/testsuite && ./run_crash_tests.sh --launcher=serial --scratch=/tmp/spindle_crash_test'; then
    echo ""
    echo "=========================================="
    echo "✓ All crash tests passed!"
    echo "=========================================="
    echo ""
    exit 0
else
    echo ""
    echo "=========================================="
    echo "✗ Some crash tests failed"
    echo "=========================================="
    echo ""
    echo "To inspect the container:"
    echo "  podman exec -it $CONTAINER_NAME bash"
    echo ""
    exit 1
fi

# Cleanup happens automatically via trap
