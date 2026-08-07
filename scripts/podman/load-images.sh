#!/bin/bash
#
# Load Spindle podman images from tarball
#
# Run this on compute nodes to load images saved with save-images.sh

set -e

TARBALL="${1}"

if [ -z "$TARBALL" ]; then
    echo "Usage: $0 <tarball-file>"
    echo ""
    echo "Example:"
    echo "  $0 /g/g24/rountree/v/rzadams/sandbox/spindle-podman-images.tar"
    exit 1
fi

if [ ! -f "$TARBALL" ]; then
    echo "Error: Tarball not found: $TARBALL"
    exit 1
fi

echo "=========================================="
echo "Loading Spindle Podman Images"
echo "=========================================="
echo ""
echo "Source: $TARBALL"
echo "This may take a minute..."
echo ""

podman load -i "$TARBALL"

echo ""
echo "=========================================="
echo "✓ Images loaded successfully"
echo "=========================================="
echo ""
echo "Available Spindle images:"
podman images | grep spindle || echo "  (none - something went wrong)"
echo ""
