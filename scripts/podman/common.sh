#!/bin/bash
#
# Common functions for running Spindle containers with podman on LC systems
#
# LC systems require special handling for:
# 1. SSL certificates (volume mounts)
# 2. apt setgroups errors (handled via Dockerfile ARG)
# 3. Storage configuration (enable-podman must be run)

set -e

# Ensure podman storage is configured for LC systems
# This checks for the storage.conf file that enable-podman creates
# If missing, creates it non-destructively (without killing processes)
ensure_podman_storage() {
    local STORAGE_CONF="$HOME/.config/containers/storage.conf"

    if [ -f "$STORAGE_CONF" ]; then
        return 0
    fi

    echo "=========================================="
    echo "Configuring podman storage for LC systems"
    echo "=========================================="
    echo ""
    echo "This is a one-time setup (creates ~/.config/containers/storage.conf)"
    echo ""

    mkdir -p ~/.config/containers/

    # Determine which tmpdir to use based on UID/USER length
    # (same logic as enable-podman)
    local VAR_TMPDIR="/var/tmp/$USER"
    local ALT_TMPDIR="/tmp/$USER"
    local TMP_PATH="$VAR_TMPDIR"

    if [[ $((${#UID}+${#USER})) -eq 9 ]]; then
        TMP_PATH="$ALT_TMPDIR"
    fi

    # Use overlay driver with fuse-overlayfs (same as enable-podman default)
    local DRIVER="overlay"
    local MOUNT_OPT='mount_program = "/usr/bin/fuse-overlayfs"'

    cat > "$STORAGE_CONF" << EOF
[storage]
  driver = "$DRIVER"
  runroot = "$TMP_PATH/run-$UID/containers"
  graphroot = "$TMP_PATH/config/containers/storage"
[storage.options.$DRIVER]
  ignore_chown_errors = "true"
  $MOUNT_OPT
EOF

    echo "✓ Podman storage configured"
    echo ""
    echo "Note: If you need to reset podman storage, run: enable-podman"
    echo "      (This will kill running containers and delete storage)"
    echo ""
}

# Run check on sourcing this file
ensure_podman_storage

# LC-specific SSL certificate mounts
# Build: Mount LLNL cert into ca-certificates directory
LC_CERT_BUILD_MOUNT="-v /etc/pki/ca-trust/source/anchors/PAN-cspca.llnl.gov.crt.pem:/usr/local/share/ca-certificates/cspca.crt:ro"

# Run: Mount system CA bundle
LC_CERT_RUN_MOUNT="-v /etc/pki/tls/certs/ca-bundle.trust.crt:/etc/ssl/certs/ca-certificates.crt:ro"

# Helper function to build images with LC-specific settings
podman_build() {
    local image_name=$1
    local dockerfile=$2
    local context=${3:-.}
    local extra_args="$4"  # Optional extra build args

    echo "========================================"
    echo "Building: $image_name"
    echo "Dockerfile: $dockerfile"
    echo "Context: $context"
    echo "========================================"

    podman build \
        --build-arg PODMAN_BUILD=true \
        $LC_CERT_BUILD_MOUNT \
        $extra_args \
        -t "$image_name" \
        -f "$dockerfile" \
        "$context"

    local rc=$?
    if [ $rc -eq 0 ]; then
        echo "✓ Build successful: $image_name"
    else
        echo "✗ Build failed with exit code $rc"
        return $rc
    fi
}

# Helper function to run containers with LC-specific settings
podman_run() {
    local image_name=$1
    shift

    echo "========================================"
    echo "Running: $image_name"
    echo "========================================"

    podman run \
        --rm \
        $LC_CERT_RUN_MOUNT \
        "$image_name" \
        "$@"

    local rc=$?
    if [ $rc -eq 0 ]; then
        echo "✓ Container exited successfully"
    else
        echo "✗ Container exited with code $rc"
        return $rc
    fi
}

# Helper function to clean up images
podman_cleanup() {
    local image_name=$1
    echo "Cleaning up image: $image_name"
    podman rmi "$image_name" 2>/dev/null || true
}
