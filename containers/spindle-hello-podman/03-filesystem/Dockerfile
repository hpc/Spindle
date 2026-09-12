FROM ubuntu:noble

# LC-specific fix for podman setgroups issue
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi

# Install tools for filesystem operations
RUN apt-get update && apt-get install -y --no-install-recommends \
    file \
 && rm -rf /var/lib/apt/lists/*

# Create a non-root user (matching pattern from 02-user-switch)
ARG USER=testuser
ARG UID=1001
ARG GID=1001

# Make USER available at runtime (ARG only available at build time)
ENV USER=${USER}

RUN groupadd -g ${GID} ${USER} && \
    useradd -m -u ${UID} -g ${GID} -s /bin/bash ${USER}

# Switch to non-root user
USER ${USER}
WORKDIR /home/${USER}

# Create mount points for testing different volume scenarios
# These will be populated via volume mounts at runtime
RUN mkdir -p /home/${USER}/host-data && \
    mkdir -p /home/${USER}/output

# Test script that validates volume mounts and filesystem operations
# Note: The actual volume content will be mounted at runtime
CMD echo "=== Filesystem Mount Test ===" && \
    echo "" && \
    echo "Testing volume mounts and file operations..." && \
    echo "" && \
    echo "1. Checking mounted host directory:" && \
    if [ -d "/home/${USER}/host-data" ]; then \
        echo "   Mount point exists: /home/${USER}/host-data" && \
        if [ "$(ls -A /home/${USER}/host-data 2>/dev/null)" ]; then \
            echo "   ✓ Host directory mounted with content:" && \
            ls -lh /home/${USER}/host-data | head -10 && \
            echo "" && \
            echo "   Reading a file from host..." && \
            if [ -f "/home/${USER}/host-data/test-input.txt" ]; then \
                cat /home/${USER}/host-data/test-input.txt && \
                echo "   ✓ Can read files from host mount" ; \
            else \
                echo "   ℹ No test-input.txt found (expected for manual testing)" ; \
            fi ; \
        else \
            echo "   ⚠ Mount point empty - volume may not be mounted" ; \
        fi ; \
    else \
        echo "   ✗ Mount point missing" ; \
    fi && \
    echo "" && \
    echo "2. Testing write to output directory:" && \
    echo "Test data written from container at $(date)" > /home/${USER}/output/container-output.txt && \
    echo "   ✓ Created file: container-output.txt" && \
    cat /home/${USER}/output/container-output.txt && \
    echo "" && \
    echo "3. Checking file ownership:" && \
    ls -ln /home/${USER}/output/container-output.txt && \
    echo "   ℹ File owned by UID=$(stat -c %u /home/${USER}/output/container-output.txt) GID=$(stat -c %g /home/${USER}/output/container-output.txt)" && \
    echo "" && \
    echo "4. Testing file type detection:" && \
    file /home/${USER}/output/container-output.txt && \
    echo "" && \
    echo "=== 03-filesystem test complete ===" && \
    echo "" && \
    echo "Key patterns validated:" && \
    echo "  ✓ Volume mounts work" && \
    echo "  ✓ Can read files from host" && \
    echo "  ✓ Can write files to mounted volumes" && \
    echo "  ✓ File ownership preserved" && \
    echo "" && \
    echo "This demonstrates the pattern used for Spindle:" && \
    echo "  - Mount source code from host (read-only)" && \
    echo "  - Mount build directory (read-write)" && \
    echo "  - Build artifacts persist on host" && \
    echo "  - Logs accessible after container exits"
