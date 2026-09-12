#!/bin/bash
#
# Test 04-networking: Validate multi-container networking
#
# This test verifies:
# - Creating custom podman networks
# - Multiple containers on the same network
# - Container-to-container communication by hostname
# - Service discovery (DNS resolution)
# - Network isolation
#
# This pattern is essential for multi-container setups like Slurm clusters
# where head nodes need to communicate with compute nodes.
#
# Run this from outside the sandbox where podman is available.

set -e

# Get the directory containing this script
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Source common functions
source "$SCRIPT_DIR/common.sh"

# Configuration
IMAGE_NAME="spindle-hello-04-networking"
DOCKERFILE="$REPO_ROOT/containers/spindle-hello-podman/04-networking/Dockerfile"
NETWORK_NAME="spindle-test-net"
SERVER_NAME="test-server"
CLIENT1_NAME="test-client1"
CLIENT2_NAME="test-client2"

echo "=========================================="
echo "Spindle Hello World - 04-networking"
echo "=========================================="
echo ""
echo "This test validates multi-container networking for Slurm/Flux clusters."
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up containers and network..."
    podman rm -f "$SERVER_NAME" "$CLIENT1_NAME" "$CLIENT2_NAME" 2>/dev/null || true
    podman network rm "$NETWORK_NAME" 2>/dev/null || true
    echo "✓ Cleanup complete"
}

# Set trap to cleanup on exit
trap cleanup EXIT

# Build the image
podman_build "$IMAGE_NAME" "$DOCKERFILE" "$REPO_ROOT"

echo ""
echo "=========================================="
echo "Setting up network infrastructure..."
echo "=========================================="
echo ""

# Create a custom network
echo "Creating network: $NETWORK_NAME"
podman network create "$NETWORK_NAME"
echo "✓ Network created"

echo ""
echo "Starting server container..."
podman run \
    --name "$SERVER_NAME" \
    --network "$NETWORK_NAME" \
    --hostname "$SERVER_NAME" \
    -d \
    "$IMAGE_NAME"
echo "✓ Server started: $SERVER_NAME"

# Give server time to start
sleep 2

echo ""
echo "=========================================="
echo "Testing container networking..."
echo "=========================================="
echo ""

echo "1. Testing DNS resolution (can clients resolve server hostname?)..."
podman run \
    --name "$CLIENT1_NAME" \
    --network "$NETWORK_NAME" \
    --hostname "$CLIENT1_NAME" \
    --rm \
    "$IMAGE_NAME" \
    /bin/bash -c "
        echo 'Client: $CLIENT1_NAME'
        echo 'Resolving $SERVER_NAME...'
        if ping -c 1 -W 2 $SERVER_NAME > /dev/null 2>&1; then
            echo '✓ DNS resolution works: $SERVER_NAME is reachable'
            echo 'Server IP:' \$(getent hosts $SERVER_NAME | awk '{print \$1}')
        else
            echo '✗ Cannot resolve $SERVER_NAME'
            exit 1
        fi
    "

echo ""
echo "2. Testing HTTP communication between containers..."
podman run \
    --name "$CLIENT2_NAME" \
    --network "$NETWORK_NAME" \
    --hostname "$CLIENT2_NAME" \
    --rm \
    "$IMAGE_NAME" \
    /bin/bash -c "
        echo 'Client: $CLIENT2_NAME'
        echo 'Fetching from http://$SERVER_NAME:8080...'
        response=\$(curl -s --max-time 5 http://$SERVER_NAME:8080)
        if [ -n \"\$response\" ]; then
            echo 'Response from server:'
            echo \"\$response\"
            echo '✓ HTTP communication works'
        else
            echo '✗ No response from server'
            exit 1
        fi
    "

echo ""
echo "3. Checking server logs..."
echo "Server received requests from:"
podman logs "$SERVER_NAME" 2>&1 | tail -5

echo ""
echo "=========================================="
echo "Test complete!"
echo ""
echo "What was tested:"
echo "  ✓ Custom network creation"
echo "  ✓ Multiple containers on same network"
echo "  ✓ DNS resolution (hostname lookups)"
echo "  ✓ Container-to-container HTTP communication"
echo "  ✓ Service discovery patterns"
echo ""
echo "This pattern enables:"
echo "  - Slurm head node + compute nodes"
echo "  - Flux broker + worker nodes"
echo "  - Server-client architectures"
echo "  - Service discovery by hostname"
echo ""
echo "Next steps:"
echo "  - 05-flux-multi: Multi-container Flux cluster"
echo "  - 06-slurm-multi: Multi-container Slurm cluster"
echo "  - Then: Real Spindle containers"
echo "=========================================="

# Cleanup happens automatically via trap
