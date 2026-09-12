FROM ubuntu:noble

# LC-specific fix for podman setgroups issue
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi

# Install basic tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    sudo \
 && rm -rf /var/lib/apt/lists/*

# Create a non-root user similar to how Spindle containers work
# This pattern is used in all Spindle containers
ARG USER=testuser
ARG UID=1001
ARG GID=1001

RUN groupadd -g ${GID} ${USER} && \
    useradd -m -u ${UID} -g ${GID} -s /bin/bash ${USER} && \
    echo "${USER} ALL=(ALL) NOPASSWD: ALL" > /etc/sudoers.d/${USER} && \
    chmod 0440 /etc/sudoers.d/${USER}

# Create a test directory owned by root to verify permission handling
RUN mkdir -p /root-owned && \
    echo "This is owned by root" > /root-owned/root-file.txt && \
    chmod 644 /root-owned/root-file.txt

# Create a test directory that will be owned by the user
RUN mkdir -p /user-owned && \
    chown ${UID}:${GID} /user-owned

# Switch to non-root user
USER ${USER}
WORKDIR /home/${USER}

# Create a file as the user to verify we're running as that user
RUN echo "This is owned by ${USER}" > /home/${USER}/user-file.txt

# Test script that validates:
# 1. Running as the correct user
# 2. Can read files owned by root
# 3. Can write to user-owned directories
# 4. Cannot write to root-owned directories
# 5. Can use sudo if needed
CMD echo "=== User Switch Test ===" && \
    echo "" && \
    echo "Current user information:" && \
    id && \
    echo "" && \
    echo "Home directory:" && \
    pwd && \
    ls -la /home/${USER} && \
    echo "" && \
    echo "Testing file permissions:" && \
    echo "  Reading root-owned file..." && \
    cat /root-owned/root-file.txt && \
    echo "  ✓ Can read root-owned files" && \
    echo "" && \
    echo "  Writing to user-owned directory..." && \
    echo "test content" > /user-owned/test.txt && \
    cat /user-owned/test.txt && \
    echo "  ✓ Can write to user-owned directories" && \
    echo "" && \
    echo "  Testing root-owned directory (should fail)..." && \
    (echo "test" > /root-owned/fail.txt 2>&1 && echo "  ✗ UNEXPECTED: Could write to root directory" || echo "  ✓ Correctly denied write to root directory") && \
    echo "" && \
    echo "  Testing sudo access..." && \
    sudo -n echo "  ✓ Sudo works (passwordless for container convenience)" && \
    echo "" && \
    echo "=== 02-user-switch test complete ===" && \
    echo "" && \
    echo "Key patterns validated:" && \
    echo "  ✓ Non-root user created (uid=${UID})" && \
    echo "  ✓ User has home directory" && \
    echo "  ✓ Correct permission handling" && \
    echo "  ✓ Sudo available when needed"
