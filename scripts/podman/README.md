# Spindle Podman Scripts

Scripts for running Spindle containers locally with podman on LC systems.

## Prerequisites

- Podman installed and working
- Subuid/subgid configured for your user
- Run from **outside the sandbox** (podman needs proper user namespaces)

## Quick Start

```bash
# Test podman environment
./run-hello-01-basic.sh

# Run serial tests (after hello-world passes)
# ./run-serial.sh

# Run flux tests
# ./run-flux.sh

# Run slurm cluster tests
# ./run-slurm-srun.sh
```

## Files

- **common.sh** - Shared functions and LC-specific configurations
  - Handles SSL certificate mounts
  - Sets PODMAN_BUILD=true for setgroups fix
  - Provides `podman_build()` and `podman_run()` wrappers

- **run-hello-01-basic.sh** - Hello world test (validates environment)
- **run-hello-02-user.sh** - User switching test (validates non-root patterns)
- **run-hello-03-filesystem.sh** - Volume mount test (TODO)
- **run-hello-04-networking.sh** - Multi-container networking test (TODO)

- **run-serial.sh** - Serial Spindle tests (TODO)
- **run-flux.sh** - Flux Spindle tests (TODO)
- **run-slurm-srun.sh** - Slurm cluster tests (TODO)

## Running from Outside Sandbox

These scripts must be run from outside the sandbox where podman has proper access:

```bash
# Navigate to the podman-port directory
cd /path/to/workspace-Spindle/Spindle/podman-port

# Run any script
./scripts/podman/run-hello-01-basic.sh
```

The scripts will automatically:
- Find the correct paths (Dockerfiles, build context)
- Apply LC-specific workarounds (certificates, setgroups)
- Build and run containers
- Report success/failure

## Troubleshooting

### "newuidmap failed: Operation not permitted"
Your user doesn't have subuid/subgid mappings. Check:
```bash
grep $(whoami) /etc/subuid /etc/subgid
```

If empty, contact your sysadmin to add entries.

### "certificate signed by unknown authority" 
The SSL certificate mounts may be incorrect for your system. Check if these files exist:
```bash
ls -l /etc/pki/ca-trust/source/anchors/PAN-cspca.llnl.gov.crt.pem
ls -l /etc/pki/tls/certs/ca-bundle.trust.crt
```

### "setgroups 65534 failed"
The Dockerfile needs the apt sandbox fix. Verify the Dockerfile has:
```dockerfile
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi
```

And you're using `podman_build()` from common.sh (not raw `podman build`).

## LC System Documentation

For more details on podman on LC systems:
https://hpc.llnl.gov/documentation/user-guides/using-containers-lc-hpc-systems/containers-how-build-container
