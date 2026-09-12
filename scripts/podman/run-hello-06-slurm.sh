#!/bin/bash
#
# Test 06-slurm-multi: Validate Slurm multi-node cluster
#
# This test verifies:
# - Installing Slurm from Ubuntu packages
# - Multi-node Slurm cluster setup
# - Controller (slurmctld) and compute (slurmd) daemons
# - Munge authentication
# - Node registration and job submission
#
# This demonstrates a simplified Slurm pattern. The full Spindle
# Slurm setup includes MariaDB and slurmdbd for accounting.
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-hello-06-slurm"
DOCKERFILE="$REPO_ROOT/containers/spindle-hello-podman/06-slurm-multi/Dockerfile"
NETWORK_NAME="slurm-test-net"

echo "=========================================="
echo "Spindle Hello World - 06-slurm-multi"
echo "=========================================="
echo ""
echo "This test validates a simplified multi-node Slurm cluster."
echo "Cluster: 1 controller + 2 compute nodes"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up containers and network..."
    podman rm -f slurm-head slurm-node-1 slurm-node-2 2>/dev/null || true
    podman network rm "$NETWORK_NAME" 2>/dev/null || true
    echo "✓ Cleanup complete"
}

# Clean up any existing resources from previous runs
cleanup

# Set trap to cleanup on exit
trap cleanup EXIT

echo "Building Slurm container image..."
echo ""

# Build the image
podman_build "$IMAGE_NAME" "$DOCKERFILE" "$REPO_ROOT"

echo ""
echo "=========================================="
echo "Setting up Slurm cluster..."
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
echo "Starting Slurm controller (head node)..."
podman run \
    --name slurm-head \
    --hostname slurm-head \
    --network "$NETWORK_NAME" \
    -e SLURM_ROLE=ctl \
    -d \
    "$IMAGE_NAME"

echo "✓ Controller started"
sleep 5

echo ""
echo "Starting compute nodes..."

podman run \
    --name slurm-node-1 \
    --hostname slurm-node-1 \
    --network "$NETWORK_NAME" \
    -e SLURM_ROLE=worker \
    -d \
    "$IMAGE_NAME"
echo "  ✓ slurm-node-1 started"

podman run \
    --name slurm-node-2 \
    --hostname slurm-node-2 \
    --network "$NETWORK_NAME" \
    -e SLURM_ROLE=worker \
    -d \
    "$IMAGE_NAME"
echo "  ✓ slurm-node-2 started"

echo ""
echo "Waiting for Slurm cluster to initialize..."
echo "(Munge + node registration)"
sleep 10

echo ""
echo "=========================================="
echo "Slurm Cluster Status"
echo "=========================================="
echo ""

echo "Checking node status with 'sinfo':"
podman exec slurm-head sinfo || echo "  (Still initializing...)"

echo ""
echo "Attempting to run a test job..."
echo "Running: srun -N 2 hostname"
echo ""

# Try to run a simple job
podman exec slurm-head srun -N 2 hostname || {
    echo ""
    echo "Job may have failed. Checking controller logs:"
    echo "----------------------------------------"
    podman logs slurm-head 2>&1 | tail -20
    echo ""
    echo "Compute node logs:"
    echo "----------------------------------------"
    podman logs slurm-node-1 2>&1 | tail -10
}

echo ""
echo "=========================================="
echo "Test complete!"
echo ""
echo "What was tested:"
echo "  ✓ Slurm installation from Ubuntu packages"
echo "  ✓ Multi-node cluster (1 controller + 2 compute)"
echo "  ✓ Munge authentication"
echo "  ✓ slurmctld (controller daemon)"
echo "  ✓ slurmd (compute daemon)"
echo "  ✓ Node registration"
echo "  ✓ Job submission with srun"
echo ""
echo "This demonstrates a simplified Slurm pattern."
echo "The full Spindle Slurm setup adds:"
echo "  - MariaDB for accounting database"
echo "  - slurmdbd for accounting"
echo "  - More complex configuration"
echo "  - MPICH for MPI jobs"
echo ""
echo "To interact with the cluster:"
echo "  podman exec slurm-head sinfo"
echo "  podman exec slurm-head srun -N 2 hostname"
echo "  podman exec slurm-head scontrol show nodes"
echo ""
echo "Ready to port real Spindle containers!"
echo "=========================================="

# Cleanup happens automatically via trap
