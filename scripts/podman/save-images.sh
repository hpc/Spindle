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

# Save all spindle images
podman save \
    localhost/spindle-slurm-base:latest \
    localhost/spindle-slurm-srun:latest \
    localhost/spindle-serial-ubuntu:latest \
    -o "$OUTPUT_FILE"

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
