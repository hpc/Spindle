#!/bin/bash
#
# Run Spindle serial tests with debug logging enabled
#
# This demonstrates:
# 1. Running tests with SPINDLE_DEBUG=3 (verbose logging)
# 2. Extracting logs from the container to the host
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-serial-ubuntu"
CONTAINER_NAME="spindlenode-debug"
LOG_DIR="$REPO_ROOT/spindle-debug-logs"

echo "=========================================="
echo "Spindle Serial Debug Test"
echo "=========================================="
echo ""
echo "This runs a single test with SPINDLE_DEBUG=3 enabled"
echo "and extracts the debug logs to the host."
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

# Create log directory on host
mkdir -p "$LOG_DIR"
echo "Debug logs will be saved to: $LOG_DIR"
echo ""

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
echo "Running single test with SPINDLE_DEBUG=3"
echo "=========================================="
echo ""
echo "Test: ./run_driver --dependency --preload"
echo ""

# Run a single test with SPINDLE_DEBUG=3
# This should generate verbose logs
podman exec "$CONTAINER_NAME" bash -c 'cd Spindle-build/testsuite && SPINDLE_DEBUG=3 ./run_driver --dependency --preload' || {
    echo ""
    echo "Note: Test may have generated logs even if it failed"
}

echo ""
echo "=========================================="
echo "Extracting logs from container"
echo "=========================================="
echo ""

# Spindle log files are named: spindle_output.<nodename>.<pid>
echo "Looking for spindle_output.* files in testsuite directory..."
if podman exec "$CONTAINER_NAME" bash -c 'ls -la /home/spindleuser/Spindle-build/testsuite/spindle_output.* 2>/dev/null'; then
    echo ""
    echo "Found Spindle log files! Extracting..."

    # Get list of log files
    LOG_FILES=$(podman exec "$CONTAINER_NAME" bash -c 'cd /home/spindleuser/Spindle-build/testsuite && ls spindle_output.* 2>/dev/null' || echo "")

    if [ -n "$LOG_FILES" ]; then
        for logfile in $LOG_FILES; do
            echo "  Copying $logfile..."
            podman cp "$CONTAINER_NAME:/home/spindleuser/Spindle-build/testsuite/$logfile" "$LOG_DIR/"
        done
        echo ""
        echo "✓ Logs extracted to: $LOG_DIR"
        echo ""
        echo "View logs with:"
        for logfile in $LOG_FILES; do
            echo "  cat $LOG_DIR/$logfile"
        done
    fi
else
    echo "No spindle_output.* files found in testsuite directory"
fi

echo ""
echo "Checking for other spindle files..."
podman exec "$CONTAINER_NAME" bash -c 'find /home/spindleuser/Spindle-build/testsuite -name "spindle*" -type f 2>/dev/null | head -20' || echo "No other spindle files found"

echo ""
echo "=========================================="
echo "Summary"
echo "=========================================="
echo ""
echo "Container: $CONTAINER_NAME (still running)"
echo "Log directory: $LOG_DIR"
echo ""
echo "To explore interactively:"
echo "  podman exec -it $CONTAINER_NAME bash"
echo "  cd /home/spindleuser/Spindle-build/testsuite"
echo "  ls spindle_output.*"
echo ""
echo "When done, cleanup with:"
echo "  podman rm -f $CONTAINER_NAME"
echo ""

# Don't cleanup automatically - let user explore
trap - EXIT
echo "Note: Container left running for inspection. Clean up manually when done."
echo ""
