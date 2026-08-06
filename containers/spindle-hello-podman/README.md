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

### 02-user-switch (Coming next)
**What it will test:**
- Creating a non-root user
- Switching to that user
- File permissions

### 03-filesystem (Coming next)
**What it will test:**
- Mounting host directories
- Writing files from container
- File ownership and permissions

### 04-networking (Coming next)
**What it will test:**
- Creating podman networks
- Multiple containers communicating
- Service discovery

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
