#!/bin/bash
#
# Run Spindle Slurm srun test with unique container names for parallel execution
#
# Usage: test-spindle-slurm-srun-parallel.sh <run-id>
#
# The run-id is appended to all container and network names to avoid conflicts
# when running multiple tests in parallel.

set -e

RUN_ID="${1}"

if [ -z "$RUN_ID" ]; then
    echo "Usage: $0 <run-id>"
    echo ""
    echo "Example: $0 42"
    echo "  Creates containers: slurm-srun-42-mariadb, slurm-srun-42-head, etc."
    exit 1
fi

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration - all names include RUN_ID for uniqueness
IMAGE_NAME="spindle-slurm-srun"
NETWORK_NAME="slurm-srun-${RUN_ID}-net"
WORKERS=4
NAME_PREFIX="slurm-srun-${RUN_ID}"

echo "=========================================="
echo "Spindle Slurm Srun Tests (Run ID: $RUN_ID)"
echo "=========================================="
echo ""
echo "Container prefix: $NAME_PREFIX"
echo "Network: $NETWORK_NAME"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up run $RUN_ID..."
    # Stop all containers in parallel for faster cleanup
    for container in ${NAME_PREFIX}-mariadb ${NAME_PREFIX}-db ${NAME_PREFIX}-head ${NAME_PREFIX}-node-{1..4}; do
        (podman stop "$container" 2>/dev/null || true) &
    done
    wait
    # Remove all containers in parallel
    for container in ${NAME_PREFIX}-mariadb ${NAME_PREFIX}-db ${NAME_PREFIX}-head ${NAME_PREFIX}-node-{1..4}; do
        (podman rm -f "$container" 2>/dev/null || true) &
    done
    wait
    podman network rm -f "$NETWORK_NAME" 2>/dev/null || true
    echo "✓ Cleanup complete for run $RUN_ID"
}

# Set trap to cleanup on exit
trap cleanup EXIT

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
# Read password from mariadb.env
# First check repo root (portable deployment), then fall back to source location
MARIADB_ENV=""
if [ -f "$REPO_ROOT/mariadb.env" ]; then
    MARIADB_ENV="$REPO_ROOT/mariadb.env"
elif [ -f "$REPO_ROOT/containers/spindle-slurm-ubuntu/testing-srun/mariadb.env" ]; then
    MARIADB_ENV="$REPO_ROOT/containers/spindle-slurm-ubuntu/testing-srun/mariadb.env"
else
    echo "Error: Could not find mariadb.env"
    echo "  Looked in:"
    echo "    $REPO_ROOT/mariadb.env"
    echo "    $REPO_ROOT/containers/spindle-slurm-ubuntu/testing-srun/mariadb.env"
    exit 1
fi

MARIADB_PASSWORD=$(grep MARIADB_PASSWORD "$MARIADB_ENV" | cut -d'"' -f2)
if [ -z "$MARIADB_PASSWORD" ]; then
    echo "Error: Could not read password from $MARIADB_ENV"
    exit 1
fi
echo "  Using password from: $MARIADB_ENV"
podman run \
    --name "${NAME_PREFIX}-mariadb" \
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
    --name "${NAME_PREFIX}-db" \
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
    --name "${NAME_PREFIX}-head" \
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
        --name "${NAME_PREFIX}-node-$i" \
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
echo "(This takes ~60 seconds for all daemons and nodes)"
sleep 60

echo ""
echo "=========================================="
echo "Checking container status..."
echo "=========================================="
echo ""

# Check if containers are still running
ALL_RUNNING=true
for container in ${NAME_PREFIX}-mariadb ${NAME_PREFIX}-db ${NAME_PREFIX}-head ${NAME_PREFIX}-node-{1..4}; do
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

podman exec "${NAME_PREFIX}-head" bash -c 'munge -n | unmunge'

echo ""
echo "✓ Munge working"

echo ""
echo "=========================================="
echo "Verifying Slurm cluster..."
echo "=========================================="
echo ""

echo "Checking node status with sinfo:"
if podman exec "${NAME_PREFIX}-head" sinfo; then
    echo "  ✓ Slurm cluster ready"
else
    echo ""
    echo "✗ Slurm cluster verification FAILED"
    echo ""
    echo "Containers are still running for debugging."
    echo "Press ENTER to cleanup and exit, or Ctrl-C to keep them running."
    echo ""
    echo "Useful debug commands:"
    echo "  podman logs ${NAME_PREFIX}-head"
    echo "  podman logs ${NAME_PREFIX}-db"
    echo "  podman exec ${NAME_PREFIX}-head sinfo"
    echo "  podman exec ${NAME_PREFIX}-head scontrol show nodes"
    echo ""
    read -r
    exit 1
fi

echo ""
echo "=========================================="
echo "Running Spindle testsuite..."
echo "=========================================="
echo ""
echo "This will take several minutes."
echo ""

# Run the testsuite
if podman exec "${NAME_PREFIX}-head" bash -c "cd Spindle-build/testsuite && salloc -n${WORKERS} -N${WORKERS} ./runTests ${WORKERS}"; then
    echo ""
    echo "=========================================="
    echo "✓ All tests passed! (Run $RUN_ID)"
    echo "=========================================="
    echo ""
    exit 0
else
    echo ""
    echo "=========================================="
    echo "✗ Some tests failed (Run $RUN_ID)"
    echo "=========================================="
    echo ""
    exit 1
fi

# Cleanup happens automatically via trap
