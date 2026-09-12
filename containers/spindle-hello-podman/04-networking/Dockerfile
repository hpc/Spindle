FROM ubuntu:noble

# LC-specific fix for podman setgroups issue
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi

# Install networking tools
RUN apt-get update && apt-get install -y --no-install-recommends \
    iputils-ping \
    iproute2 \
    curl \
    netcat-openbsd \
 && rm -rf /var/lib/apt/lists/*

# Create a non-root user
ARG USER=testuser
ARG UID=1001
ARG GID=1001

# Make USER available at runtime
ENV USER=${USER}

RUN groupadd -g ${GID} ${USER} && \
    useradd -m -u ${UID} -g ${GID} -s /bin/bash ${USER}

# Switch to non-root user
USER ${USER}
WORKDIR /home/${USER}

# Default command: run as server listening on port 8080
# This can be overridden when running as a client
CMD echo "=== Network Container ===" && \
    echo "Container: $(hostname)" && \
    echo "IP Address: $(hostname -I)" && \
    echo "" && \
    echo "Starting simple HTTP server on port 8080..." && \
    while true; do \
        echo "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nHello from $(hostname) at $(date)" | nc -l -p 8080 -q 1; \
    done
