#!/bin/bash
#
# Clean up Spindle test containers and networks
#
# Usage: cleanup.sh [run-id]
#
# With no arguments: removes ALL slurm-srun-* containers and networks
# With run-id: removes only containers/networks for that specific run

set -e

RUN_ID="${1}"

if [ -n "$RUN_ID" ]; then
    echo "=========================================="
    echo "Cleaning up run $RUN_ID..."
    echo "=========================================="
    echo ""

    NAME_PREFIX="slurm-srun-${RUN_ID}"
    NETWORK_NAME="slurm-srun-${RUN_ID}-net"

    # Stop and remove containers for this run
    for container in ${NAME_PREFIX}-mariadb ${NAME_PREFIX}-db ${NAME_PREFIX}-head ${NAME_PREFIX}-node-{1..4}; do
        if podman ps -a --format "{{.Names}}" | grep -q "^${container}$"; then
            echo "Removing $container..."
            podman stop "$container" 2>/dev/null || true
            podman rm -f "$container" 2>/dev/null || true
        fi
    done

    # Remove network
    if podman network exists "$NETWORK_NAME" 2>/dev/null; then
        echo "Removing network $NETWORK_NAME..."
        podman network rm -f "$NETWORK_NAME" 2>/dev/null || true
    fi

    echo "✓ Cleanup complete for run $RUN_ID"
else
    echo "=========================================="
    echo "Cleaning up ALL Spindle test containers"
    echo "=========================================="
    echo ""

    # Find all slurm-srun containers
    CONTAINERS=$(podman ps -a --format "{{.Names}}" | grep "^slurm-srun-" || true)

    if [ -n "$CONTAINERS" ]; then
        COUNT=$(echo "$CONTAINERS" | wc -l)
        echo "Found $COUNT containers to remove"
        echo ""

        # Skip stop, just force-remove (stops and removes in one step)
        # Use timeout and lower parallelism to avoid hangs
        echo "$CONTAINERS" | while read container; do
            echo "Removing $container..."
            timeout 10 podman rm -f "$container" 2>/dev/null || echo "  (timed out, skipping)" &
        done
        wait
    else
        echo "No slurm-srun containers found"
    fi

    echo ""

    # Find all slurm-srun networks
    NETWORKS=$(podman network ls --format "{{.Name}}" | grep "^slurm-srun-" || true)

    if [ -n "$NETWORKS" ]; then
        COUNT=$(echo "$NETWORKS" | wc -l)
        echo "Found $COUNT networks to remove"
        echo ""

        # Remove networks sequentially with timeout to avoid hangs
        echo "$NETWORKS" | while read network; do
            echo "Removing $network..."
            timeout 5 podman network rm -f "$network" 2>/dev/null || echo "  (timed out or in use, skipping)"
        done
    else
        echo "No slurm-srun networks found"
    fi

    echo ""
    echo "✓ Cleanup complete"
fi
