#!/bin/bash
#
# Flux multi-node entrypoint
# Based on Spindle's Flux container setup
#
# All nodes run the same image but behave differently based on hostname.
# flux-node-1 is the head node, others are workers.

set -e

echo "=== Flux Container Startup ==="
echo "Hostname: $(hostname)"
echo "IP: $(hostname -I)"

# Start munge for authentication
echo "Starting munge..."
sudo /usr/sbin/munged
sleep 1

# Determine if this is the head node or a worker
MAIN_HOST="flux-node-1"
THIS_HOST=$(hostname)

brokerOptions="-Stbon.fanout=256 \
  -Srundir=/run/flux \
  -Sstatedir=${STATE_DIR} \
  -Slog-stderr-level=6 \
  -Slog-stderr-mode=local"

if [ "${THIS_HOST}" != "${MAIN_HOST}" ]; then
    # Worker node - wait for head node to be ready
    echo "Worker node: connecting to ${MAIN_HOST}..."
    sleep 5

    # Start flux broker and connect to head node
    flux start -o --config /etc/flux/config ${brokerOptions} sleep inf
else
    # Head node
    echo "Head node: starting Flux broker..."

    # Start flux broker
    flux start -o --config /etc/flux/config ${brokerOptions} bash -c '
        echo ""
        echo "=== Flux Instance Started ==="
        echo ""

        # Wait for workers to connect
        echo "Waiting for workers to connect..."
        sleep 10

        echo ""
        echo "Flux instance status:"
        flux resource list

        echo ""
        echo "Running test job across cluster..."
        flux run -N 3 hostname

        echo ""
        echo "=== Flux cluster is operational ==="
        echo "You can now run flux commands."
        echo ""

        # Keep running
        sleep inf
    '
fi
