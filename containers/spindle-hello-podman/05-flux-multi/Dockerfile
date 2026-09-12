# Simplified Flux multi-node example
# Based on https://flux-framework.readthedocs.io/en/latest/tutorials/containers
# Uses the official Flux Framework base image

ARG flux_sched_version=noble-v0.48.0-amd64
FROM fluxrm/flux-sched:${flux_sched_version}

USER root

# LC-specific fix for podman setgroups issue
ARG PODMAN_BUILD=false
RUN if [ "$PODMAN_BUILD" = "true" ]; then \
      echo 'APT::Sandbox::User root;' > /etc/apt/apt.conf.d/00-apt-sandbox; \
    fi

# Install basic tools for testing
RUN apt-get update && apt-get install -y --no-install-recommends \
    iputils-ping \
    iproute2 \
 && rm -rf /var/lib/apt/lists/*

# Configure Flux for multi-node cluster
ARG workers=3
ENV workers=${workers}
ENV STATE_DIR=/var/lib/flux

# Set up Flux directories and configuration
RUN mkdir -p ${STATE_DIR} /etc/flux/system /etc/flux/config /run/flux /mnt/curve && \
    flux keygen /mnt/curve/curve.cert && \
    flux R encode --hosts="flux-node-[1-${workers}]" > /etc/flux/system/R

# Create broker configuration
RUN echo '[bootstrap]' > /etc/flux/config/broker.toml && \
    echo 'default_port = 8050' >> /etc/flux/config/broker.toml && \
    echo 'default_bind = "tcp://eth0:%p"' >> /etc/flux/config/broker.toml && \
    echo 'default_connect = "tcp://%h.local:%p"' >> /etc/flux/config/broker.toml && \
    echo 'hosts = [' >> /etc/flux/config/broker.toml && \
    echo '    { host = "flux-node-1" },' >> /etc/flux/config/broker.toml && \
    echo '    { host = "flux-node-[2-'${workers}']" },' >> /etc/flux/config/broker.toml && \
    echo ']' >> /etc/flux/config/broker.toml

# Set proper ownership
ARG USER=fluxuser
RUN chown -R ${USER}:${USER} /run/flux ${STATE_DIR} /mnt/curve /etc/flux

# Allow fluxuser to run munge
RUN sh -c "printf \"${USER} ALL=(ALL) NOPASSWD: /usr/sbin/munged\\n\" >> /etc/sudoers"

# Copy entrypoint as root, make executable, then set ownership
COPY containers/spindle-hello-podman/05-flux-multi/entrypoint.sh /home/${USER}/entrypoint.sh
RUN chmod +x /home/${USER}/entrypoint.sh && \
    chown ${USER}:${USER} /home/${USER}/entrypoint.sh

USER ${USER}
WORKDIR /home/${USER}

ENV HWLOC_HIDE_ERRORS=2

ENTRYPOINT ["./entrypoint.sh"]
