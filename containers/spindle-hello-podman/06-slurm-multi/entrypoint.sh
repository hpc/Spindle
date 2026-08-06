#!/bin/bash
#
# Slurm multi-node entrypoint
# Simplified version for hello-world demonstration
# Based on Spindle's working entrypoint
#
# Determines role based on SLURM_ROLE environment variable:
#   - ctl: runs slurmctld (head node)
#   - worker: runs slurmd (compute node)

set -e

echo "=== Slurm Container Startup ==="
echo "Hostname: $(hostname)"
echo "Role: ${SLURM_ROLE}"

# Start munge for authentication
echo "Starting munge..."
sudo -u munge /usr/sbin/munged
sleep 2

case "${SLURM_ROLE}" in
    ctl)
        echo "Starting slurmctld (controller daemon)..."
        # Run as slurm user in foreground with high verbosity
        sudo -u slurm /usr/sbin/slurmctld -i -Dvvv
        ;;

    worker)
        echo "Waiting for controller to be ready..."
        sleep 5

        echo "Testing connectivity to controller..."
        ping -c 3 slurm-head || echo "WARNING: Cannot ping slurm-head"

        echo "Testing munge authentication..."
        munge -n | unmunge || echo "WARNING: Munge test failed"

        echo "Starting slurmd (compute daemon)..."
        # Run as root (via sudo) in foreground - slurmd needs root for cgroup management
        # Based on Spindle's pattern: sudo bash -c 'exec /usr/sbin/slurmd -Dvvv'
        sudo /usr/sbin/slurmd -Dvvv
        ;;

    *)
        echo "ERROR: SLURM_ROLE must be 'ctl' or 'worker'"
        echo "Got: ${SLURM_ROLE}"
        exit 1
        ;;
esac
