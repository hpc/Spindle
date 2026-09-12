# Spindle Hello World - Podman Tutorial

This directory contains a series of progressive test containers that demonstrate and validate the patterns needed to run Spindle containers with podman on LC systems.

## Purpose

These containers serve as:
1. **Validation** - Test that podman is properly configured
2. **Tutorial** - Show common container patterns (user switching, volumes, networking)
3. **Documentation** - Demonstrate LC-specific workarounds

## Tests

### 01-basic (✓ Available)
**What it tests:**
- Container builds with apt-get (tests setgroups fix)
- Container runs successfully  
- Network connectivity works
- SSL certificates are properly configured

**Run from outside the sandbox:**
```bash
cd /path/to/workspace-Spindle/Spindle/podman-port
./scripts/podman/run-hello-01-basic.sh
```

### 02-user-switch (✓ Available)
**What it tests:**
- Creating a non-root user (uid=1001, gid=1001)
- Switching to that user with USER directive
- File permission handling (read root files, write user files)
- Sudo access (passwordless for container convenience)

**Run from outside the sandbox:**
```bash
cd /path/to/workspace-Spindle/Spindle/podman-port
./scripts/podman/run-hello-02-user.sh
```

### 03-filesystem (✓ Available)
**What it tests:**
- Volume mounts with -v flag
- Reading files from host directories
- Writing files to mounted volumes
- File persistence after container exit
- SELinux context handling (:Z flag)

**Run from outside the sandbox:**
```bash
cd /path/to/workspace-Spindle/Spindle/podman-port
./scripts/podman/run-hello-03-filesystem.sh
```

This creates temporary test directories, mounts them into the container,
and verifies files can be read/written. Artifacts persist after the
container exits, demonstrating how Spindle build/test output is preserved.

### 04-networking (✓ Available)
**What it tests:**
- Custom network creation
- Multiple containers on same network
- DNS resolution (hostname lookups)
- Container-to-container HTTP communication
- Service discovery patterns

**Run from outside the sandbox:**
```bash
cd /path/to/workspace-Spindle/Spindle/podman-port
./scripts/podman/run-hello-04-networking.sh
```

This creates a server container and two client containers, all on a custom
network. Clients can resolve the server by hostname and communicate via HTTP.
This demonstrates the pattern used for Slurm/Flux multi-node clusters.

## LC-Specific Issues Addressed

### Issue 1: setgroups errors with apt-get
**Problem:** Ubuntu/Debian apt fails with "setgroups 65534 failed" on LC systems

**Solution:** Add to Dockerfile:
```dockerfile
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi
```

This is controlled via build arg (automatically set by scripts).

### Issue 2: SSL certificate errors
**Problem:** HTTPS fails with "certificate signed by unknown authority"

**Solution:** Mount LC certificates into container:
- Build: `-v /etc/pki/ca-trust/source/anchors/PAN-cspca.llnl.gov.crt.pem:/usr/local/share/ca-certificates/cspca.crt:ro`
- Run: `-v /etc/pki/tls/certs/ca-bundle.trust.crt:/etc/ssl/certs/ca-certificates.crt:ro`

This is handled automatically by `scripts/podman/common.sh`.

## Usage

All scripts are designed to be run from **outside the sandbox** where podman is available:

```bash
# From your normal home directory or workspace
cd /path/to/workspace-Spindle/Spindle/podman-port
./scripts/podman/run-hello-01-basic.sh
```

## Next Steps

After completing these hello-world tests, proceed to:
- `scripts/podman/run-serial.sh` - Single container Spindle tests
- `scripts/podman/run-flux.sh` - Flux resource manager tests
- `scripts/podman/run-slurm-srun.sh` - Multi-container Slurm cluster tests
