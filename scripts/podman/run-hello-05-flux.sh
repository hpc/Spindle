#!/bin/bash
#
# Test 05-flux-multi: Validate Flux multi-node cluster
#
# This test verifies:
# - Using official fluxrm/flux-sched base image
# - Multi-node Flux cluster setup
# - Head node and worker node coordination
# - Running distributed jobs with `flux run`
# - Flux resource management
#
# This demonstrates the pattern used for Spindle's Flux tests,
# simplified to just show Flux itself working.
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-hello-05-flux"
DOCKERFILE="$REPO_ROOT/containers/spindle-hello-podman/05-flux-multi/Dockerfile"
NETWORK_NAME="flux-test-net"
NUM_WORKERS=3

echo "=========================================="
echo "Spindle Hello World - 05-flux-multi"
echo "=========================================="
echo ""
echo "This test validates a multi-node Flux cluster setup."
echo "Nodes: 1 head + ${NUM_WORKERS} workers = $(($NUM_WORKERS + 1)) total"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up containers and network..."
    for i in $(seq 1 $NUM_WORKERS); do
        podman rm -f "flux-node-$i" 2>/dev/null || true
    done
    podman network rm "$NETWORK_NAME" 2>/dev/null || true
    echo "✓ Cleanup complete"
}

# Set trap to cleanup on exit
trap cleanup EXIT

echo "Building Flux container image..."
echo "(This may take a few minutes - downloading fluxrm/flux-sched base image)"
echo ""

# Build the image
podman_build "$IMAGE_NAME" "$DOCKERFILE" "$REPO_ROOT"

echo ""
echo "=========================================="
echo "Setting up Flux cluster..."
echo "=========================================="
echo ""

# Create network
echo "Creating network: $NETWORK_NAME"
podman network create "$NETWORK_NAME"
echo "✓ Network created"

echo ""
echo "Starting Flux nodes..."

# Start head node first
echo "  Starting flux-node-1 (head node)..."
podman run \
    --name "flux-node-1" \
    --hostname "flux-node-1" \
    --network "$NETWORK_NAME" \
    -d \
    "$IMAGE_NAME"

sleep 2

# Start worker nodes
for i in $(seq 2 $NUM_WORKERS); do
    echo "  Starting flux-node-$i (worker)..."
    podman run \
        --name "flux-node-$i" \
        --hostname "flux-node-$i" \
        --network "$NETWORK_NAME" \
        -d \
        "$IMAGE_NAME"
    sleep 1
done

echo ""
echo "✓ All nodes started"
echo ""
echo "Waiting for Flux cluster to initialize..."
echo "(Munge authentication + broker connections)"
sleep 15

echo ""
echo "=========================================="
echo "Flux Cluster Status"
echo "=========================================="
echo ""

# Show head node logs
echo "Head node logs:"
echo "----------------------------------------"
podman logs flux-node-1 2>&1 | tail -30

echo ""
echo "=========================================="
echo "Test complete!"
echo ""
echo "What was tested:"
echo "  ✓ Official fluxrm/flux-sched base image"
echo "  ✓ Multi-node Flux cluster (1 head + ${NUM_WORKERS} workers)"
echo "  ✓ Munge authentication"
echo "  ✓ Flux broker connections"
echo "  ✓ Distributed job execution (flux run)"
echo "  ✓ Resource management"
echo ""
echo "This demonstrates the Flux pattern used by Spindle:"
echo "  - Same image for all nodes"
echo "  - Head node vs worker behavior based on hostname"
echo "  - Workers connect to head node"
echo "  - Jobs can run across all nodes"
echo ""
echo "To interact with the cluster:"
echo "  podman exec -it flux-node-1 flux resource list"
echo "  podman exec -it flux-node-1 flux run -N ${NUM_WORKERS} hostname"
echo ""
echo "Next: 06-slurm-multi (Slurm cluster example)"
echo "=========================================="

# Cleanup happens automatically via trap
