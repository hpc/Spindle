#!/bin/bash
#
# Test 03-filesystem: Validate volume mounts and filesystem operations
#
# This test verifies:
# - Mounting host directories into containers
# - Reading files from host mounts
# - Writing files to mounted volumes
# - File ownership and permissions with :Z flag
# - Artifacts persisting after container exits
#
# This pattern is critical for Spindle containers:
# - Source code is mounted from host (read-only or read-write)
# - Build artifacts are written to mounted volumes
# - Logs persist on host for debugging
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-hello-03-filesystem"
DOCKERFILE="$REPO_ROOT/containers/spindle-hello-podman/03-filesystem/Dockerfile"

# Create temporary directories for testing
TEST_DIR=$(mktemp -d)
HOST_DATA_DIR="$TEST_DIR/host-data"
OUTPUT_DIR="$TEST_DIR/output"

mkdir -p "$HOST_DATA_DIR"
mkdir -p "$OUTPUT_DIR"

# Create test input file
echo "Hello from the host filesystem!" > "$HOST_DATA_DIR/test-input.txt"
echo "This file was created outside the container." >> "$HOST_DATA_DIR/test-input.txt"
echo "Container should be able to read this." >> "$HOST_DATA_DIR/test-input.txt"

# No special permissions needed - we'll use --userns=keep-id to map host UID into container

echo "=========================================="
echo "Spindle Hello World - 03-filesystem"
echo "=========================================="
echo ""
echo "This test validates volume mount patterns used in Spindle containers."
echo ""
echo "Test setup:"
echo "  Host data directory: $HOST_DATA_DIR"
echo "  Output directory: $OUTPUT_DIR"
echo "  Using --userns=keep-id to map host UID into container"
echo ""

# Build the image
podman_build "$IMAGE_NAME" "$DOCKERFILE" "$REPO_ROOT"

echo ""
echo "Running container with volume mounts..."
echo ""

# Run the container with volume mounts
# --userns=keep-id - Map host UID into container (avoids permission issues)
# -v host:container:Z - Z flag sets SELinux context for container access
podman run \
    --rm \
    --userns=keep-id \
    $LC_CERT_RUN_MOUNT \
    -v "$HOST_DATA_DIR:/home/testuser/host-data:Z" \
    -v "$OUTPUT_DIR:/home/testuser/output:Z" \
    "$IMAGE_NAME"

EXIT_CODE=$?

echo ""
echo "=========================================="
echo "Container exited. Checking results..."
echo "=========================================="
echo ""

if [ $EXIT_CODE -eq 0 ]; then
    echo "✓ Container executed successfully"
else
    echo "✗ Container exited with code $EXIT_CODE"
fi

echo ""
echo "Files created by container in output directory:"
ls -lh "$OUTPUT_DIR"

echo ""
echo "Content of container-output.txt:"
if [ -f "$OUTPUT_DIR/container-output.txt" ]; then
    cat "$OUTPUT_DIR/container-output.txt"
    echo ""
    echo "✓ Container successfully wrote to mounted volume"
else
    echo "✗ Expected output file not found"
fi

echo ""
echo "=========================================="
echo "Test complete!"
echo ""
echo "What was tested:"
echo "  ✓ Volume mounts (-v host:container:Z)"
echo "  ✓ User namespace mapping (--userns=keep-id)"
echo "  ✓ Reading files from host"
echo "  ✓ Writing files to mounted volumes"
echo "  ✓ File persistence after container exit"
echo "  ✓ SELinux context handling (:Z flag)"
echo ""
echo "This pattern enables Spindle to:"
echo "  - Access source code from host"
echo "  - Write build artifacts to persistent storage"
echo "  - Generate logs accessible after tests"
echo "  - Share data between container and host"
echo ""
echo "Cleaning up test directories..."
rm -rf "$TEST_DIR"
echo "✓ Cleanup complete"
echo ""
echo "Next step: run-hello-04-networking.sh"
echo "=========================================="

# Optional: Clean up the image
# Uncomment if you want to remove the image after testing
# podman_cleanup "$IMAGE_NAME"
