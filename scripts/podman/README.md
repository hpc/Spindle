# Spindle Podman Scripts

Scripts for running Spindle containers locally with podman on LC systems.

## Prerequisites

- Podman installed and working
- **LC systems: Run `enable-podman` before using these scripts**
- Subuid/subgid configured for your user

## Quick Start

### Hello World Tests (verify environment)

```bash
./run-hello-01-basic.sh      # Basic container execution
./run-hello-02-user.sh        # User switching
./run-hello-03-filesystem.sh  # Volume mounts
./run-hello-04-networking.sh  # Multi-container networking
./run-hello-05-flux.sh        # Flux cluster
./run-hello-06-slurm.sh       # Slurm cluster
```

### Spindle Tests

```bash
# Build images
./build-spindle-serial.sh
./build-spindle-slurm-base.sh && ./build-spindle-slurm-srun.sh
./build-spindle-flux.sh

# Run tests
./test-spindle-serial.sh
./test-spindle-slurm-srun.sh
./test-spindle-flux.sh

# Debug variants (keeps containers running)
./test-spindle-serial-debug.sh

# Crash tests
./test-spindle-serial-crash.sh
```

### Parallel Testing on Compute Nodes

```bash
# On login node: save images to tarball
./save-images.sh

# On compute node: load images
./load-images.sh /path/to/spindle-podman-images.tar

# Run multiple tests in parallel
for i in $(seq 1 10); do 
  ./test-spindle-slurm-srun-parallel.sh $i > out.$i 2>&1 &
done
wait

# Clean up
./cleanup.sh
```

## Script Reference

### Infrastructure

- **common.sh** - Shared functions and LC-specific configurations
  - SSL certificate mounts for LC systems
  - `podman_build()` and `podman_run()` wrappers with PODMAN_BUILD arg
  - Handles setgroups workaround

- **cleanup.sh** - Remove all or specific test containers/networks
  - Usage: `./cleanup.sh` (all) or `./cleanup.sh <run-id>` (specific)
  - Parallel cleanup for speed

- **save-images.sh** - Save Spindle images to tarball for compute node deployment
  - Creates `spindle-podman-images.tar` in repo root
  - Bundles `mariadb.env` for portable Slurm testing

- **load-images.sh** - Load images from tarball on compute nodes
  - Usage: `./load-images.sh <tarball-path>`
  - Extracts and loads each image separately

### Hello World Tests

- **run-hello-01-basic.sh** - Basic execution (validates environment)
- **run-hello-02-user.sh** - User switching (validates non-root patterns)
- **run-hello-03-filesystem.sh** - Volume mounts (validates host/container filesystem)
- **run-hello-04-networking.sh** - Multi-container networking (validates cluster patterns)
- **run-hello-05-flux.sh** - Flux cluster (validates Flux setup)
- **run-hello-06-slurm.sh** - Slurm cluster (validates Slurm setup)

### Build Scripts

- **build-spindle-serial.sh** - Build serial Spindle container
- **build-spindle-slurm-base.sh** - Build Slurm base image (Slurm + MPICH from source, ~6 min)
- **build-spindle-slurm-srun.sh** - Build Slurm srun test layer (requires base)
- **build-spindle-slurm-rshlaunch.sh** - Build Slurm rshlaunch test layer (BLOCKED: SSH issues)
- **build-spindle-flux.sh** - Build Flux Spindle container (~7 min)

### Test Scripts

- **test-spindle-serial.sh** - Serial launcher tests
- **test-spindle-serial-debug.sh** - Serial tests with containers kept running for debugging
- **test-spindle-serial-crash.sh** - Serial crash dump tests

- **test-spindle-slurm-srun.sh** - Slurm srun tests (7 containers: MariaDB, slurmdbd, slurmctld, 4 workers)
- **test-spindle-slurm-srun-parallel.sh** - Parallel-safe variant with unique container names
  - Usage: `./test-spindle-slurm-srun-parallel.sh <run-id>`
  - Enables multiple concurrent test runs without conflicts

- **test-spindle-slurm-rshlaunch.sh** - Slurm rshlaunch tests (BLOCKED: SSH setgroups issue)

- **test-spindle-flux.sh** - Flux launcher tests (PARTIALLY WORKING: cluster starts but Spindle tests fail)

## Deployment Workflow

### Local Development (Login Node)

1. Build images: `./build-spindle-*.sh`
2. Test locally: `./test-spindle-*.sh`
3. Iterate and debug

### Compute Node Deployment

1. **On login node:**
   ```bash
   ./save-images.sh
   # Creates spindle-podman-images.tar + mariadb.env
   ```

2. **On compute node:**
   ```bash
   enable-podman  # REQUIRED on LC systems
   ./load-images.sh /path/to/spindle-podman-images.tar
   ```

3. **Run parallel tests:**
   ```bash
   for i in $(seq 1 50); do 
     ./test-spindle-slurm-srun-parallel.sh $i > out.$i 2>&1 &
   done
   wait
   
   # Check results
   grep -l "ALL TESTS PASSED" out.*
   grep -l "SOME TESTS FAILED" out.*
   ```

4. **Cleanup:**
   ```bash
   ./cleanup.sh
   ```

## Known Issues

### On LC Systems: Must Run enable-podman First

LC systems require running `enable-podman` before using podman. This configures storage and clears any stale state. If you see "Access denied" or image loading issues, run `enable-podman` and retry.

### Slurm Tests Require mariadb.env

Slurm tests need the MariaDB password from `mariadb.env`. The `save-images.sh` script copies this to the repo root for portability. If you get "Access denied for user 'slurm'@...", check that `mariadb.env` exists in the repo root.

### Parallel Test Limits

Rootless podman has resource limits. On compute nodes, 50-100 parallel tests work well. Beyond that, you may hit:
- File descriptor limits
- Network namespace limits
- Memory pressure

### Flux Tests Partially Working

Flux cluster starts correctly, but Spindle tests fail with "spindleRunBE failed!". Needs debugging. See PODMAN.md for details.

### Slurm rshlaunch Blocked

SSH privilege separation calls `setgroups()`, which fails in rootless podman on LC systems. Tried `UsePrivilegeSeparation=no` but still fails. May need sshd rebuild or alternative SSH implementation.

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

### Image IDs All the Same After Load

If `podman images | grep spindle` shows the same image ID for all tags, the tarball wasn't created correctly. Re-run `save-images.sh` which saves each image separately then combines them.

### Containers Exit Immediately

Check that you ran `enable-podman` before loading images. Also verify images loaded correctly with `podman images | grep spindle` - each should have a different ID.

## LC System Documentation

For more details on podman on LC systems:
https://hpc.llnl.gov/documentation/user-guides/using-containers-lc-hpc-systems/containers-how-build-container
