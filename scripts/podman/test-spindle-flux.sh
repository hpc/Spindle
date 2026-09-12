#!/bin/bash
#
# Run Spindle Flux tests in podman
#
# This runs the Spindle testsuite in a 4-node Flux cluster.
# Based on the CI workflow and docker-compose configuration.
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-flux-ubuntu"
NETWORK_NAME="flux-test-net"
WORKERS=4
MAIN_HOST="node-1"

echo "=========================================="
echo "Spindle Flux Tests"
echo "=========================================="
echo ""
echo "This runs the Spindle testsuite in a 4-node Flux cluster."
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up containers and network..."
    for i in $(seq 1 $WORKERS); do
        podman rm -f "node-$i" 2>/dev/null || true
    done
    podman network rm "$NETWORK_NAME" 2>/dev/null || true
    echo "✓ Cleanup complete"
}

# Set trap to cleanup on exit
trap cleanup EXIT

# Initial cleanup
cleanup

echo "=========================================="
echo "Setting up Flux cluster..."
echo "=========================================="
echo ""

# Create network (or reuse if exists)
if podman network exists "$NETWORK_NAME" 2>/dev/null; then
    echo "Network $NETWORK_NAME already exists, reusing"
else
    echo "Creating network: $NETWORK_NAME"
    podman network create "$NETWORK_NAME"
    echo "✓ Network created"
fi

echo ""
echo "Starting Flux nodes..."
echo ""

# Start all 4 nodes
for i in $(seq 1 $WORKERS); do
    NODE_NAME="node-$i"
    echo "Starting $NODE_NAME..."
    podman run \
        --name "$NODE_NAME" \
        --hostname "$NODE_NAME" \
        --network "$NETWORK_NAME" \
        -e mainHost="$MAIN_HOST" \
        -e workers="$WORKERS" \
        --cap-add SYS_NICE \
        -d \
        "$IMAGE_NAME"
    echo "  ✓ $NODE_NAME started"
done

echo ""
echo "Waiting for Flux cluster to initialize..."
echo "(This takes ~20-30 seconds for all nodes to register)"
sleep 30

echo ""
echo "=========================================="
echo "Checking container status..."
echo "=========================================="
echo ""

# Check if containers are still running
for i in $(seq 1 $WORKERS); do
    NODE_NAME="node-$i"
    if podman ps --filter "name=$NODE_NAME" --format "{{.Names}}" | grep -q "$NODE_NAME"; then
        echo "  ✓ $NODE_NAME is running"
    else
        echo "  ✗ $NODE_NAME has exited!"
        echo ""
        echo "Last 30 lines of $NODE_NAME logs:"
        echo "----------------------------------------"
        podman logs "$NODE_NAME" 2>&1 | tail -30
        echo "----------------------------------------"
        echo ""
        echo "Container exited unexpectedly. Check logs above."
        exit 1
    fi
done

echo ""
echo "=========================================="
echo "Verifying munge authentication..."
echo "=========================================="
echo ""

podman exec "$MAIN_HOST" bash -c 'munge -n | unmunge'

echo ""
echo "✓ Munge working"

echo ""
echo "=========================================="
echo "Verifying Flux cluster health..."
echo "=========================================="
echo ""

echo "Checking Flux status..."
podman exec "$MAIN_HOST" bash -c 'flux resource list' || echo "  (Flux may still be initializing)"

echo ""
echo "Running flux healthcheck..."
podman exec "$MAIN_HOST" bash -c './flux_healthcheck.sh' || echo "  (Some nodes may not be registered yet)"

echo ""
echo "=========================================="
echo "Running Spindle testsuite..."
echo "=========================================="
echo ""
echo "This will take several minutes."
echo ""

# Run the testsuite
# Based on CI: docker exec node-1 bash -c 'cd Spindle-build/testsuite && flux alloc --nodes=${workers} ./runTests --nodes=${workers} --tasks-per-node=3'
if podman exec "$MAIN_HOST" bash -c "cd Spindle-build/testsuite && flux alloc --nodes=${WORKERS} ./runTests --nodes=${WORKERS} --tasks-per-node=3"; then
    echo ""
    echo "=========================================="
    echo "✓ All tests passed!"
    echo "=========================================="
    echo ""
    exit 0
else
    echo ""
    echo "=========================================="
    echo "✗ Some tests failed"
    echo "=========================================="
    echo ""
    echo "To inspect the cluster:"
    echo "  podman exec -it $MAIN_HOST bash"
    echo "  flux resource list"
    echo "  cd Spindle-build/testsuite"
    echo ""
    exit 1
fi

# Cleanup happens automatically via trap
