#!/bin/bash
#
# Save Spindle podman images to tarball for compute node deployment
#
# Run this on the login node where images were built.
# The tarball can then be loaded on compute nodes via load-images.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

OUTPUT_FILE="${1:-$REPO_ROOT/spindle-podman-images.tar}"

echo "=========================================="
echo "Saving Spindle Podman Images"
echo "=========================================="
echo ""
echo "This saves all Spindle images to a tarball for deployment to compute nodes."
echo "Output: $OUTPUT_FILE"
echo ""

# Check what images exist
echo "Available Spindle images:"
podman images | grep spindle || {
    echo "Error: No Spindle images found. Build them first."
    exit 1
}

echo ""
echo "Saving images to tarball..."
echo "This may take several minutes..."
echo ""

# Save images separately to avoid parent-child ID collisions
# When multiple images share a base, podman save can collapse them incorrectly
TEMP_DIR=$(mktemp -d)
trap "rm -rf $TEMP_DIR" EXIT

echo "Saving spindle-slurm-base..."
podman save localhost/spindle-slurm-base:latest -o "$TEMP_DIR/base.tar"

echo "Saving spindle-slurm-srun..."
podman save localhost/spindle-slurm-srun:latest -o "$TEMP_DIR/srun.tar"

echo "Saving spindle-serial-ubuntu..."
podman save localhost/spindle-serial-ubuntu:latest -o "$TEMP_DIR/serial.tar"

echo "Combining into single tarball..."
tar -cf "$OUTPUT_FILE" -C "$TEMP_DIR" base.tar srun.tar serial.tar

# Copy mariadb.env for portable deployment
MARIADB_ENV_SOURCE="$REPO_ROOT/containers/spindle-slurm-ubuntu/testing-srun/mariadb.env"
MARIADB_ENV_DEST="$REPO_ROOT/mariadb.env"
if [ -f "$MARIADB_ENV_SOURCE" ]; then
    echo "Copying mariadb.env for portable deployment..."
    cp "$MARIADB_ENV_SOURCE" "$MARIADB_ENV_DEST"
    echo "  ✓ mariadb.env copied to repo root"
fi

SIZE=$(du -h "$OUTPUT_FILE" | cut -f1)
echo ""
echo "=========================================="
echo "✓ Images saved successfully"
echo "=========================================="
echo ""
echo "File: $OUTPUT_FILE"
echo "Size: $SIZE"
echo ""
echo "To load on compute nodes:"
echo "  ./scripts/podman/load-images.sh $OUTPUT_FILE"
echo ""
