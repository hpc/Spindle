#!/bin/bash
#
# Bulletproof workflow for testing podman changes
#
# This script runs ON THE LOGIN NODE and guides you through the process
#
# Run from anywhere in the repo - it will find the right paths

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

BRANCH=$(git -C "$REPO_ROOT" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")

echo "=========================================="
echo "Podman Testing Workflow"
echo "=========================================="
echo ""
echo "Branch: $BRANCH"
echo "Repo root: $REPO_ROOT"
echo ""
echo "This script runs on the LOGIN NODE"
echo "It will rebuild, save, and give you commands for the compute node"
echo ""

cd "$REPO_ROOT"

TARBALL="$REPO_ROOT/spindle-podman-images.tar"

# Check if base image exists
if ! podman images | grep -q "spindle-slurm-base"; then
    echo "Base image not found in podman."
    echo ""

    # Check if tarball exists
    if [ -f "$TARBALL" ]; then
        echo "Found tarball: $TARBALL"
        echo "Loading images from tarball..."
        echo "Command: ./scripts/podman/load-images.sh $TARBALL"
        echo ""
        ./scripts/podman/load-images.sh "$TARBALL" 2>&1 | ts
        echo ""

        # Check again if image is now available
        if podman images | grep -q "spindle-slurm-base"; then
            echo "✓ Base image loaded from tarball"
            echo ""
        else
            echo "ERROR: Base image not in tarball. Need to rebuild."
            echo ""
            echo "Building spindle-slurm-base from source..."
            echo "Command: ./scripts/podman/build-spindle-slurm-base.sh"
            echo "(This takes ~6 minutes - compiles Slurm + MPICH from source)"
            echo ""
            ./scripts/podman/build-spindle-slurm-base.sh 2>&1 | ts
            echo ""
            echo "✓ Base image built"
            echo ""
        fi
    else
        echo "Tarball not found at: $TARBALL"
        echo "Building spindle-slurm-base from source..."
        echo "Command: ./scripts/podman/build-spindle-slurm-base.sh"
        echo "(This takes ~6 minutes - compiles Slurm + MPICH from source)"
        echo ""
        ./scripts/podman/build-spindle-slurm-base.sh 2>&1 | ts
        echo ""
        echo "✓ Base image built"
        echo ""
    fi
else
    echo "✓ Base image already loaded"
    echo ""
fi

echo "Step 1: Rebuild slurm-srun image"
echo "Command: ./scripts/podman/build-spindle-slurm-srun.sh"
./scripts/podman/build-spindle-slurm-srun.sh 2>&1 | ts

echo ""
echo "✓ Build complete"
echo ""

echo "Step 2: Save images to tarball"
echo "Command: ./scripts/podman/save-images.sh"
./scripts/podman/save-images.sh 2>&1 | ts

TARBALL_PATH="$REPO_ROOT/spindle-podman-images.tar"
echo ""
echo "✓ Images saved to: $TARBALL_PATH"
echo ""

echo "=========================================="
echo "NOW SWITCH TO COMPUTE NODE"
echo "=========================================="
echo ""
echo "Run these commands on the compute node:"
echo ""
echo "  # Get allocation (if needed)"
echo "  salloc -N1 -t 60"
echo ""
echo "  # Setup podman"
echo "  enable-podman"
echo ""
echo "  # Navigate to repo"
echo "  cd $REPO_ROOT"
echo ""
echo "  # Load images"
echo "  ./scripts/podman/load-images.sh ./spindle-podman-images.tar"
echo ""
echo "  # Verify canary - should show 'ENTRYPOINT DEBUG (v2)'"
echo "  podman run --rm -e SLURM_ROLE=db -e MARIADB_PASSWORD=test123 localhost/spindle-slurm-srun:latest 2>&1 | head -30"
echo ""
echo "  # Run test"
echo "  ./scripts/podman/test-spindle-slurm-srun.sh 1 2>&1 | tee test-output.log"
echo ""
echo "=========================================="
echo ""
echo "Copy the commands above to your compute node terminal"
echo ""
