FROM ubuntu:noble

# LC-specific fix for podman setgroups issue
# This is a no-op in Docker, but required for podman on LC systems
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi

# Install curl to test both apt-get and SSL connectivity
RUN apt-get update && apt-get install -y --no-install-recommends \
    curl \
    ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Test script that validates:
# 1. Container runs
# 2. Network access works
# 3. SSL certificates work
CMD echo "=== Hello from Podman! ===" && \
    echo "Container is running successfully." && \
    echo "" && \
    echo "Testing network and SSL connectivity..." && \
    curl -I https://www.google.com 2>&1 | head -5 && \
    echo "" && \
    echo "If you see HTTP headers above, SSL is working!" && \
    echo "" && \
    echo "=== 01-basic test complete ==="
