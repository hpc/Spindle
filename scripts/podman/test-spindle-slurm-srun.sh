#!/bin/bash
#
# Run Spindle Slurm srun tests in podman
#
# This runs the Spindle testsuite in a Slurm cluster with srun launcher.
# Based on the CI workflow and docker-compose configuration.
#
# Cluster: 1 MariaDB + 1 slurmdbd + 1 slurmctld + 4 slurmd workers
#
# CPU Pinning: Cores 24-30 (avoiding system cores 0-23)
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-slurm-srun"
NETWORK_NAME="slurm-srun-test-net"
WORKERS=4

# CPU pinning disabled - not supported in rootless podman on this system
# See PODMAN.md for details
# CPU_MARIADB=24
# CPU_DB=25
# CPU_HEAD=26
# CPU_NODE_BASE=27  # nodes 1-4 get 27-30

echo "=========================================="
echo "Spindle Slurm Srun Tests"
echo "=========================================="
echo ""
echo "This runs the Spindle testsuite in a Slurm cluster."
echo "Cluster: MariaDB + slurmdbd + slurmctld + 4 workers"
echo "Note: CPU pinning disabled (not supported in rootless podman)"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up containers and network..."
    # Stop and remove all containers (force removal even if running)
    for container in slurm-srun-mariadb slurm-srun-db slurm-srun-head slurm-srun-node-{1..4}; do
        podman stop "$container" 2>/dev/null || true
        podman rm -f "$container" 2>/dev/null || true
    done
    podman network rm -f "$NETWORK_NAME" 2>/dev/null || true
    echo "✓ Cleanup complete"
}

# Set trap to cleanup on exit
# DISABLED for debugging - cleanup manually with: podman rm -f slurm-srun-{mariadb,db,head,node-{1..4}}; podman network rm -f slurm-srun-test-net
# trap cleanup EXIT

# Initial cleanup
cleanup

echo "=========================================="
echo "Setting up Slurm cluster..."
echo "=========================================="
echo ""

# Create network
if podman network exists "$NETWORK_NAME" 2>/dev/null; then
    echo "Network $NETWORK_NAME already exists, reusing"
else
    echo "Creating network: $NETWORK_NAME"
    podman network create "$NETWORK_NAME"
    echo "✓ Network created"
fi

echo ""
echo "Starting MariaDB..."
# Read password from generated mariadb.env
MARIADB_PASSWORD=$(grep MARIADB_PASSWORD "$REPO_ROOT/containers/spindle-slurm-ubuntu/testing-srun/mariadb.env" | cut -d'"' -f2)
if [ -z "$MARIADB_PASSWORD" ]; then
    echo "Error: Could not read password from mariadb.env"
    exit 1
fi
podman run \
    --name slurm-srun-mariadb \
    --hostname slurm-mariadb \
    --network "$NETWORK_NAME" \
    -e MYSQL_RANDOM_ROOT_PASSWORD=yes \
    -e MYSQL_DATABASE=slurm_acct_db \
    -e MYSQL_USER=slurm \
    -e MYSQL_PASSWORD="$MARIADB_PASSWORD" \
    -d \
    mariadb:12

echo "  ✓ MariaDB started"
echo "Waiting for MariaDB to initialize..."
sleep 15

echo ""
echo "Starting slurmdbd (accounting daemon)..."
podman run \
    --name slurm-srun-db \
    --hostname slurm-db \
    --network "$NETWORK_NAME" \
    -e SLURM_ROLE=db \
    -e SLURM_HEAD_NODE=slurm-head \
    -e workers="$WORKERS" \
    -d \
    "$IMAGE_NAME"

echo "  ✓ slurmdbd started"
sleep 10

echo ""
echo "Starting slurmctld (controller)..."
podman run \
    --name slurm-srun-head \
    --hostname slurm-head \
    --network "$NETWORK_NAME" \
    -e SLURM_ROLE=ctl \
    -e SLURM_HEAD_NODE=slurm-head \
    -e workers="$WORKERS" \
    -t \
    -d \
    "$IMAGE_NAME"

echo "  ✓ slurmctld started"
sleep 10

echo ""
echo "Starting worker nodes..."
for i in $(seq 1 $WORKERS); do
    echo "Starting slurm-node-$i..."
    podman run \
        --name "slurm-srun-node-$i" \
        --hostname "slurm-node-$i" \
        --network "$NETWORK_NAME" \
        -e SLURM_ROLE=worker \
        -e SLURM_HEAD_NODE=slurm-head \
        -e workers="$WORKERS" \
        -d \
        "$IMAGE_NAME"
    echo "  ✓ slurm-node-$i started"
done

echo ""
echo "Waiting for Slurm cluster to initialize..."
echo "(This takes ~30 seconds for all daemons and nodes)"
sleep 30

echo ""
echo "=========================================="
echo "Checking container status..."
echo "=========================================="
echo ""

# Check if containers are still running
ALL_RUNNING=true
for container in slurm-srun-mariadb slurm-srun-db slurm-srun-head slurm-srun-node-{1..4}; do
    if podman ps --filter "name=$container" --format "{{.Names}}" | grep -q "$container"; then
        echo "  ✓ $container is running"
    else
        echo "  ✗ $container has exited!"
        ALL_RUNNING=false
        echo ""
        echo "Last 30 lines of $container logs:"
        echo "----------------------------------------"
        podman logs "$container" 2>&1 | tail -30
        echo "----------------------------------------"
    fi
done

if [ "$ALL_RUNNING" = false ]; then
    echo ""
    echo "Some containers exited. Check logs above."
    exit 1
fi

echo ""
echo "=========================================="
echo "Verifying munge authentication..."
echo "=========================================="
echo ""

podman exec slurm-srun-head bash -c 'munge -n | unmunge'

echo ""
echo "✓ Munge working"

echo ""
echo "=========================================="
echo "Verifying Slurm cluster..."
echo "=========================================="
echo ""

echo "Checking node status with sinfo:"
podman exec slurm-srun-head sinfo || echo "  (Nodes may still be registering)"

echo ""
echo "Checking cluster status with scontrol:"
podman exec slurm-srun-head scontrol show nodes || echo "  (Still initializing)"

echo ""
echo "=========================================="
echo "Running Spindle testsuite..."
echo "=========================================="
echo ""
echo "This will take several minutes."
echo ""

# Run the testsuite
# Based on CI: docker exec slurm-srun-head bash -c 'cd Spindle-build/testsuite && salloc -n${workers} -N${workers} ./runTests ${workers}'
if podman exec slurm-srun-head bash -c "cd Spindle-build/testsuite && salloc -n${WORKERS} -N${WORKERS} ./runTests ${WORKERS}"; then
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
    echo "  podman exec -it slurm-srun-head bash"
    echo "  sinfo"
    echo "  scontrol show nodes"
    echo "  cd Spindle-build/testsuite"
    echo ""
    echo "Manual cleanup when done:"
    echo "  podman rm -f slurm-srun-mariadb slurm-srun-db slurm-srun-head slurm-srun-node-{1..4}"
    echo "  podman network rm -f slurm-srun-test-net"
    echo ""
    exit 1
fi

echo ""
echo "Manual cleanup when done:"
echo "  podman rm -f slurm-srun-mariadb slurm-srun-db slurm-srun-head slurm-srun-node-{1..4}"
echo "  podman network rm -f slurm-srun-test-net"
echo ""

# Cleanup disabled for debugging - do manually
